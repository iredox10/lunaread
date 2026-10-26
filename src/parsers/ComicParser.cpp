#include "ComicParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>

namespace LunaRead {

bool ComicParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "cbz" || ext == "cbr";
}

std::optional<BookDocument> ComicParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "cbz", filePath);
}

std::optional<BookDocument> ComicParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    if (data.size() < 30) return std::nullopt;

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "Comic Book";
    QString docId = HashUtils::calculateBytesMd5(data);

    BookDocument doc;
    doc.id = docId;
    doc.title = fileName;
    doc.author = "Comic Creator";
    doc.format = "cbz";

    ChapterContent c;
    c.index = 0;
    c.title = fileName;
    c.textContent = QString("Comic Book Archive: %1").arg(fileName);
    c.isImagePage = true;
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
