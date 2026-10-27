#include "ComicParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QCollator>
#include <algorithm>

namespace LunaRead {

bool ComicParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == "cbz" || ext == "cbr";
}

std::optional<BookDocument> ComicParser::parseFile(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return std::nullopt;
    }

    QString unzip = QStandardPaths::findExecutable("unzip");
    if (!unzip.isEmpty()) {
        QString tempDir = QDir::tempPath() + QString("/lunaread_comic_%1").arg(qHash(filePath));
        QDir(tempDir).removeRecursively();
        QDir().mkpath(tempDir);

        QProcess proc;
        proc.start(unzip, QStringList() << "-q" << "-o" << filePath << "-d" << tempDir);
        if (proc.waitForFinished(15000) && proc.exitCode() == 0) {
            QStringList imagePaths;
            QDirIterator it(tempDir, QStringList() << "*.jpg" << "*.jpeg" << "*.png" << "*.webp" << "*.bmp" << "*.gif", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                imagePaths.append(it.next());
            }

            QCollator collator;
            collator.setNumericMode(true);
            std::sort(imagePaths.begin(), imagePaths.end(), [&](const QString& a, const QString& b) {
                return collator.compare(a, b) < 0;
            });

            if (!imagePaths.isEmpty()) {
                QString fileName = QFileInfo(filePath).completeBaseName();
                BookDocument doc;
                doc.id = HashUtils::calculateFileMd5(filePath);
                doc.title = fileName;
                doc.author = "Comic Creator";
                doc.format = "cbz";

                for (int i = 0; i < imagePaths.size(); ++i) {
                    ChapterContent c;
                    c.index = i;
                    c.title = QString("Page %1").arg(i + 1);
                    c.isImagePage = true;

                    QFile imgFile(imagePaths[i]);
                    if (imgFile.open(QIODevice::ReadOnly)) {
                        c.imageBytes = imgFile.readAll();
                    }
                    doc.chapters.append(c);

                    TocItem item;
                    item.id = QString("toc_%1").arg(i);
                    item.title = c.title;
                    item.chapterIndex = i;
                    doc.toc.append(item);
                }

                if (!doc.chapters.isEmpty()) {
                    doc.coverImageBytes = doc.chapters[0].imageBytes;
                }
                doc.totalChapters = static_cast<int>(doc.chapters.size());

                return doc;
            }
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), "cbz", filePath);
}

std::optional<BookDocument> ComicParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    Q_UNUSED(format);
    if (data.size() < 30) return std::nullopt;

    // Try temporary file unpacking
    QTemporaryFile tempFile;
    if (tempFile.open()) {
        tempFile.write(data);
        tempFile.flush();
        auto docOpt = parseFile(tempFile.fileName());
        if (docOpt.has_value() && !docOpt->chapters.isEmpty()) {
            if (!filePath.isEmpty()) {
                docOpt->title = QFileInfo(filePath).completeBaseName();
            }
            return docOpt;
        }
    }

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
