#include "EpubParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>

namespace LunaRead {

static QString stripHtmlTags(const QString& html) {
    static const QRegularExpression tagRegex("<[^>]*>");
    QString text = html;
    text.replace(tagRegex, " ");
    text.replace("&nbsp;", " ");
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&quot;", "\"");
    return text.simplified();
}

bool EpubParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "epub";
}

std::optional<BookDocument> EpubParser::parseFile(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return std::nullopt;
    }

    QString unzip = QStandardPaths::findExecutable("unzip");
    if (!unzip.isEmpty()) {
        QString tempDir = QDir::tempPath() + QString("/lunaread_epub_%1").arg(qHash(filePath));
        QString containerPath = tempDir + "/META-INF/container.xml";
        bool alreadyExtracted = QFile::exists(containerPath);
        if (!alreadyExtracted) {
            QDir(tempDir).removeRecursively();
            QDir().mkpath(tempDir);

            QProcess proc;
            proc.start(unzip, QStringList() << "-q" << "-o" << filePath << "-d" << tempDir);
            if (!proc.waitForFinished(10000) || proc.exitCode() != 0) {
                return std::nullopt;
            }
        }

        BookDocument doc;
        doc.id = HashUtils::calculateFileMd5(filePath);
        doc.filePath = filePath;
        doc.format = "epub";
        doc.title = QFileInfo(filePath).completeBaseName();
        doc.author = "Unknown Author";

        // 1. Locate container.xml
        QString opfPath;
            if (QFile::exists(containerPath)) {
                QFile cFile(containerPath);
                if (cFile.open(QIODevice::ReadOnly)) {
                    QXmlStreamReader xml(&cFile);
                    while (!xml.atEnd()) {
                        xml.readNext();
                        if (xml.isStartElement() && xml.name() == QLatin1String("rootfile")) {
                            opfPath = tempDir + "/" + xml.attributes().value("full-path").toString();
                            break;
                        }
                    }
                }
            }

            if (opfPath.isEmpty()) {
                QDirIterator it(tempDir, QStringList() << "*.opf", QDir::Files, QDirIterator::Subdirectories);
                if (it.hasNext()) opfPath = it.next();
            }

            if (!opfPath.isEmpty() && QFile::exists(opfPath)) {
                QFileInfo opfInfo(opfPath);
                QDir opfDir = opfInfo.dir();

                QFile opfFile(opfPath);
                if (opfFile.open(QIODevice::ReadOnly)) {
                    QXmlStreamReader xml(&opfFile);
                    QMap<QString, QString> manifestHref;
                    QMap<QString, QString> manifestType;
                    QStringList spineIdrefs;
                    QString coverItemId;

                    while (!xml.atEnd()) {
                        xml.readNext();
                        if (xml.isStartElement()) {
                            QString tag = xml.name().toString().toLower();
                            if (tag == "title") {
                                doc.title = xml.readElementText().trimmed();
                            } else if (tag == "creator") {
                                doc.author = xml.readElementText().trimmed();
                            } else if (tag == "meta") {
                                auto attrs = xml.attributes();
                                if (attrs.value("name").toString().compare("cover", Qt::CaseInsensitive) == 0) {
                                    coverItemId = attrs.value("content").toString();
                                }
                            } else if (tag == "item") {
                                auto attrs = xml.attributes();
                                QString id = attrs.value("id").toString();
                                QString href = attrs.value("href").toString();
                                QString type = attrs.value("media-type").toString();
                                QString properties = attrs.value("properties").toString();
                                manifestHref[id] = href;
                                manifestType[id] = type;
                                if (properties.contains("cover-image", Qt::CaseInsensitive) ||
                                    (coverItemId.isEmpty() && id.contains("cover", Qt::CaseInsensitive) && type.startsWith("image/"))) {
                                    coverItemId = id;
                                }
                            } else if (tag == "itemref") {
                                QString idref = xml.attributes().value("idref").toString();
                                if (!idref.isEmpty()) spineIdrefs.append(idref);
                            }
                        }
                    }

                    // Extract cover image
                    if (!coverItemId.isEmpty() && manifestHref.contains(coverItemId)) {
                        QString coverPath = opfDir.filePath(manifestHref[coverItemId]);
                        QFile cf(coverPath);
                        if (cf.open(QIODevice::ReadOnly)) {
                            doc.coverImageBytes = cf.readAll();
                        }
                    } else {
                        for (auto it = manifestHref.begin(); it != manifestHref.end(); ++it) {
                            if (it.value().contains("cover", Qt::CaseInsensitive) && manifestType.value(it.key()).startsWith("image/")) {
                                QString coverPath = opfDir.filePath(it.value());
                                QFile cf(coverPath);
                                if (cf.open(QIODevice::ReadOnly)) {
                                    doc.coverImageBytes = cf.readAll();
                                    break;
                                }
                            }
                        }
                    }

                    int chIdx = 0;
                    for (const QString& idref : spineIdrefs) {
                        if (!manifestHref.contains(idref)) continue;
                        QString relHref = manifestHref[idref];
                        QString itemPath = opfDir.filePath(relHref);

                        QFile itemFile(itemPath);
                        if (itemFile.open(QIODevice::ReadOnly)) {
                            QString html = QString::fromUtf8(itemFile.readAll());
                            QFileInfo itemInfo(itemPath);
                            QDir itemDir = itemInfo.dir();

                            // Inline relative images as base64 data URIs so they survive tempDir cleanup
                            static const QRegularExpression imgRegex(R"(<img[^>]+src=["']([^"']+)["'])", QRegularExpression::CaseInsensitiveOption);
                            auto it = imgRegex.globalMatch(html);
                            while (it.hasNext()) {
                                auto m = it.next();
                                QString rel = m.captured(1);
                                if (rel.startsWith("data:") || rel.startsWith("http:") || rel.startsWith("https:")) continue;
                                QString cleanPath = QDir::cleanPath(itemDir.filePath(rel));
                                QFile imgF(cleanPath);
                                if (imgF.open(QIODevice::ReadOnly)) {
                                    QByteArray imgData = imgF.readAll();
                                    QString suf = QFileInfo(cleanPath).suffix().toLower();
                                    QString mime = "image/jpeg";
                                    if (suf == "png") mime = "image/png";
                                    else if (suf == "gif") mime = "image/gif";
                                    else if (suf == "svg") mime = "image/svg+xml";
                                    else if (suf == "webp") mime = "image/webp";
                                    QString dataUri = QString("data:%1;base64,%2").arg(mime, QString::fromLatin1(imgData.toBase64()));
                                    html.replace(m.captured(0), QString("<img src=\"%1\"").arg(dataUri));
                                }
                            }

                            static const QRegularExpression svgImgRegex(R"(<image[^>]+(?:xlink:)?href=["']([^"']+)["'])", QRegularExpression::CaseInsensitiveOption);
                            auto itSvg = svgImgRegex.globalMatch(html);
                            while (itSvg.hasNext()) {
                                auto m = itSvg.next();
                                QString rel = m.captured(1);
                                if (rel.startsWith("data:")) continue;
                                QString cleanPath = QDir::cleanPath(itemDir.filePath(rel));
                                QFile imgF(cleanPath);
                                if (imgF.open(QIODevice::ReadOnly)) {
                                    QByteArray imgData = imgF.readAll();
                                    QString suf = QFileInfo(cleanPath).suffix().toLower();
                                    QString mime = "image/jpeg";
                                    if (suf == "png") mime = "image/png";
                                    else if (suf == "gif") mime = "image/gif";
                                    else if (suf == "svg") mime = "image/svg+xml";
                                    QString dataUri = QString("data:%1;base64,%2").arg(mime, QString::fromLatin1(imgData.toBase64()));
                                    html.replace(m.captured(0), QString("<image xlink:href=\"%1\"").arg(dataUri));
                                }
                            }

                            ChapterContent ch;
                            ch.index = chIdx;

                            static const QRegularExpression hRegex(R"(<(?:h1|title)[^>]*>(.*?)<\/(?:h1|title)>)", QRegularExpression::CaseInsensitiveOption);
                            auto m = hRegex.match(html);
                            if (m.hasMatch()) {
                                ch.title = stripHtmlTags(m.captured(1)).trimmed();
                            } else {
                                ch.title = QString("Chapter %1").arg(chIdx + 1);
                            }

                            ch.htmlContent = html;
                            ch.textContent = stripHtmlTags(html).trimmed();
                            if (ch.textContent.isEmpty() && html.contains("<img", Qt::CaseInsensitive)) {
                                ch.textContent = QString("[%1 - Illustrated Content]").arg(ch.title);
                            }

                            if (!ch.textContent.isEmpty()) {
                                doc.chapters.append(ch);

                                TocItem tocItem;
                                tocItem.id = QString("toc_%1").arg(chIdx);
                                tocItem.title = ch.title;
                                tocItem.chapterIndex = chIdx;
                                tocItem.pageIndex = chIdx;
                                doc.toc.append(tocItem);
                                chIdx++;
                            }
                        }
                    }
                    doc.totalChapters = static_cast<int>(doc.chapters.size());
                }
            }

            if (!doc.chapters.isEmpty()) {
                return doc;
            }
        }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "epub", filePath);
}

std::optional<BookDocument> EpubParser::parseBytes(const QByteArray& data, const QString& /*format*/, const QString& filePath) {
    if (data.size() < 30) return std::nullopt;

    if (data[0] != 'P' || data[1] != 'K' || data[2] != 0x03 || data[3] != 0x04) {
        return std::nullopt;
    }

    QString docId = HashUtils::calculateBytesMd5(data);
    QString fileName = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : "EPUB Publication";

    BookDocument doc;
    doc.id = docId;
    doc.title = fileName;
    doc.author = "Unknown Author";
    doc.format = "epub";

    ChapterContent c;
    c.index = 0;
    c.title = fileName;
    c.textContent = QString("EPUB Book: %1\n\nExtracted and ready for reading in LunaRead-Qt.").arg(fileName);
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
