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

QByteArray PdfPageService::getCachedPageImage(const QString& filePath, const QString& bookId, int pageNumber, int dpi) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || pageNumber < 1) {
        return QByteArray();
    }

    QString memKey = QString("%1:%2:%3").arg(bookId, QString::number(pageNumber), QString::number(dpi));
    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_memCache.contains(memKey)) {
            return m_memCache.value(memKey);
        }
    }

    QString cacheDir = getCacheDir(bookId, filePath);
    const QStringList candidateFiles = {
        cacheDir + QString("/page_%1_%2dpi.jpg").arg(pageNumber).arg(dpi),
        cacheDir + QString("/page_%1_%2dpi.png").arg(pageNumber).arg(dpi),
        cacheDir + QString("/page_%1_150dpi.jpg").arg(pageNumber),
        cacheDir + QString("/page_%1_150dpi.png").arg(pageNumber),
        cacheDir + QString("/page_%1_200dpi.png").arg(pageNumber)
    };

    for (const auto& candidate : candidateFiles) {
        QFile f(candidate);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            if (!data.isEmpty()) {
                QMutexLocker locker(&m_cacheMutex);
                m_memCache.insert(memKey, data);
                m_lruKeys.append(memKey);
                if (m_lruKeys.size() > 60) {
                    m_memCache.remove(m_lruKeys.takeFirst());
                }
                return data;
            }
        }
    }

    return QByteArray();
}

QByteArray PdfPageService::getPageImage(const QString& filePath, const QString& bookId, int pageNumber, int dpi) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || pageNumber < 1) {
        return QByteArray();
    }

    QString memKey = QString("%1:%2:%3").arg(bookId, QString::number(pageNumber), QString::number(dpi));
    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_memCache.contains(memKey)) {
            return m_memCache.value(memKey);
        }
    }

    QString cacheDir = getCacheDir(bookId, filePath);

    auto loadCachedPage = [this, &memKey, cacheDir, pageNumber, dpi]() -> QByteArray {
        const QStringList candidateFiles = {
            cacheDir + QString("/page_%1_%2dpi.jpg").arg(pageNumber).arg(dpi),
            cacheDir + QString("/page_%1_%2dpi.png").arg(pageNumber).arg(dpi),
            cacheDir + QString("/page_%1_150dpi.jpg").arg(pageNumber),
            cacheDir + QString("/page_%1_150dpi.png").arg(pageNumber),
            cacheDir + QString("/page_%1_200dpi.png").arg(pageNumber)
        };
        for (const auto& candidate : candidateFiles) {
            QFile f(candidate);
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QByteArray data = f.readAll();
            if (data.isEmpty()) continue;
            QMutexLocker locker(&m_cacheMutex);
            m_memCache.insert(memKey, data);
            m_lruKeys.append(memKey);
            if (m_lruKeys.size() > 60) {
                m_memCache.remove(m_lruKeys.takeFirst());
            }
            return data;
        }
        return {};
    };

    // 1. Check disk cache
    if (const QByteArray data = loadCachedPage(); !data.isEmpty()) {
        return data;
    }

    // 2. Concurrency control per page: allow different pages to rasterize concurrently
    QString renderKey = QString("%1:%2:%3").arg(bookId, QString::number(pageNumber), QString::number(dpi));
    {
        QMutexLocker locker(&m_renderMutex);
        while (m_activeRenders.contains(renderKey)) {
            m_renderWaitCond.wait(&m_renderMutex, 100);
        }
        m_activeRenders.insert(renderKey);
    }

    struct RenderGuard {
        QMutex& m;
        QWaitCondition& wc;
        QSet<QString>& set;
        QString key;
        ~RenderGuard() {
            QMutexLocker locker(&m);
            set.remove(key);
            wc.wakeAll();
        }
    } guard{m_renderMutex, m_renderWaitCond, m_activeRenders, renderKey};

    // Re-check cache in case another worker just finished it
    if (const QByteArray data = loadCachedPage(); !data.isEmpty()) {
        return data;
    }

    // 3. Ultra-fast JPEG rasterization (~75ms per page vs 1,760ms PNG)
    QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (!pdftoppm.isEmpty()) {
        QString prefix = cacheDir + QString("/page_%1_%2dpi").arg(pageNumber).arg(dpi);
        QProcess proc;
        proc.start(pdftoppm, QStringList()
            << "-jpeg"
            << "-jpegopt" << "quality=95"
            << "-r" << QString::number(dpi)
            << "-singlefile"
            << "-aa" << "yes"
            << "-aaVector" << "yes"
            << "-f" << QString::number(pageNumber)
            << "-l" << QString::number(pageNumber)
            << filePath
            << prefix
        );

        if (proc.waitForFinished(8000) && proc.exitCode() == 0) {
            return loadCachedPage();
        }
    }

    return QByteArray();
}

void PdfPageService::prefetchPage(const QString& filePath, const QString& bookId, int pageNumber, int dpi) {
    if (filePath.isEmpty() || pageNumber < 1) return;

    QString cacheDir = getCacheDir(bookId, filePath);
    QString jpgFile = cacheDir + QString("/page_%1_%2dpi.jpg").arg(pageNumber).arg(dpi);
    QString pngFile = cacheDir + QString("/page_%1_%2dpi.png").arg(pageNumber).arg(dpi);
    if (QFile::exists(jpgFile) || QFile::exists(pngFile)) return;

    QThreadPool::globalInstance()->start([this, filePath, bookId, pageNumber, dpi]() {
        getPageImage(filePath, bookId, pageNumber, dpi);
    });
}

void PdfPageService::clearMemoryCache() {
    QMutexLocker locker(&m_cacheMutex);
    m_memCache.clear();
    m_lruKeys.clear();
}

} // namespace LunaRead
