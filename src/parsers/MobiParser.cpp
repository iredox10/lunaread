#include "MobiParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <QRegularExpression>

namespace LunaRead {

bool MobiParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "mobi" || ext == "azw" || ext == "azw3" || ext == "prc";
}

std::optional<BookDocument> MobiParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "mobi", filePath);
}

QByteArray MobiParser::decompressPalmDocLz77(const QByteArray& src) {
    QByteArray dst;
    int i = 0;
    const int len = src.size();

    while (i < len) {
        uint8_t byte = static_cast<uint8_t>(src[i++]);
        if (byte == 0x00) {
            dst.append(char(0x00));
        } else if (byte >= 0x01 && byte <= 0x08) {
            int count = byte;
            while (count > 0 && i < len) {
                dst.append(src[i++]);
                count--;
            }
        } else if (byte >= 0x09 && byte <= 0x7F) {
            dst.append(static_cast<char>(byte));
        } else if (byte >= 0x80 && byte <= 0xBF) {
            if (i >= len) break;
            uint8_t next = static_cast<uint8_t>(src[i++]);
            int distance = ((byte & 0x3F) << 5) | ((next & 0xF8) >> 3);
            int length = (next & 0x07) + 3;

            int copyStart = dst.size() - distance;
            if (copyStart >= 0) {
                for (int k = 0; k < length; ++k) {
                    if (copyStart + k < dst.size()) {
                        dst.append(dst[copyStart + k]);
                    }
                }
            }
        } else {
            // 0xC0..0xFF: Space followed by character
            dst.append(' ');
            dst.append(static_cast<char>(byte ^ 0x80));
        }
    }
    return dst;
}

std::optional<BookDocument> MobiParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    Q_UNUSED(format);
    if (data.size() < 78) return std::nullopt;

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "Kindle Book";
    QString docId = HashUtils::calculateBytesMd5(data);

    // Read PDB record count
    uint16_t numRecords = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(data.constData() + 76));
    if (numRecords == 0 || data.size() < 78 + (numRecords * 8)) {
        return std::nullopt;
    }

    QVector<uint32_t> recordOffsets;
    for (int i = 0; i < numRecords; ++i) {
        uint32_t offset = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(data.constData() + 78 + (i * 8)));
        recordOffsets.append(offset);
    }

    // Read Record 0 for PalmDOC and MOBI header
    uint32_t rec0Offset = recordOffsets[0];
    uint32_t rec0End = numRecords > 1 ? recordOffsets[1] : data.size();
    if (rec0End > static_cast<uint32_t>(data.size()) || rec0Offset >= rec0End) return std::nullopt;

    QByteArray rec0 = data.mid(rec0Offset, rec0End - rec0Offset);
    if (rec0.size() < 16) return std::nullopt;

    uint16_t compression = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(rec0.constData()));
    uint16_t textRecordCount = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(rec0.constData() + 8));

    QString title = fileName;
    QString author = "Unknown Author";
    uint32_t firstImageIndex = 0;
    int coverRecordIndex = -1;

    // Parse MOBI Header in Record 0
    if (rec0.size() >= 40 && rec0.mid(16, 4) == "MOBI") {
        uint32_t mobiHeaderLen = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(rec0.constData() + 20));
        
        // Full Title offset
        if (rec0.size() >= 16 + 88) {
            uint32_t titleOffset = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(rec0.constData() + 16 + 84));
            uint32_t titleLen = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(rec0.constData() + 16 + 88));
            if (titleOffset + titleLen <= static_cast<uint32_t>(rec0.size()) && titleLen > 0) {
                QString parsedTitle = QString::fromUtf8(rec0.mid(titleOffset, titleLen)).trimmed();
                if (!parsedTitle.isEmpty()) {
                    title = parsedTitle;
                }
            }
        }

        // First image record index
        if (rec0.size() >= 16 + 112) {
            firstImageIndex = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(rec0.constData() + 16 + 108));
        }

        // EXTH Header
        uint32_t exthOffset = 16 + mobiHeaderLen;
        if (exthOffset + 12 <= static_cast<uint32_t>(rec0.size()) && rec0.mid(exthOffset, 4) == "EXTH") {
            uint32_t exthCount = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(rec0.constData() + exthOffset + 8));
            uint32_t curr = exthOffset + 12;

            for (uint32_t k = 0; k < exthCount && curr + 8 <= static_cast<uint32_t>(rec0.size()); ++k) {
                uint32_t recType = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(rec0.constData() + curr));
                uint32_t recLen = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(rec0.constData() + curr + 4));
                if (recLen < 8 || curr + recLen > static_cast<uint32_t>(rec0.size())) break;

                QByteArray val = rec0.mid(curr + 8, recLen - 8);
                if (recType == 100) { // Author
                    QString a = QString::fromUtf8(val).trimmed();
                    if (!a.isEmpty()) author = a;
                } else if (recType == 503) { // Updated Title
                    QString t = QString::fromUtf8(val).trimmed();
                    if (!t.isEmpty()) title = t;
                } else if (recType == 201) { // Cover offset
                    if (val.size() >= 4) {
                        uint32_t coverOffsetVal = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(val.constData()));
                        coverRecordIndex = static_cast<int>(firstImageIndex + coverOffsetVal);
                    }
                }
                curr += recLen;
            }
        }
    }

    // Extract cover image if found
    QByteArray coverBytes;
    if (coverRecordIndex > 0 && coverRecordIndex < recordOffsets.size()) {
        uint32_t cStart = recordOffsets[coverRecordIndex];
        uint32_t cEnd = (coverRecordIndex + 1 < recordOffsets.size()) ? recordOffsets[coverRecordIndex + 1] : data.size();
        if (cStart < static_cast<uint32_t>(data.size()) && cStart < cEnd) {
            coverBytes = data.mid(cStart, cEnd - cStart);
        }
    }

    // Decompress text
    QByteArray decompressedText;
    int maxRecords = (textRecordCount > 0 && textRecordCount < numRecords) ? textRecordCount : (numRecords - 1);
    for (int i = 1; i <= maxRecords && i < recordOffsets.size(); ++i) {
        uint32_t start = recordOffsets[i];
        uint32_t end = (i + 1 < recordOffsets.size()) ? recordOffsets[i + 1] : data.size();
        if (start >= static_cast<uint32_t>(data.size()) || start >= end) continue;

        QByteArray chunk = data.mid(start, end - start);
        if (compression == 1) {
            decompressedText.append(chunk);
        } else if (compression == 2) {
            decompressedText.append(decompressPalmDocLz77(chunk));
        } else {
            decompressedText.append(chunk);
        }
    }

    QString fullHtml = QString::fromUtf8(decompressedText);

    // Multi-chapter segmentation
    // MOBI files commonly segment using <mbp:pagebreak/> or page-break-before or heading tags
    static const QRegularExpression splitRegex(R"((?:<mbp:pagebreak\s*/?>|<div[^>]*style="[^"]*page-break-before:\s*always[^"]*"[^>]*>|(?=<h[12][^>]*>)))", QRegularExpression::CaseInsensitiveOption);
    QStringList rawParts = fullHtml.split(splitRegex, Qt::SkipEmptyParts);

    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;

    static const QRegularExpression titleRegex(R"(<h[1-3][^>]*>(.*?)</h[1-3]>)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression stripTagRegex(R"(<[^>]*>)");

    int chIdx = 0;
    for (const QString& part : rawParts) {
        QString textOnly = part;
        textOnly.replace(stripTagRegex, " ").replace(QRegularExpression(R"(\s+)"), " ").trimmed();
        if (textOnly.isEmpty()) continue;

        QString chTitle;
        auto match = titleRegex.match(part);
        if (match.hasMatch()) {
            chTitle = match.captured(1);
            chTitle = chTitle.remove(stripTagRegex).trimmed();
        }

        if (chTitle.isEmpty()) {
            chTitle = QString("Chapter %1").arg(chIdx + 1);
        }

        ChapterContent c;
        c.index = chIdx;
        c.title = chTitle;
        c.textContent = textOnly;
        c.htmlContent = part;
        chapters.append(c);

        TocItem item;
        item.id = QString("toc_%1").arg(chIdx);
        item.title = chTitle;
        item.chapterIndex = chIdx;
        item.pageIndex = 0;
        item.level = 0;
        toc.append(item);

        chIdx++;
    }

    if (chapters.isEmpty()) {
        ChapterContent c;
        c.index = 0;
        c.title = title;
        c.textContent = fullHtml;
        c.htmlContent = fullHtml;
        chapters.append(c);

        TocItem item;
        item.id = "toc_0";
        item.title = title;
        item.chapterIndex = 0;
        item.pageIndex = 0;
        item.level = 0;
        toc.append(item);
    }

    BookDocument doc;
    doc.id = docId;
    doc.filePath = filePath;
    doc.title = title;
    doc.author = author;
    doc.format = "mobi";
    doc.totalChapters = static_cast<int>(chapters.size());
    doc.chapters = std::move(chapters);
    doc.toc = std::move(toc);
    doc.coverImageBytes = coverBytes;

    return doc;
}

} // namespace LunaRead
