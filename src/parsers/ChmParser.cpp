#include "ChmParser.h"
#include "../utils/EncodingDetector.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtEndian>

namespace LunaRead {

static const char CHM_MAGIC[] = "ITSF";

bool ChmParser::validateHeader(const QByteArray& data) {
    if (data.size() < 4) return false;
    return (data.startsWith(CHM_MAGIC));
}

bool ChmParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == QStringLiteral("chm");
}

bool ChmParser::canParse(const QString& filePath, const QByteArray& headerBytes) const {
    if (canParse(filePath)) {
        return true;
    }
    return validateHeader(headerBytes);
}

std::optional<BookDocument> ChmParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), QStringLiteral("chm"), filePath);
}

QVector<ChmParser::ChmFileEntry> ChmParser::parseDirectory(const QByteArray& data) {
    QVector<ChmFileEntry> entries;
    if (data.size() < 0x50) return entries;

    // Scan for directory chunk signature "PMGL"
    const QByteArray pmglSig = "PMGL";
    qsizetype pos = 0;
    while ((pos = data.indexOf(pmglSig, pos)) != -1) {
        if (pos + 20 > data.size()) break;

        // Parse PMGL directory block
        qsizetype cur = pos + 20; // Skip PMGL header
        while (cur < data.size() && cur < pos + 4096) {
            // Read filename length (encint)
            if (cur >= data.size()) break;
            quint8 nameLen = static_cast<quint8>(data[cur++]);
            if (nameLen == 0 || cur + nameLen > data.size()) break;

            QString name = QString::fromUtf8(data.mid(cur, nameLen));
            cur += nameLen;

            // Simple validation on filename
            if (name.startsWith('/') && (name.endsWith(".htm", Qt::CaseInsensitive) ||
                                         name.endsWith(".html", Qt::CaseInsensitive) ||
                                         name.endsWith(".hhc", Qt::CaseInsensitive) ||
                                         name.endsWith(".hhk", Qt::CaseInsensitive))) {
                ChmFileEntry entry;
                entry.path = name;
                entries.append(entry);
            }
        }
        pos += 4;
    }
    return entries;
}

std::optional<BookDocument> ChmParser::parseBytes(const QByteArray& data,
                                                  const QString& /*format*/,
                                                  const QString& filePath) {
    if (!validateHeader(data)) {
        return std::nullopt;
    }

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).fileName() : QString();

    BookDocument doc;
    doc.id = QStringLiteral("chm_%1").arg(qHash(data));
    doc.format = QStringLiteral("chm");
    doc.title = fileName.isEmpty() ? QStringLiteral("CHM Document") : QFileInfo(fileName).baseName();
    doc.author = QStringLiteral("Unknown Author");

    QString decodedData = QString::fromLatin1(data);

    // Extract book title from <title> tag if present
    qsizetype tStart = decodedData.indexOf("<title", 0, Qt::CaseInsensitive);
    if (tStart != -1) {
        qsizetype tClose = decodedData.indexOf('>', tStart);
        if (tClose != -1) {
            qsizetype tEnd = decodedData.indexOf("</title>", tClose, Qt::CaseInsensitive);
            if (tEnd != -1) {
                QString t = stripHtml(decodedData.mid(tClose + 1, tEnd - tClose - 1)).trimmed();
                if (!t.isEmpty()) {
                    doc.title = t;
                }
            }
        }
    }

    // Scan for embedded HTML text blocks across the binary container
    // CHM contains raw HTML or LZX blocks. We search for HTML markers:
    static const QRegularExpression htmlBlockRegex(QStringLiteral("<(?:html|body)[^>]*>(.*?)<\\/(?:html|body)>"),
                                                   QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto it = htmlBlockRegex.globalMatch(decodedData);

    int chapterIndex = 0;
    while (it.hasNext()) {
        auto match = it.next();
        QString html = match.captured(0);

        ChapterContent ch;
        ch.index = chapterIndex;

        // Extract title from <h1> or <h2>, or fallback to <title>
        static const QRegularExpression h1Regex(QStringLiteral("<h[1-2][^>]*>(.*?)<\\/h[1-2]>"), QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression titleRegex(QStringLiteral("<title[^>]*>(.*?)<\\/title>"), QRegularExpression::CaseInsensitiveOption);
        auto hMatch = h1Regex.match(html);
        if (hMatch.hasMatch()) {
            ch.title = stripHtml(hMatch.captured(1)).trimmed();
        } else {
            auto tMatch = titleRegex.match(html);
            if (tMatch.hasMatch()) {
                ch.title = stripHtml(tMatch.captured(1)).trimmed();
            } else {
                ch.title = QStringLiteral("Section %1").arg(chapterIndex + 1);
            }
        }

        ch.htmlContent = html;
        ch.textContent = stripHtml(html).trimmed();

        if (ch.textContent.length() > 50) { // Ignore tiny stub fragments
            TocItem toc;
            toc.id = QStringLiteral("toc_%1").arg(chapterIndex);
            toc.title = ch.title;
            toc.chapterIndex = chapterIndex;
            toc.pageIndex = chapterIndex;
            toc.level = 0;

            doc.chapters.append(ch);
            doc.toc.append(toc);
            chapterIndex++;
        }
    }

    // Fallback: If no raw HTML blocks matched, chunk plain text strings from the container
    if (doc.chapters.isEmpty()) {
        ChapterContent fallbackCh;
        fallbackCh.index = 0;
        fallbackCh.title = doc.title;
        fallbackCh.textContent = EncodingDetector::decodeBytes(data).simplified();
        fallbackCh.htmlContent = QStringLiteral("<p>%1</p>").arg(fallbackCh.textContent);

        TocItem toc;
        toc.id = QStringLiteral("toc_0");
        toc.title = doc.title;
        toc.chapterIndex = 0;
        toc.pageIndex = 0;
        toc.level = 0;

        doc.chapters.append(fallbackCh);
        doc.toc.append(toc);
    }

    doc.totalChapters = static_cast<int>(doc.chapters.size());
    return doc;
}

QString ChmParser::stripHtml(const QString& html) {
    static const QRegularExpression tagRegex(QStringLiteral("<[^>]*>"));
    QString t = html;
    t.replace(tagRegex, QStringLiteral(" "));
    t.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    t.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return t.simplified();
}

} // namespace LunaRead
