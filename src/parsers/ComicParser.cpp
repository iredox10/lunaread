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

std::optional<BookDocument> ComicParser::parseBytes(const QByteArray& data, const QString& format, const QString& filePath) {
    Q_UNUSED(format);
    if (data.size() < 30) return std::nullopt;

    const QString displayName = filePath.isEmpty()
        ? QStringLiteral("Comic Book")
        : QFileInfo(filePath).completeBaseName();

    QTemporaryFile tempFile;
    if (tempFile.open()) {
        tempFile.write(data);
        tempFile.flush();
        if (auto doc = parseArchive(tempFile.fileName(), displayName)) {
            return doc;
        }
    }

    return fallbackDocument(data, filePath);
}

std::optional<BookDocument> ComicParser::parseArchive(const QString& archivePath, const QString& displayName) {
    QString unzip = QStandardPaths::findExecutable("unzip");
    if (unzip.isEmpty()) {
        return std::nullopt;
    }

    QString tempDir = QDir::tempPath() + QString("/lunaread_comic_%1").arg(qHash(archivePath));
    QDir(tempDir).removeRecursively();
    QDir().mkpath(tempDir);

    QProcess proc;
    proc.start(unzip, QStringList() << "-q" << "-o" << archivePath << "-d" << tempDir);
    if (!proc.waitForFinished(15000) || proc.exitCode() != 0) {
        return std::nullopt;
    }

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

    if (imagePaths.isEmpty()) {
        return std::nullopt;
    }

    BookDocument doc;
    doc.id = HashUtils::calculateFileMd5(archivePath);
    doc.title = displayName;
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

    doc.coverImageBytes = doc.chapters[0].imageBytes;
    doc.totalChapters = static_cast<int>(doc.chapters.size());

    return doc;
}

BookDocument ComicParser::fallbackDocument(const QByteArray& data, const QString& filePath) {
    const QString fileName = filePath.isEmpty()
        ? QStringLiteral("Comic Book")
        : QFileInfo(filePath).completeBaseName();

    BookDocument doc;
    doc.id = HashUtils::calculateBytesMd5(data);
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
