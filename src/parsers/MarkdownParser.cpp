#include "MarkdownParser.h"
#include "../utils/EncodingDetector.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace LunaRead {

bool MarkdownParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "md" || ext == "markdown";
}

std::optional<BookDocument> MarkdownParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "md", filePath);
}

std::optional<BookDocument> MarkdownParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    if (data.isEmpty()) return std::nullopt;

    QString text = EncodingDetector::decodeBytes(data);
    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "Markdown Document";
    QString docId = HashUtils::calculateBytesMd5(data);

    // Split chapters on H1 or H2: ^#\s+ or ^##\s+
    QRegularExpression headerRegex(R"(^(#{1,2})\s+(.+)$)", QRegularExpression::MultilineOption);
    auto matches = headerRegex.globalMatch(text);

    QVector<QRegularExpressionMatch> matchList;
    while (matches.hasNext()) {
        matchList.append(matches.next());
    }

    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;

    if (!matchList.isEmpty()) {
        for (int i = 0; i < matchList.size(); ++i) {
            int start = matchList[i].capturedStart();
            int end = (i + 1 < matchList.size()) ? matchList[i + 1].capturedStart() : text.length();
            QString chunk = text.mid(start, end - start).trimmed();
            QString heading = matchList[i].captured(2).trimmed();

            int chIdx = chapters.size();
            ChapterContent c;
            c.index = chIdx;
            c.title = heading;
            c.textContent = chunk;
            c.htmlContent = QString("<p>%1</p>").arg(chunk.toHtmlEscaped().replace("\n", "<br/>"));
            chapters.append(c);

            TocItem item;
            item.id = QString("md_toc_%1").arg(chIdx);
            item.title = heading;
            item.chapterIndex = chIdx;
            toc.append(item);
        }
    } else {
        ChapterContent c;
        c.index = 0;
        c.title = fileName;
        c.textContent = text;
        c.htmlContent = QString("<p>%1</p>").arg(text.toHtmlEscaped().replace("\n", "<br/>"));
        chapters.append(c);

        TocItem item;
        item.id = "md_toc_0";
        item.title = fileName;
        item.chapterIndex = 0;
        toc.append(item);
    }

    BookDocument doc;
    doc.id = docId;
    doc.title = fileName;
    doc.author = "Unknown Author";
    doc.format = "md";
    doc.totalChapters = chapters.size();
    doc.chapters = chapters;
    doc.toc = toc;

    return doc;
}

} // namespace LunaRead
