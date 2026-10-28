#include "Fb2Parser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QMap>
#include <QRegularExpression>

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
    Q_UNUSED(format);
    if (data.isEmpty()) return std::nullopt;

    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "FictionBook";
    QString docId = HashUtils::calculateBytesMd5(data);

    QString title = fileName;
    QString author;
    QString coverHref;
    QMap<QString, QByteArray> binaryMap; // id -> binary data
    QMap<QString, QString> binaryMime;

    struct RawSection {
        QString title;
        QString text;
        QString html;
        int level = 0;
    };
    QVector<RawSection> rawSections;

    QXmlStreamReader xml(data);
    int currentSectionLevel = 0;
    QString currentSectionTitle;
    QString currentSectionText;
    QString currentSectionHtml;
    bool inTitle = false;
    bool inDescription = false;
    bool inCoverpage = false;
    bool inAuthor = false;
    QString authorFirst, authorMiddle, authorLast;

    auto finalizeCurrentSection = [&]() {
        if (!currentSectionText.trimmed().isEmpty() || !currentSectionHtml.trimmed().isEmpty()) {
            RawSection s;
            s.title = currentSectionTitle.trimmed();
            s.text = currentSectionText.trimmed();
            s.html = currentSectionHtml.trimmed();
            s.level = currentSectionLevel;
            rawSections.append(s);
            currentSectionText.clear();
            currentSectionHtml.clear();
            currentSectionTitle.clear();
        }
    };

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString().toLower();

            if (name == "description") {
                inDescription = true;
            } else if (name == "coverpage") {
                inCoverpage = true;
            } else if (name == "author" && inDescription) {
                inAuthor = true;
                authorFirst.clear();
                authorMiddle.clear();
                authorLast.clear();
            } else if (inAuthor && name == "first-name") {
                authorFirst = xml.readElementText().trimmed();
            } else if (inAuthor && name == "middle-name") {
                authorMiddle = xml.readElementText().trimmed();
            } else if (inAuthor && name == "last-name") {
                authorLast = xml.readElementText().trimmed();
            } else if (name == "book-title" && inDescription) {
                title = xml.readElementText().trimmed();
            } else if (name == "image") {
                QString href = xml.attributes().value("l:href").toString();
                if (href.isEmpty()) href = xml.attributes().value("href").toString();
                if (href.startsWith('#')) href = href.mid(1);

                if (inCoverpage && coverHref.isEmpty()) {
                    coverHref = href;
                } else if (!inDescription) {
                    currentSectionHtml += QString("<p style=\"text-align: center;\"><img src=\"fb2-img://%1\" style=\"max-width: 90%; height: auto; border-radius: 4px;\"/></p>\n").arg(href);
                }
            } else if (name == "binary") {
                QString id = xml.attributes().value("id").toString();
                QString contentType = xml.attributes().value("content-type").toString();
                QString base64Str = xml.readElementText().trimmed();
                QByteArray decoded = QByteArray::fromBase64(base64Str.toLatin1());
                binaryMap[id] = decoded;
                binaryMime[id] = contentType;
            } else if (name == "section") {
                finalizeCurrentSection();
                currentSectionLevel++;
            } else if (name == "title") {
                inTitle = true;
            } else if (name == "p") {
                QString pText = xml.readElementText().trimmed();
                if (inTitle) {
                    if (!currentSectionTitle.isEmpty()) currentSectionTitle += " ";
                    currentSectionTitle += pText;
                } else if (!inDescription) {
                    currentSectionText += pText + "\n\n";
                    currentSectionHtml += QString("<p>%1</p>\n").arg(pText.toHtmlEscaped());
                }
            } else if (name == "empty-line") {
                if (!inDescription && !inTitle) {
                    currentSectionText += "\n";
                    currentSectionHtml += "<br/>\n";
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            QString name = xml.name().toString().toLower();
            if (name == "description") {
                inDescription = false;
            } else if (name == "coverpage") {
                inCoverpage = false;
            } else if (name == "author" && inAuthor) {
                inAuthor = false;
                QString fullAuthor = QString("%1 %2 %3").arg(authorFirst, authorMiddle, authorLast).simplified();
                if (!fullAuthor.isEmpty() && author.isEmpty()) {
                    author = fullAuthor;
                }
            } else if (name == "title") {
                inTitle = false;
            } else if (name == "section") {
                finalizeCurrentSection();
                if (currentSectionLevel > 0) currentSectionLevel--;
            }
        }
    }

    finalizeCurrentSection();

    if (author.isEmpty()) {
        author = "Unknown Author";
    }

    // Resolve cover image
    QByteArray coverBytes;
    if (!coverHref.isEmpty() && binaryMap.contains(coverHref)) {
        coverBytes = binaryMap[coverHref];
    } else {
        // Fallback: search for first binary with image content-type
        for (auto it = binaryMap.begin(); it != binaryMap.end(); ++it) {
            QString mime = binaryMime.value(it.key());
            if (mime.contains("image") || it.key().contains("cover", Qt::CaseInsensitive)) {
                coverBytes = it.value();
                break;
            }
        }
    }

    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;

    if (rawSections.isEmpty()) {
        // Fallback: entire text as single chapter
        ChapterContent c;
        c.index = 0;
        c.title = title;
        c.textContent = QString::fromUtf8(data);
        c.htmlContent.clear();
        chapters.append(c);

        TocItem item;
        item.id = "toc_0";
        item.title = title;
        item.chapterIndex = 0;
        item.pageIndex = 0;
        item.level = 0;
        toc.append(item);
    } else {
        for (int i = 0; i < rawSections.size(); ++i) {
            const auto& sec = rawSections[i];
            ChapterContent c;
            c.index = i;
            c.title = sec.title.isEmpty() ? QString("Chapter %1").arg(i + 1) : sec.title;
            c.textContent = sec.text;

            // Replace fb2-img:// links with base64 data URIs so they render in Qt QTextDocument
            QString processedHtml = sec.html;
            static const QRegularExpression imgRegex(R"(fb2-img://([^"\s>]+))");
            QRegularExpressionMatchIterator it = imgRegex.globalMatch(processedHtml);
            while (it.hasNext()) {
                auto match = it.next();
                QString imgId = match.captured(1);
                if (binaryMap.contains(imgId)) {
                    QString mime = binaryMime.value(imgId, QStringLiteral("image/jpeg"));
                    QString dataUri = QString("data:%1;base64,%2").arg(mime, QString::fromLatin1(binaryMap[imgId].toBase64()));
                    processedHtml.replace(match.captured(0), dataUri);
                }
            }

            c.htmlContent = processedHtml;
            chapters.append(c);

            TocItem item;
            item.id = QString("toc_%1").arg(i);
            item.title = c.title;
            item.chapterIndex = i;
            item.pageIndex = 0;
            item.level = std::max(0, sec.level - 1);
            toc.append(item);
        }
    }

    BookDocument doc;
    doc.id = docId;
    doc.filePath = filePath;
    doc.title = title;
    doc.author = author;
    doc.format = "fb2";
    doc.totalChapters = static_cast<int>(chapters.size());
    doc.chapters = std::move(chapters);
    doc.toc = std::move(toc);
    doc.coverImageBytes = coverBytes;

    return doc;
}

} // namespace LunaRead
