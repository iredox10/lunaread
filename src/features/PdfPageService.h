#pragma once

#include <QString>
#include <QByteArray>
#include <QMap>
#include <QList>
#include <QMutex>

namespace LunaRead {

class PdfPageService {
public:
    static PdfPageService& instance();

    /// Renders or retrieves a cached high-res page image for a PDF file (1-indexed pageNumber)
    QByteArray getPageImage(const QString& filePath, const QString& bookId, int pageNumber, int dpi = 150);

    /// Total pages in a PDF document
    int getPageCount(const QString& filePath);

    /// Asynchronously prefetch upcoming pages into disk cache
    void prefetchPage(const QString& filePath, const QString& bookId, int pageNumber, int dpi = 150);

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
