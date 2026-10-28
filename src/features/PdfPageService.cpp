#include "PdfPageService.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QThreadPool>
#include <QRegularExpression>
#include <algorithm>

namespace LunaRead {

PdfPageService& PdfPageService::instance() {
    static PdfPageService s_instance;
    return s_instance;
}

QString PdfPageService::getCacheDir(const QString& bookId, const QString& filePath) {
    QString id = bookId;
    if (id.isEmpty() && !filePath.isEmpty()) {
        id = HashUtils::calculateFileMd5(filePath);
    }
    if (id.isEmpty()) id = "unknown";

    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QDir::tempPath() + "/lunaread_cache";
    }
    QString dir = cacheRoot + "/pdf_pages/" + id;
    QDir().mkpath(dir);
    return dir;
}

int PdfPageService::getPageCount(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) return 0;

    QString pdfinfo = QStandardPaths::findExecutable("pdfinfo");
    if (!pdfinfo.isEmpty()) {
        QProcess proc;
        proc.start(pdfinfo, QStringList() << filePath);
        if (proc.waitForFinished(4000) && proc.exitCode() == 0) {
            QString out = QString::fromUtf8(proc.readAllStandardOutput());
            static const QRegularExpression pageRegex(R"(Pages:\s+(\d+))");
            auto match = pageRegex.match(out);
            if (match.hasMatch()) {
                return match.captured(1).toInt();
            }
        }
    }

    // Fallback: estimate page count via pdftotext form-feeds
    QString pdftotext = QStandardPaths::findExecutable("pdftotext");
    if (!pdftotext.isEmpty()) {
        QProcess proc;
        proc.start(pdftotext, QStringList() << "-f" << "1" << filePath << "-");
        if (proc.waitForFinished(6000)) {
            QByteArray out = proc.readAllStandardOutput();
            int ff = out.count('\x0c');
            if (ff > 0) return ff;
        }
    }

    return 0;
}

QByteArray PdfPageService::getPageImage(const QString& filePath, const QString& bookId, int pageNumber, int dpi) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || pageNumber < 1) {
        return QByteArray();
    }

    QString memKey = QString("%1:%2:%3").arg(bookId, QString::number(pageNumber), QString::number(dpi));

    {
        QMutexLocker locker(&m_mutex);
        if (m_memCache.contains(memKey)) {
            return m_memCache.value(memKey);
        }
    }

    QString cacheDir = getCacheDir(bookId, filePath);
    QString cacheFile = cacheDir + QString("/page_%1_%2dpi.png").arg(pageNumber).arg(dpi);

    // 1. Check disk cache
    if (QFile::exists(cacheFile)) {
        QFile f(cacheFile);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            if (!data.isEmpty()) {
                QMutexLocker locker(&m_mutex);
                m_memCache.insert(memKey, data);
                m_lruKeys.append(memKey);
                if (m_lruKeys.size() > 30) {
                    m_memCache.remove(m_lruKeys.takeFirst());
                }
                return data;
            }
        }
    }

    // 2. Render on demand via pdftoppm
    QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (!pdftoppm.isEmpty()) {
        QString tempPrefix = cacheDir + QString("/ppm_%1").arg(pageNumber);
        QProcess proc;
        proc.start(pdftoppm, QStringList()
            << "-png"
            << "-r" << QString::number(dpi)
            << "-f" << QString::number(pageNumber)
            << "-l" << QString::number(pageNumber)
            << filePath
            << tempPrefix
        );

        if (proc.waitForFinished(10000) && proc.exitCode() == 0) {
            QDir dir(cacheDir);
            QStringList matches = dir.entryList(
                QStringList() << QString("ppm_%1-*.png").arg(pageNumber),
                QDir::Files
            );

            if (!matches.isEmpty()) {
                QString foundFile = cacheDir + "/" + matches.first();
                QFile::remove(cacheFile);
                QFile::rename(foundFile, cacheFile);

                QFile f(cacheFile);
                if (f.open(QIODevice::ReadOnly)) {
                    QByteArray data = f.readAll();
                    if (!data.isEmpty()) {
                        QMutexLocker locker(&m_mutex);
                        m_memCache.insert(memKey, data);
                        m_lruKeys.append(memKey);
                        if (m_lruKeys.size() > 30) {
                            m_memCache.remove(m_lruKeys.takeFirst());
                        }
                        return data;
                    }
                }
            }
        }
    }

    return QByteArray();
}

void PdfPageService::prefetchPage(const QString& filePath, const QString& bookId, int pageNumber, int dpi) {
    if (filePath.isEmpty() || pageNumber < 1) return;

    QString cacheDir = getCacheDir(bookId, filePath);
    QString cacheFile = cacheDir + QString("/page_%1_%2dpi.png").arg(pageNumber).arg(dpi);
    if (QFile::exists(cacheFile)) return;

    QThreadPool::globalInstance()->start([this, filePath, bookId, pageNumber, dpi]() {
        getPageImage(filePath, bookId, pageNumber, dpi);
    });
}

void PdfPageService::clearMemoryCache() {
    QMutexLocker locker(&m_mutex);
    m_memCache.clear();
    m_lruKeys.clear();
}

} // namespace LunaRead
