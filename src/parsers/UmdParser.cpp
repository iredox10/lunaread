#include "UmdParser.h"
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <zlib.h>

namespace LunaRead {

static const quint32 UMD_MAGIC = 0xDE9A9B89;

bool UmdParser::validateHeader(const QByteArray& data) {
    if (data.size() < 4) return false;
    quint32 magic = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData()));
    return magic == UMD_MAGIC;
}

bool UmdParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == QStringLiteral("umd");
}

bool UmdParser::canParse(const QString& filePath, const QByteArray& headerBytes) const {
    if (canParse(filePath)) {
        return true;
    }
    return validateHeader(headerBytes);
}

std::optional<BookDocument> UmdParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), QStringLiteral("umd"), filePath);
}

QString UmdParser::readUcs2String(const QByteArray& data, qsizetype offset, qsizetype length) {
    if (offset < 0 || offset + length > data.size()) return QString();
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData() + offset), length / 2);
}

std::optional<BookDocument> UmdParser::parseBytes(const QByteArray& data,
                                                  const QString& /*format*/,
                                                  const QString& filePath) {
    if (!validateHeader(data)) {
        return std::nullopt;
    }

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).fileName() : QString();

    BookDocument doc;
    doc.id = QStringLiteral("umd_%1").arg(qHash(data));
    doc.format = QStringLiteral("umd");
    doc.title = fileName.isEmpty() ? QStringLiteral("UMD Book") : QFileInfo(fileName).baseName();
    doc.author = QStringLiteral("Unknown Author");

    qsizetype pos = 4;
    QByteArray uncompressedText;
    QStringList chapterTitles;
    QVector<quint32> chapterOffsets;

    while (pos < data.size()) {
        char ch = data[pos];
        if (ch == '#') {
            pos++;
            if (pos >= data.size()) break;
            quint8 blockType = static_cast<quint8>(data[pos++]);
            // Skip 2 bytes padding/separator
            pos += 2;
            if (pos >= data.size()) break;
            quint8 dataLen = static_cast<quint8>(data[pos++]);
            if (pos + dataLen > data.size()) break;

            if (blockType == 0x03) { // Title
                QString title = readUcs2String(data, pos, dataLen);
                if (!title.isEmpty()) doc.title = title;
            } else if (blockType == 0x04) { // Author
                QString author = readUcs2String(data, pos, dataLen);
                if (!author.isEmpty()) doc.author = author;
            }
            pos += dataLen;
        } else if (ch == '$') {
            pos++;
            if (pos >= data.size()) break;
            char nextCh = data[pos++];
            if (nextCh == '#') {
                if (pos >= data.size()) break;
                quint8 blockType = static_cast<quint8>(data[pos++]);
                pos += 4; // Skip random key
                if (pos + 4 > data.size()) break;
                quint32 blockLen = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + pos));
                pos += 4;

                if (blockType == 0x84) { // Chapter titles
                    qsizetype endPos = std::min<qsizetype>(data.size(), pos + blockLen - 9);
                    while (pos < endPos) {
                        quint8 titleByteLen = static_cast<quint8>(data[pos++]);
                        if (pos + titleByteLen > endPos) break;
                        QString cTitle = readUcs2String(data, pos, titleByteLen);
                        chapterTitles.append(cTitle);
                        pos += titleByteLen;
                    }
                } else if (blockType == 0x83) { // Chapter offsets
                    qsizetype numOffsets = (blockLen - 9) / 4;
                    for (qsizetype i = 0; i < numOffsets && pos + 4 <= data.size(); ++i) {
                        quint32 off = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + pos));
                        chapterOffsets.append(off);
                        pos += 4;
                    }
                } else if (blockType == 0x81) { // Content stream chunk
                    // Decompress zlib chunk
                    qsizetype chunkDataLen = blockLen - 9;
                    if (pos + chunkDataLen <= data.size()) {
                        QByteArray compressedChunk = data.mid(pos, chunkDataLen);
                        uLongf destLen = 131072; // 128KB chunk buffer
                        QByteArray decompressed;
                        decompressed.resize(static_cast<qsizetype>(destLen));
                        if (uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &destLen,
                                       reinterpret_cast<const Bytef*>(compressedChunk.constData()),
                                       static_cast<uLong>(compressedChunk.size())) == Z_OK) {
                            decompressed.resize(static_cast<qsizetype>(destLen));
                            uncompressedText.append(decompressed);
                        }
                        pos += chunkDataLen;
                    }
                } else {
                    pos += (blockLen > 9 ? blockLen - 9 : 0);
                }
            }
        } else {
            pos++;
        }
    }

    // Convert uncompressed UTF-16LE text
    QString fullBookText;
    if (!uncompressedText.isEmpty()) {
        fullBookText = QString::fromUtf16(reinterpret_cast<const char16_t*>(uncompressedText.constData()),
                                          uncompressedText.size() / 2);
    }

    if (fullBookText.isEmpty()) {
        // Fallback plain decoding if compressed chunks weren't parsed
        fullBookText = QString::fromLatin1(data);
    }

    // Assemble chapters
    if (chapterTitles.isEmpty()) {
        ChapterContent ch;
        ch.index = 0;
        ch.title = doc.title;
        ch.textContent = fullBookText;
        ch.htmlContent = QStringLiteral("<p>%1</p>").arg(fullBookText);

        TocItem toc;
        toc.id = QStringLiteral("toc_0");
        toc.title = doc.title;
        toc.chapterIndex = 0;
        toc.pageIndex = 0;
        toc.level = 0;

        doc.chapters.append(ch);
        doc.toc.append(toc);
    } else {
        // Split using offsets if available, otherwise evenly
        for (int i = 0; i < chapterTitles.size(); ++i) {
            ChapterContent ch;
            ch.index = i;
            ch.title = chapterTitles[i];

            int startChar = (i < chapterOffsets.size()) ? static_cast<int>(chapterOffsets[i] / 2) : 0;
            int endChar = (i + 1 < chapterOffsets.size()) ? static_cast<int>(chapterOffsets[i + 1] / 2) : static_cast<int>(fullBookText.length());

            if (startChar < fullBookText.length()) {
                ch.textContent = fullBookText.mid(startChar, endChar - startChar);
            } else {
                ch.textContent = QString();
            }
            ch.htmlContent = QStringLiteral("<p>%1</p>").arg(ch.textContent);

            TocItem toc;
            toc.id = QStringLiteral("toc_%1").arg(i);
            toc.title = ch.title;
            toc.chapterIndex = i;
            toc.pageIndex = i;
            toc.level = 0;

            doc.chapters.append(ch);
            doc.toc.append(toc);
        }
    }

    doc.totalChapters = static_cast<int>(doc.chapters.size());
    return doc;
}

} // namespace LunaRead
