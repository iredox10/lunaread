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

QString PdfPageService::getPageText(const QString& filePath, int pageNumber) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || pageNumber < 1) {
        return QString();
    }
    QString pdftotext = QStandardPaths::findExecutable("pdftotext");
    if (pdftotext.isEmpty()) return QString();

    QProcess proc;
    proc.start(pdftotext, QStringList() << "-f" << QString::number(pageNumber)
                                        << "-l" << QString::number(pageNumber)
                                        << "-layout" << filePath << "-");
    if (proc.waitForFinished(1000)) {
        return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }
    return QString();
}

QSizeF PdfPageService::getPageSizePoints(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return QSizeF(612, 792);
    }
    QString pdfinfo = QStandardPaths::findExecutable("pdfinfo");
    if (!pdfinfo.isEmpty()) {
        QProcess proc;
        proc.start(pdfinfo, QStringList() << filePath);
        if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
            QString out = QString::fromUtf8(proc.readAllStandardOutput());
            static const QRegularExpression sizeRegex(R"(Page\s+size:\s+([\d\.]+)\s+x\s+([\d\.]+))");
            auto match = sizeRegex.match(out);
            if (match.hasMatch()) {
                qreal w = match.captured(1).toDouble();
                qreal h = match.captured(2).toDouble();
                if (w > 0 && h > 0) {
                    return QSizeF(w, h);
                }
            }
        }
    }
    return QSizeF(612, 792);
}

QString PdfPageService::getPageRegionText(const QString& filePath, int pageNumber, const QRectF& normalizedRect) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || pageNumber < 1) {
        return QString();
    }
    QSizeF pts = getPageSizePoints(filePath);
    int x = std::clamp(static_cast<int>(normalizedRect.x() * pts.width()), 0, static_cast<int>(pts.width()));
    int y = std::clamp(static_cast<int>(normalizedRect.y() * pts.height()), 0, static_cast<int>(pts.height()));
    int w = std::clamp(static_cast<int>(normalizedRect.width() * pts.width()), 1, static_cast<int>(pts.width() - x));
    int h = std::clamp(static_cast<int>(normalizedRect.height() * pts.height()), 1, static_cast<int>(pts.height() - y));

    QString pdftotext = QStandardPaths::findExecutable("pdftotext");
    if (!pdftotext.isEmpty()) {
        QProcess proc;
        proc.start(pdftotext, QStringList()
            << "-f" << QString::number(pageNumber)
            << "-l" << QString::number(pageNumber)
            << "-x" << QString::number(x)
            << "-y" << QString::number(y)
            << "-W" << QString::number(w)
            << "-H" << QString::number(h)
            << "-layout"
            << filePath
            << "-"
        );
        if (proc.waitForFinished(1200)) {
            QString res = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            if (!res.isEmpty()) {
                return res;
            }
        }
    }
    return getPageText(filePath, pageNumber);
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

    auto loadCachedPage = [this, &memKey, &cacheFile]() -> QByteArray {
        QFile f(cacheFile);
        if (!f.open(QIODevice::ReadOnly)) return {};
        const QByteArray data = f.readAll();
        if (data.isEmpty()) return {};
        QMutexLocker locker(&m_mutex);
        m_memCache.insert(memKey, data);
        m_lruKeys.append(memKey);
        if (m_lruKeys.size() > 30) {
            m_memCache.remove(m_lruKeys.takeFirst());
        }
        return data;
    };

    // 1. Check disk cache
    if (const QByteArray data = loadCachedPage(); !data.isEmpty()) {
        return data;
    }

    // 2. Render on demand via pdftoppm. The reader and the prefetcher may ask
    // for the same page at once; running one rasterizer at a time keeps their
    // temp output files from colliding.
    QMutexLocker renderLock(&m_renderMutex);
    if (const QByteArray data = loadCachedPage(); !data.isEmpty()) {
        return data;
    }

    QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (!pdftoppm.isEmpty()) {
        QString tempPrefix = cacheDir + QString("/ppm_%1").arg(pageNumber);
        QProcess proc;
        proc.start(pdftoppm, QStringList()
            << "-png"
            << "-r" << QString::number(dpi)
            << "-aa" << "yes"
            << "-aaVector" << "yes"
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
                return loadCachedPage();
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
