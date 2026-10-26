#include "HtmlParser.h"
#include "../utils/EncodingDetector.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace LunaRead {

bool HtmlParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return supportedExtensions().contains(ext);
}

bool HtmlParser::canParse(const QString& filePath, const QByteArray& headerBytes) const {
    if (canParse(filePath)) {
        return true;
    }
    const QString headerStr = QString::fromUtf8(headerBytes.left(512)).toLower();
    return headerStr.contains("<!doctype html") || headerStr.contains("<html") || headerStr.contains("<?xml");
}

std::optional<BookDocument> HtmlParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), QFileInfo(filePath).suffix(), filePath);
}

std::optional<BookDocument> HtmlParser::parseBytes(const QByteArray& data,
                                                   const QString& /*format*/,
                                                   const QString& filePath) {
    if (data.isEmpty()) {
        return std::nullopt;
    }

    QString rawHtml = EncodingDetector::decodeBytes(data);

    // Strip scripts and styles
    static const QRegularExpression scriptRegex(QStringLiteral("<script\\b[^<]*(?:(?!<\\/script>)<[^<]*)*<\\/script>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression styleRegex(QStringLiteral("<style\\b[^<]*(?:(?!<\\/style>)<[^<]*)*<\\/style>"), QRegularExpression::CaseInsensitiveOption);
    rawHtml.remove(scriptRegex);
    rawHtml.remove(styleRegex);

    BookDocument doc;
    doc.id = QStringLiteral("doc_%1").arg(qHash(data));
    doc.format = QStringLiteral("html");

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).fileName() : QString();

    // Title extraction
    static const QRegularExpression titleRegex(QStringLiteral("<title[^>]*>(.*?)<\\/title>"), QRegularExpression::CaseInsensitiveOption);
    auto titleMatch = titleRegex.match(rawHtml);
    if (titleMatch.hasMatch()) {
        doc.title = titleMatch.captured(1).trimmed();
    } else {
        doc.title = fileName.isEmpty() ? QStringLiteral("Untitled Document") : QFileInfo(fileName).baseName();
    }

    // Author extraction from meta tags
    static const QRegularExpression authorRegex(QStringLiteral("<meta\\s+name=[\"']author[\"']\\s+content=[\"'](.*?)[\"']"), QRegularExpression::CaseInsensitiveOption);
    auto authorMatch = authorRegex.match(rawHtml);
    if (authorMatch.hasMatch()) {
        doc.author = authorMatch.captured(1).trimmed();
    } else {
        doc.author = QStringLiteral("Unknown Author");
    }

    // Split chapters on <h1> or <h2> or <hr>
    static const QRegularExpression headingRegex(QStringLiteral("<h[1-2][^>]*>(.*?)<\\/h[1-2]>"), QRegularExpression::CaseInsensitiveOption);
    auto matchIt = headingRegex.globalMatch(rawHtml);

    QVector<int> splitPoints;
    QStringList chapterTitles;

    while (matchIt.hasNext()) {
        auto m = matchIt.next();
        splitPoints.append(static_cast<int>(m.capturedStart()));
        chapterTitles.append(stripTags(m.captured(1)).trimmed());
    }

    if (splitPoints.isEmpty()) {
        // Single chapter document
        ChapterContent ch;
        ch.index = 0;
        ch.title = doc.title;
        ch.htmlContent = rawHtml;
        ch.textContent = stripTags(rawHtml).trimmed();

        TocItem toc;
        toc.id = QStringLiteral("toc_0");
        toc.title = doc.title;
        toc.chapterIndex = 0;
        toc.pageIndex = 0;
        toc.level = 0;

        doc.chapters.append(ch);
        doc.toc.append(toc);
    } else {
        // Multi-chapter split
        for (int i = 0; i < splitPoints.size(); ++i) {
            int start = splitPoints[i];
            int end = (i + 1 < splitPoints.size()) ? splitPoints[i + 1] : static_cast<int>(rawHtml.length());
            QString chHtml = rawHtml.mid(start, end - start);

            ChapterContent ch;
            ch.index = i;
            ch.title = chapterTitles[i].isEmpty() ? QStringLiteral("Section %1").arg(i + 1) : chapterTitles[i];
            ch.htmlContent = chHtml;
            ch.textContent = stripTags(chHtml).trimmed();

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

QString HtmlParser::stripTags(const QString& html) {
    static const QRegularExpression tagRegex(QStringLiteral("<[^>]*>"));
    QString text = html;
    text.replace(tagRegex, QStringLiteral(" "));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    return text.simplified();
}

QString HtmlParser::extractMetadata(const QString& html, const QString& tag) {
    QRegularExpression re(QStringLiteral("<meta\\s+name=[\"']%1[\"']\\s+content=[\"'](.*?)[\"']").arg(tag),
                          QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(html);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

} // namespace LunaRead
