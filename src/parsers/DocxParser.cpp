#include "DocxParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>

namespace LunaRead {

bool DocxParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "docx";
}

std::optional<BookDocument> DocxParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "docx", filePath);
}

std::optional<BookDocument> DocxParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    if (data.size() < 30) return std::nullopt;

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "Word Document";
    QString docId = HashUtils::calculateBytesMd5(data);

    BookDocument doc;
    doc.id = docId;
    doc.title = fileName;
    doc.author = "Unknown Author";
    doc.format = "docx";

    ChapterContent c;
    c.index = 0;
    c.title = fileName;
    c.textContent = QString("Word Document: %1\n\nLoaded into LunaRead-Qt desktop reader.").arg(fileName);
    c.htmlContent = QString("<h1>%1</h1><p>%2</p>").arg(fileName).arg(c.textContent);
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
