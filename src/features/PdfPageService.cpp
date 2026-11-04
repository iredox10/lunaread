#include "PdfPageService.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QThreadPool>
#include <QRegularExpression>
#include <QXmlStreamReader>
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

QVector<PdfLink> PdfPageService::getPageLinks(const QString& filePath, int pageNumber) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || pageNumber < 1) {
        return {};
    }

    const QString cacheKey = QStringLiteral("%1:%2").arg(filePath, QString::number(pageNumber));
    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_linkCache.contains(cacheKey)) {
            return m_linkCache.value(cacheKey);
        }
    }

    QVector<PdfLink> links;
    QString pdftohtml = QStandardPaths::findExecutable(QStringLiteral("pdftohtml"));
    if (!pdftohtml.isEmpty()) {
        QProcess proc;
        proc.start(pdftohtml, QStringList()
            << QStringLiteral("-xml")
            << QStringLiteral("-stdout")
            << QStringLiteral("-f") << QString::number(pageNumber)
            << QStringLiteral("-l") << QString::number(pageNumber)
            << filePath
        );
        if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
            const QString output = QString::fromUtf8(proc.readAllStandardOutput());

            // 1. Extract page dimensions from <page ... height="H" width="W"> (support any attribute order)
            static const QRegularExpression pageTagRegex(R"re(<page\b[^>]*>)re");
            static const QRegularExpression widthAttrRegex(R"re(\bwidth="(\d+)")re");
            static const QRegularExpression heightAttrRegex(R"re(\bheight="(\d+)")re");
            qreal pw = 612.0;
            qreal ph = 792.0;
            auto tagMatch = pageTagRegex.match(output);
            if (tagMatch.hasMatch()) {
                QString tag = tagMatch.captured(0);
                auto wMatch = widthAttrRegex.match(tag);
                auto hMatch = heightAttrRegex.match(tag);
                if (wMatch.hasMatch()) pw = wMatch.captured(1).toDouble();
                if (hMatch.hasMatch()) ph = hMatch.captured(1).toDouble();
            }

            if (pw > 0 && ph > 0) {
                // 2. Extract <text top="T" left="L" width="W" height="H" ...><a href="HREF">
                static const QRegularExpression linkRegex(R"re(<text[^>]+top="(\d+)"[^>]+left="(\d+)"[^>]+width="(\d+)"[^>]+height="(\d+)"[^>]*>.*?<a\s+href="([^"]+))re");
                auto it = linkRegex.globalMatch(output);
                static const QRegularExpression hashPageRegex(R"re(#(?:page=|p)?(\d+)(?:[^\d]|$))re", QRegularExpression::CaseInsensitiveOption);
                while (it.hasNext()) {
                    auto match = it.next();
                    qreal top = match.captured(1).toDouble();
                    qreal left = match.captured(2).toDouble();
                    qreal w = match.captured(3).toDouble();
                    qreal h = match.captured(4).toDouble();
                    QString href = match.captured(5);

                    PdfLink link;
                    link.normalizedRect = QRectF(left / pw, top / ph, w / pw, h / ph);
                    link.target = href;

                    auto hashMatch = hashPageRegex.match(href);
                    if (hashMatch.hasMatch()) {
                        link.targetPage = hashMatch.captured(1).toInt();
                    } else {
                        link.targetPage = -1;
                    }

                    links.append(link);
                }
            }
        }
    }

    {
        QMutexLocker locker(&m_cacheMutex);
        m_linkCache.insert(cacheKey, links);
    }
    return links;
}

PdfLink PdfPageService::linkAt(const QString& filePath, int pageNumber, const QPointF& normalizedPos) {
    const auto links = getPageLinks(filePath, pageNumber);
    for (const auto& l : links) {
        QRectF hitRect = l.normalizedRect.adjusted(-0.012, -0.010, 0.012, 0.010);
        if (hitRect.contains(normalizedPos)) {
            return l;
        }
    }
    return PdfLink();
}

QVector<PdfWord> PdfPageService::getPageWords(const QString& filePath, int pageNumber) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || pageNumber < 1) {
        return {};
    }

    const QString cacheKey = QStringLiteral("%1:%2").arg(filePath, QString::number(pageNumber));
    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_wordsCache.contains(cacheKey)) {
            return m_wordsCache.value(cacheKey);
        }
    }

    QVector<PdfWord> words;
    QString pdftotext = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (!pdftotext.isEmpty()) {
        QProcess proc;
        proc.start(pdftotext, QStringList()
            << QStringLiteral("-bbox-layout")
            << QStringLiteral("-f") << QString::number(pageNumber)
            << QStringLiteral("-l") << QString::number(pageNumber)
            << filePath
            << QStringLiteral("-")
        );
        if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
            QXmlStreamReader xml(proc.readAllStandardOutput());
            qreal pw = 531.0;
            qreal ph = 666.0;
            int currentLineIndex = 0;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement()) {
                    auto name = xml.name();
                    if (name == QLatin1String("page")) {
                        pw = xml.attributes().value(QStringLiteral("width")).toDouble();
                        ph = xml.attributes().value(QStringLiteral("height")).toDouble();
                    } else if (name == QLatin1String("line")) {
                        currentLineIndex++;
                    } else if (name == QLatin1String("word")) {
                        qreal x1 = xml.attributes().value(QStringLiteral("xMin")).toDouble();
                        qreal y1 = xml.attributes().value(QStringLiteral("yMin")).toDouble();
                        qreal x2 = xml.attributes().value(QStringLiteral("xMax")).toDouble();
                        qreal y2 = xml.attributes().value(QStringLiteral("yMax")).toDouble();
                        QString text = xml.readElementText();
                        if (pw > 0 && ph > 0 && !text.isEmpty()) {
                            PdfWord w;
                            w.normalizedRect = QRectF(x1 / pw, y1 / ph, (x2 - x1) / pw, (y2 - y1) / ph);
                            w.text = text;
                            w.lineIndex = currentLineIndex;
                            words.append(w);
                        }
                    }
                }
            }
        }
    }

    {
        QMutexLocker locker(&m_cacheMutex);
        m_wordsCache.insert(cacheKey, words);
    }
    return words;
}

static int findWordIndexAt(const QVector<PdfWord>& words, const QPointF& normPos) {
    if (words.isEmpty()) return -1;

    // 1. Exact hit test with generous padding
    for (int i = 0; i < words.size(); ++i) {
        if (words[i].normalizedRect.adjusted(-0.004, -0.006, 0.004, 0.006).contains(normPos)) {
            return i;
        }
    }

    // 2. Find closest word horizontally on the same line band
    int closestOnLine = -1;
    qreal minDistOnLine = 1e9;
    for (int i = 0; i < words.size(); ++i) {
        const auto& r = words[i].normalizedRect;
        if (normPos.y() >= r.top() - 0.008 && normPos.y() <= r.bottom() + 0.008) {
            qreal dx = 0.0;
            if (normPos.x() < r.left()) dx = r.left() - normPos.x();
            else if (normPos.x() > r.right()) dx = normPos.x() - r.right();
            if (dx < minDistOnLine) {
                minDistOnLine = dx;
                closestOnLine = i;
            }
        }
    }
    if (closestOnLine != -1) return closestOnLine;

    // 3. Fallback: closest word overall
    int closestOverall = 0;
    qreal minOverall = 1e9;
    for (int i = 0; i < words.size(); ++i) {
        const auto& r = words[i].normalizedRect;
        qreal cx = r.center().x();
        qreal cy = r.center().y();
        qreal d2 = (normPos.x() - cx) * (normPos.x() - cx) + (normPos.y() - cy) * (normPos.y() - cy) * 9.0;
        if (d2 < minOverall) {
            minOverall = d2;
            closestOverall = i;
        }
    }
    return closestOverall;
}

PdfTextSelection PdfPageService::selectWords(const QString& filePath, int pageNumber, const QPointF& startNormPos, const QPointF& endNormPos) {
    const auto words = getPageWords(filePath, pageNumber);
    if (words.isEmpty()) return {};

    int w1 = findWordIndexAt(words, startNormPos);
    int w2 = findWordIndexAt(words, endNormPos);
    if (w1 < 0 || w2 < 0) return {};

    int i1 = std::min(w1, w2);
    int i2 = std::max(w1, w2);

    PdfTextSelection sel;
    sel.startWordIndex = i1;
    sel.endWordIndex = i2;

    QString text;
    int curLine = -1;
    QRectF curLineRect;
    QRectF totalEnclosing;

    for (int i = i1; i <= i2; ++i) {
        if (i > i1) {
            if (words[i].lineIndex != words[i - 1].lineIndex) {
                text.append(QLatin1Char('\n'));
            } else {
                text.append(QLatin1Char(' '));
            }
        }
        text.append(words[i].text);

        if (words[i].lineIndex != curLine) {
            if (curLine != -1 && !curLineRect.isNull()) {
                sel.normalizedLineRects.append(curLineRect);
            }
            curLine = words[i].lineIndex;
            curLineRect = words[i].normalizedRect;
        } else {
            curLineRect = curLineRect.united(words[i].normalizedRect);
        }

        totalEnclosing = totalEnclosing.isNull() ? words[i].normalizedRect : totalEnclosing.united(words[i].normalizedRect);
    }

    if (!curLineRect.isNull()) {
        sel.normalizedLineRects.append(curLineRect);
    }

    sel.text = text;
    sel.enclosingRect = totalEnclosing;

    const auto& firstWord = words[i1].normalizedRect;
    const auto& lastWord = words[i2].normalizedRect;
    sel.startHandle = QPointF(firstWord.left(), firstWord.bottom());
    sel.endHandle = QPointF(lastWord.right(), lastWord.bottom());

    return sel;
}

PdfTextSelection PdfPageService::selectWordAt(const QString& filePath, int pageNumber, const QPointF& normPos) {
    const auto words = getPageWords(filePath, pageNumber);
    if (words.isEmpty()) return {};

    int idx = findWordIndexAt(words, normPos);
    if (idx < 0) return {};

    PdfTextSelection sel;
    sel.startWordIndex = idx;
    sel.endWordIndex = idx;
    sel.text = words[idx].text;
    sel.normalizedLineRects.append(words[idx].normalizedRect);
    sel.enclosingRect = words[idx].normalizedRect;
    sel.startHandle = QPointF(words[idx].normalizedRect.left(), words[idx].normalizedRect.bottom());
    sel.endHandle = QPointF(words[idx].normalizedRect.right(), words[idx].normalizedRect.bottom());
    return sel;
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
    m_linkCache.clear();
    m_wordsCache.clear();
}

} // namespace LunaRead
