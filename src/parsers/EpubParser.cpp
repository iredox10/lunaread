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
        QDir(tempDir).removeRecursively();
        QDir().mkpath(tempDir);

        QProcess proc;
        proc.start(unzip, QStringList() << "-q" << "-o" << filePath << "-d" << tempDir);
        if (proc.waitForFinished(10000) && proc.exitCode() == 0) {
            BookDocument doc;
            doc.id = HashUtils::calculateFileMd5(filePath);
            doc.format = "epub";
            doc.title = QFileInfo(filePath).completeBaseName();
            doc.author = "Unknown Author";

            // 1. Locate container.xml
            QString containerPath = tempDir + "/META-INF/container.xml";
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

                    while (!xml.atEnd()) {
                        xml.readNext();
                        if (xml.isStartElement()) {
                            QString tag = xml.name().toString().toLower();
                            if (tag == "title") {
                                doc.title = xml.readElementText().trimmed();
                            } else if (tag == "creator") {
                                doc.author = xml.readElementText().trimmed();
                            } else if (tag == "item") {
                                auto attrs = xml.attributes();
                                QString id = attrs.value("id").toString();
                                QString href = attrs.value("href").toString();
                                QString type = attrs.value("media-type").toString();
                                manifestHref[id] = href;
                                manifestType[id] = type;
                            } else if (tag == "itemref") {
                                QString idref = xml.attributes().value("idref").toString();
                                if (!idref.isEmpty()) spineIdrefs.append(idref);
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

            QDir(tempDir).removeRecursively();

            if (!doc.chapters.isEmpty()) {
                return doc;
            }
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
