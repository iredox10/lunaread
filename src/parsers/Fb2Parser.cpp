#include "Fb2Parser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

namespace LunaRead {

bool Fb2Parser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "fb2";
}

std::optional<BookDocument> Fb2Parser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "fb2", filePath);
}

std::optional<BookDocument> Fb2Parser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    if (data.isEmpty()) return std::nullopt;

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "FictionBook";
    QString docId = HashUtils::calculateBytesMd5(data);

    QString title = fileName;
    QString author = "Unknown Author";

    QXmlStreamReader xml(data);
    QString currentText;
    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;
    QByteArray coverBytes;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            if (name == "book-title") {
                title = xml.readElementText().trimmed();
            } else if (name == "author") {
                // Read author subelements
            } else if (name == "binary") {
                QString id = xml.attributes().value("id").toString();
                QString contentType = xml.attributes().value("content-type").toString();
                QString base64Str = xml.readElementText().trimmed();
                QByteArray decoded = QByteArray::fromBase64(base64Str.toLatin1());
                if (coverBytes.isEmpty() && contentType.contains("image")) {
                    coverBytes = decoded;
                }
            } else if (name == "p") {
                currentText += xml.readElementText().trimmed() + "\n\n";
            }
        }
    }

    if (currentText.isEmpty()) {
        currentText = QString::fromUtf8(data);
    }

    ChapterContent c;
    c.index = 0;
    c.title = title;
    c.textContent = currentText;
    c.htmlContent.clear();
    chapters.append(c);

    TocItem item;
    item.id = "toc_0";
    item.title = title;
    item.chapterIndex = 0;
    toc.append(item);

    BookDocument doc;
    doc.id = docId;
    doc.title = title;
    doc.author = author;
    doc.format = "fb2";
    doc.totalChapters = 1;
    doc.chapters = chapters;
    doc.toc = toc;
    doc.coverImageBytes = coverBytes;

    return doc;
}

} // namespace LunaRead
