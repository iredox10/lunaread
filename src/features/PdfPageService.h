#pragma once

#include <QString>
#include <QByteArray>
#include <QMap>
#include <QList>
#include <QMutex>
#include <QSizeF>
#include <QRectF>

namespace LunaRead {

class PdfPageService {
public:
    static PdfPageService& instance();

    /// Renders or retrieves a cached high-res page image for a PDF file (1-indexed pageNumber)
    QByteArray getPageImage(const QString& filePath, const QString& bookId, int pageNumber, int dpi = 300);

    /// Total pages in a PDF document
    int getPageCount(const QString& filePath);

    /// Extracts plaintext for a single PDF page (1-indexed) on demand for TTS or search
    QString getPageText(const QString& filePath, int pageNumber);

    /// Gets page dimensions in points (e.g. 612x792) from pdfinfo
    QSizeF getPageSizePoints(const QString& filePath);

    /// Extracts plaintext for a bounded region on a page (normalized coordinates 0.0 .. 1.0)
    QString getPageRegionText(const QString& filePath, int pageNumber, const QRectF& normalizedRect);

    /// Asynchronously prefetch upcoming pages into disk cache
    void prefetchPage(const QString& filePath, const QString& bookId, int pageNumber, int dpi = 300);

    /// Clear memory or disk cache
    void clearMemoryCache();

private:
    PdfPageService() = default;
    ~PdfPageService() = default;
    PdfPageService(const PdfPageService&) = delete;
    PdfPageService& operator=(const PdfPageService&) = delete;

    QString getCacheDir(const QString& bookId, const QString& filePath);

    QMutex m_mutex;
    QMap<QString, QByteArray> m_memCache; // Key: "bookId:pageNum", max 30 entries
    QList<QString> m_lruKeys;
};

} // namespace LunaRead
