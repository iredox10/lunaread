#include "TxtParser.h"
#include "../utils/EncodingDetector.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace LunaRead {

bool TxtParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "txt" || ext == "text";
}

std::optional<BookDocument> TxtParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "txt", filePath);
}

std::optional<BookDocument> TxtParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    if (data.isEmpty()) return std::nullopt;

    QString text = EncodingDetector::decodeBytes(data);
    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "Text Document";
    QString docId = HashUtils::calculateBytesMd5(data);

    // Heuristic chapter regular expression
    QRegularExpression chapterRegex(
        R"((?:第[0-9一二三四五六七八九十百千万]+[章回节卷集部篇]|(?:Chapter|Section|Part|Book)\s+(?:\d+|[IVXLCDM]+)|^\d+[\.、]\s+))",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption
    );

    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;

    auto matches = chapterRegex.globalMatch(text);
    QVector<QRegularExpressionMatch> matchList;
    while (matches.hasNext()) {
        matchList.append(matches.next());
    }

    if (!matchList.isEmpty()) {
        // Preamble guard (foreword/title page before first chapter)
        if (matchList[0].capturedStart() > 200) {
            QString preambleText = text.left(matchList[0].capturedStart()).trimmed();
            ChapterContent c;
            c.index = 0;
            c.title = "Preface / Title";
            c.textContent = preambleText;
            c.htmlContent = QString("<p>%1</p>").arg(preambleText.toHtmlEscaped().replace("\n", "<br/>"));
            chapters.append(c);

            TocItem item;
            item.id = "toc_preface";
            item.title = c.title;
            item.chapterIndex = 0;
            toc.append(item);
        }

        for (int i = 0; i < matchList.size(); i++) {
            int start = matchList[i].capturedStart();
            int end = (i + 1 < matchList.size()) ? matchList[i + 1].capturedStart() : text.length();
            QString chunk = text.mid(start, end - start).trimmed();
            int lineEnd = text.indexOf('\n', start);
            if (lineEnd == -1 || lineEnd > start + 120) {
                lineEnd = std::min(static_cast<int>(text.length()), start + 80);
            }
            QString title = text.mid(start, lineEnd - start).trimmed();

            int chIdx = chapters.size();
            ChapterContent c;
            c.index = chIdx;
            c.title = title;
            c.textContent = chunk;
            c.htmlContent = QString("<p>%1</p>").arg(chunk.toHtmlEscaped().replace("\n", "<br/>"));
            chapters.append(c);

            TocItem item;
            item.id = QString("toc_%1").arg(chIdx);
            item.title = title;
            item.chapterIndex = chIdx;
            toc.append(item);
        }
    } else {
        // Fallback: chunk by ~25,000 characters
        int offset = 0;
        int index = 0;
        const int chunkSize = 25000;

        while (offset < text.length()) {
            int end = offset + chunkSize;
            if (end < text.length()) {
                int nextBreak = text.indexOf("\n\n", end);
                if (nextBreak != -1 && nextBreak - end < 2000) {
                    end = nextBreak;
                }
            } else {
                end = text.length();
            }

            QString chunk = text.mid(offset, end - offset).trimmed();
            ChapterContent c;
            c.index = index;
            c.title = QString("%1 - Part %2").arg(fileName).arg(index + 1);
            c.textContent = chunk;
            c.htmlContent = QString("<p>%1</p>").arg(chunk.toHtmlEscaped().replace("\n", "<br/>"));
            chapters.append(c);

            TocItem item;
            item.id = QString("toc_%1").arg(index);
            item.title = c.title;
            item.chapterIndex = index;
            toc.append(item);

            offset = end;
            index++;
        }
    }

    BookDocument doc;
    doc.id = docId;
    doc.title = fileName;
    doc.author = "Unknown Author";
    doc.format = "txt";
    doc.totalChapters = chapters.size();
    doc.chapters = chapters;
    doc.toc = toc;

    return doc;
}

} // namespace LunaRead
