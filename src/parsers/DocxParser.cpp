#include "DocxParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QTemporaryFile>

namespace LunaRead {

bool DocxParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "docx";
}

std::optional<BookDocument> DocxParser::parseFile(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return std::nullopt;
    }

    const QString displayName = QFileInfo(filePath).completeBaseName();
    if (auto doc = parseArchive(filePath, displayName)) {
        return doc;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return fallbackDocument(file.readAll(), filePath);
}

std::optional<BookDocument> DocxParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    Q_UNUSED(format);
    if (data.size() < 30) return std::nullopt;

    const QString displayName = filePath.isEmpty()
        ? QStringLiteral("Word Document")
        : QFileInfo(filePath).completeBaseName();

    QTemporaryFile tempDocx;
    if (tempDocx.open()) {
        tempDocx.write(data);
        tempDocx.flush();
        if (auto doc = parseArchive(tempDocx.fileName(), displayName)) {
            return doc;
        }
    }

    return fallbackDocument(data, filePath);
}

std::optional<BookDocument> DocxParser::parseArchive(const QString& archivePath, const QString& displayName) {
    QString unzip = QStandardPaths::findExecutable("unzip");
    if (unzip.isEmpty()) {
        return std::nullopt;
    }

    QProcess proc;
    proc.start(unzip, QStringList() << "-p" << archivePath << "word/document.xml");
    if (!proc.waitForFinished(10000) || proc.exitCode() != 0) {
        return std::nullopt;
    }

    QByteArray xmlData = proc.readAllStandardOutput();
    if (xmlData.isEmpty()) {
        return std::nullopt;
    }

    QXmlStreamReader xml(xmlData);
    QString fullText;
    QString html;
    QString currentParaText;
    QString currentParaHtml;
    bool isBold = false;
    bool isItalic = false;
    bool isHeading = false;

    while (!xml.atEnd() && !xml.hasError()) {
        auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            auto name = xml.name();
            if (name == "p") {
                currentParaText.clear();
                currentParaHtml.clear();
                isHeading = false;
            } else if (name == "pStyle") {
                QString val = xml.attributes().value("w:val").toString();
                if (val.contains("Heading", Qt::CaseInsensitive) || val.contains("Title", Qt::CaseInsensitive)) {
                    isHeading = true;
                }
            } else if (name == "b") {
                isBold = true;
            } else if (name == "i") {
                isItalic = true;
            } else if (name == "t") {
                QString textRun = xml.readElementText();
                currentParaText += textRun;
                QString escaped = textRun.toHtmlEscaped();
                if (isBold && isItalic) escaped = QString("<b><i>%1</i></b>").arg(escaped);
                else if (isBold) escaped = QString("<b>%1</b>").arg(escaped);
                else if (isItalic) escaped = QString("<i>%1</i>").arg(escaped);
                currentParaHtml += escaped;
                isBold = false;
                isItalic = false;
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == "p") {
                if (!currentParaText.trimmed().isEmpty()) {
                    fullText += currentParaText.trimmed() + "\n\n";
                    if (isHeading) {
                        html += QString("<h2>%1</h2>\n").arg(currentParaHtml);
                    } else {
                        html += QString("<p>%1</p>\n").arg(currentParaHtml);
                    }
                }
            }
        }
    }

    if (fullText.trimmed().isEmpty()) {
        return std::nullopt;
    }

    BookDocument doc;
    doc.id = HashUtils::calculateFileMd5(archivePath);
    doc.title = displayName;
    doc.author = "Microsoft Word";
    doc.format = "docx";

    ChapterContent c;
    c.index = 0;
    c.title = displayName;
    c.textContent = fullText.trimmed();
    c.htmlContent = html.trimmed();
    doc.chapters.append(c);

    TocItem item;
    item.id = "toc_0";
    item.title = displayName;
    item.chapterIndex = 0;
    doc.toc.append(item);
    doc.totalChapters = 1;

    return doc;
}

BookDocument DocxParser::fallbackDocument(const QByteArray& data, const QString& filePath) {
    const QString fileName = filePath.isEmpty()
        ? QStringLiteral("Word Document")
        : QFileInfo(filePath).completeBaseName();

    BookDocument doc;
    doc.id = HashUtils::calculateBytesMd5(data);
    doc.title = fileName;
    doc.author = "Word Author";
    doc.format = "docx";

    ChapterContent c;
    c.index = 0;
    c.title = fileName;
    c.textContent = QString("Word Document: %1").arg(fileName);
    c.htmlContent.clear();
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
