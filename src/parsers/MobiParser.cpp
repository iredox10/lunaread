#include "MobiParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

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

    // Read Record 0 for PalmDOC header
    uint32_t rec0Offset = recordOffsets[0];
    uint32_t rec0End = numRecords > 1 ? recordOffsets[1] : data.size();
    if (rec0End > static_cast<uint32_t>(data.size()) || rec0Offset >= rec0End) return std::nullopt;

    QByteArray rec0 = data.mid(rec0Offset, rec0End - rec0Offset);
    if (rec0.size() < 16) return std::nullopt;

    uint16_t compression = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(rec0.constData()));
    uint16_t textRecordCount = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(rec0.constData() + 8));

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

    QString text = QString::fromUtf8(decompressedText);

    BookDocument doc;
    doc.id = docId;
    doc.title = fileName;
    doc.author = "Unknown Author";
    doc.format = "mobi";

    ChapterContent c;
    c.index = 0;
    c.title = fileName;
    c.textContent = text.isEmpty() ? fileName : text;
    c.htmlContent = QString("<p>%1</p>").arg(c.textContent.toHtmlEscaped().replace("\n", "<br/>"));
    doc.chapters.append(c);

    TocItem item;
    item.id = "toc_0";
    item.title = fileName;
    item.chapterIndex = 0;
    doc.toc.append(item);
    doc.totalChapters = 1;

    return doc;
}

} // namespace LunaRead
