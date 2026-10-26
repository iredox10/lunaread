#include "OdtParser.h"
#include "../utils/HashUtils.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QXmlStreamReader>
#include <memory>
#include <algorithm>

#include <archive.h>
#include <archive_entry.h>

namespace LunaRead {

namespace {

struct ArchiveDeleter {
    void operator()(struct archive* a) const noexcept {
        if (a) {
            archive_read_close(a);
            archive_read_free(a);
        }
    }
};

using ScopedArchive = std::unique_ptr<struct archive, ArchiveDeleter>;

struct ExtractedElement {
    QString plainText;
    QString htmlContent;
    QVector<QString> imageReferences;
};

// Recursively processes an XML element (such as <text:h> or <text:p>)
ExtractedElement parseXmlTextElement(QXmlStreamReader& xml, const QStringView& targetEndTag) {
    ExtractedElement result;

    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::Characters) {
            const QString text = xml.text().toString();
            result.plainText.append(text);
            result.htmlContent.append(text.toHtmlEscaped());
        } else if (token == QXmlStreamReader::StartElement) {
            const auto tagName = xml.name();

            if (tagName == u"s") { // <text:s text:c="N"/>
                int spaceCount = xml.attributes().value(u"text:c").toInt();
                if (spaceCount <= 0) {
                    spaceCount = 1;
                }
                result.plainText.append(QString(spaceCount, QLatin1Char(' ')));
                for (int i = 0; i < spaceCount; ++i) {
                    result.htmlContent.append(QStringLiteral("&nbsp;"));
                }
            } else if (tagName == u"tab") { // <text:tab/>
                result.plainText.append(QLatin1Char('\t'));
                result.htmlContent.append(QStringLiteral("&emsp;"));
            } else if (tagName == u"line-break") { // <text:line-break/>
                result.plainText.append(QLatin1Char('\n'));
                result.htmlContent.append(QStringLiteral("<br/>"));
            } else if (tagName == u"a") { // <text:a xlink:href="...">
                const QString href = xml.attributes().value(u"xlink:href").toString();
                result.htmlContent.append(QStringLiteral("<a href=\"%1\">").arg(href.toHtmlEscaped()));
            } else if (tagName == u"image") { // <draw:image xlink:href="...">
                const QString href = xml.attributes().value(u"xlink:href").toString();
                if (!href.isEmpty()) {
                    result.imageReferences.append(href);
                    result.htmlContent.append(QStringLiteral("<img src=\"%1\" />").arg(href.toHtmlEscaped()));
                }
            } else if (tagName == u"span") {
                result.htmlContent.append(QStringLiteral("<span>"));
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const auto tagName = xml.name();
            if (tagName == targetEndTag) {
                break;
            } else if (tagName == u"a") {
                result.htmlContent.append(QStringLiteral("</a>"));
            } else if (tagName == u"span") {
                result.htmlContent.append(QStringLiteral("</span>"));
            }
        }
    }

    return result;
}

std::optional<QMap<QString, QByteArray>> extractOdtArchiveEntries(const QByteArray& data) {
    if (data.size() < 30) {
        return std::nullopt;
    }

    // Verify ZIP magic number PK\x03\x04
    if (data[0] != 'P' || data[1] != 'K' || data[2] != 0x03 || data[3] != 0x04) {
        return std::nullopt;
    }

    ScopedArchive archiveHandle(archive_read_new());
    if (!archiveHandle) {
        return std::nullopt;
    }

    archive_read_support_format_zip(archiveHandle.get());
    archive_read_support_filter_all(archiveHandle.get());

    if (archive_read_open_memory(archiveHandle.get(), data.constData(), static_cast<size_t>(data.size())) != ARCHIVE_OK) {
        return std::nullopt;
    }

    QMap<QString, QByteArray> entries;
    struct archive_entry* entry = nullptr;

    while (archive_read_next_header(archiveHandle.get(), &entry) == ARCHIVE_OK) {
        const char* pathStr = archive_entry_pathname(entry);
        if (!pathStr) {
            continue;
        }

        const QString entryPath = QString::fromUtf8(pathStr);
        QByteArray entryData;
        char buffer[16384];
        la_ssize_t bytesRead = 0;

        while ((bytesRead = archive_read_data(archiveHandle.get(), buffer, sizeof(buffer))) > 0) {
            entryData.append(buffer, static_cast<qsizetype>(bytesRead));
        }

        entries.insert(entryPath, entryData);
    }

    if (!entries.contains(QStringLiteral("content.xml"))) {
        return std::nullopt;
    }

    return entries;
}

} // namespace

bool OdtParser::isValidOdtPackage(const QByteArray& data) {
    auto entriesOpt = extractOdtArchiveEntries(data);
    return entriesOpt.has_value();
}

bool OdtParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains(QLatin1Char('.'))) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == QStringLiteral("odt");
}

std::optional<BookDocument> OdtParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), QStringLiteral("odt"), filePath);
}

std::optional<BookDocument> OdtParser::parseBytes(const QByteArray& data,
                                                 const QString& format,
                                                 const QString& filePath) {
    Q_UNUSED(format);

    auto archiveOpt = extractOdtArchiveEntries(data);
    if (!archiveOpt.has_value()) {
        return std::nullopt;
    }

    const auto& files = *archiveOpt;
    const QString fileName = !filePath.isEmpty()
        ? QFileInfo(filePath).completeBaseName()
        : QStringLiteral("OpenDocument Text");

    const QString docId = HashUtils::calculateBytesMd5(data);

    // Extract metadata from meta.xml
    QString docTitle;
    QString docAuthor;

    if (files.contains(QStringLiteral("meta.xml"))) {
        QXmlStreamReader metaXml(files[QStringLiteral("meta.xml")]);
        while (!metaXml.atEnd() && !metaXml.hasError()) {
            const auto token = metaXml.readNext();
            if (token == QXmlStreamReader::StartElement) {
                const auto tagName = metaXml.name();
                if (tagName == u"title" && docTitle.isEmpty()) {
                    docTitle = metaXml.readElementText().trimmed();
                } else if ((tagName == u"creator" || tagName == u"initial-creator") && docAuthor.isEmpty()) {
                    docAuthor = metaXml.readElementText().trimmed();
                }
            }
        }
    }

    // Extract embedded media and thumbnail
    QByteArray coverBytes;
    QMap<QString, QByteArray> allImages;

    if (files.contains(QStringLiteral("Thumbnails/thumbnail.png"))) {
        coverBytes = files[QStringLiteral("Thumbnails/thumbnail.png")];
    }

    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const QString& key = it.key();
        if (key.startsWith(QStringLiteral("Pictures/")) ||
            key.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive) ||
            key.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive) ||
            key.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive) ||
            key.endsWith(QStringLiteral(".webp"), Qt::CaseInsensitive)) {
            allImages.insert(key, it.value());
            if (coverBytes.isEmpty() && !key.contains(QStringLiteral("Thumbnails/"))) {
                coverBytes = it.value();
            }
        }
    }

    // Parse content.xml
    QXmlStreamReader contentXml(files[QStringLiteral("content.xml")]);
    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;

    ChapterContent currentChapter;
    currentChapter.index = 0;
    bool hasStartedChapter = false;

    auto commitChapter = [&]() {
        if (!currentChapter.textContent.trimmed().isEmpty() || !currentChapter.title.isEmpty()) {
            currentChapter.index = static_cast<int>(chapters.size());
            if (currentChapter.title.isEmpty()) {
                currentChapter.title = QStringLiteral("Chapter %1").arg(currentChapter.index + 1);
            }
            chapters.append(currentChapter);

            bool tocExists = false;
            for (const auto& item : toc) {
                if (item.chapterIndex == currentChapter.index) {
                    tocExists = true;
                    break;
                }
            }

            if (!tocExists) {
                TocItem tocItem;
                tocItem.id = QStringLiteral("toc_%1").arg(currentChapter.index);
                tocItem.title = currentChapter.title;
                tocItem.chapterIndex = currentChapter.index;
                tocItem.level = 1;
                toc.append(tocItem);
            }

            currentChapter = ChapterContent();
            currentChapter.index = static_cast<int>(chapters.size());
        }
    };

    while (!contentXml.atEnd() && !contentXml.hasError()) {
        const auto token = contentXml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            const auto tagName = contentXml.name();

            if (tagName == u"h") { // <text:h>
                int outlineLevel = contentXml.attributes().value(u"text:outline-level").toInt();
                if (outlineLevel <= 0) {
                    outlineLevel = 1;
                }

                ExtractedElement hElement = parseXmlTextElement(contentXml, u"h");
                QString headingText = hElement.plainText.trimmed();
                if (headingText.isEmpty()) {
                    headingText = QStringLiteral("Chapter %1").arg(chapters.size() + 1);
                }

                // If level 1 heading and chapter has content, segment into a new chapter
                if (outlineLevel == 1 && (!currentChapter.textContent.trimmed().isEmpty() || hasStartedChapter)) {
                    commitChapter();
                }

                hasStartedChapter = true;
                if (currentChapter.title.isEmpty()) {
                    currentChapter.title = headingText;
                }

                // Add entry to Table of Contents
                TocItem tocItem;
                tocItem.id = QStringLiteral("toc_%1").arg(toc.size());
                tocItem.title = headingText;
                tocItem.chapterIndex = static_cast<int>(chapters.size());
                tocItem.level = outlineLevel;
                toc.append(tocItem);

                // Add heading text to chapter
                currentChapter.textContent.append(headingText + QStringLiteral("\n\n"));
                const int hTagLevel = std::min(outlineLevel + 1, 6);
                currentChapter.htmlContent.append(QStringLiteral("<h%1>%2</h%1>\n")
                                                     .arg(hTagLevel)
                                                     .arg(hElement.htmlContent));
            } else if (tagName == u"p") { // <text:p>
                ExtractedElement pElement = parseXmlTextElement(contentXml, u"p");
                const QString paragraphText = pElement.plainText.trimmed();

                if (!paragraphText.isEmpty() || !pElement.imageReferences.isEmpty()) {
                    hasStartedChapter = true;
                    currentChapter.textContent.append(paragraphText + QStringLiteral("\n\n"));
                    currentChapter.htmlContent.append(QStringLiteral("<p>") + pElement.htmlContent + QStringLiteral("</p>\n"));

                    for (const auto& href : pElement.imageReferences) {
                        if (allImages.contains(href)) {
                            currentChapter.images.insert(href, allImages[href]);
                        }
                    }
                }
            }
        }
    }

    commitChapter();

    // Fallback if document has no headings
    if (chapters.isEmpty()) {
        ChapterContent fallbackChapter;
        fallbackChapter.index = 0;
        fallbackChapter.title = !docTitle.trimmed().isEmpty() ? docTitle.trimmed() : fileName;
        fallbackChapter.textContent = currentChapter.textContent.trimmed();
        fallbackChapter.htmlContent = currentChapter.htmlContent.trimmed();
        chapters.append(fallbackChapter);

        TocItem tocItem;
        tocItem.id = QStringLiteral("toc_0");
        tocItem.title = fallbackChapter.title;
        tocItem.chapterIndex = 0;
        tocItem.level = 1;
        toc.append(tocItem);
    }

    BookDocument doc;
    doc.id = docId;
    doc.format = QStringLiteral("odt");
    doc.title = !docTitle.trimmed().isEmpty() ? docTitle.trimmed() : fileName;
    doc.author = !docAuthor.trimmed().isEmpty() ? docAuthor.trimmed() : QStringLiteral("Unknown Author");
    doc.chapters = chapters;
    doc.toc = toc;
    doc.totalChapters = static_cast<int>(chapters.size());
    doc.coverImageBytes = coverBytes;

    return doc;
}

} // namespace LunaRead
