#pragma once

#include <QString>
#include <QByteArray>
#include <QMap>
#include <QSet>
#include <QList>
#include <QMutex>
#include <QRecursiveMutex>
#include <QWaitCondition>
#include <QSizeF>
#include <QRectF>

namespace LunaRead {

struct PdfLink {
    QRectF normalizedRect; // 0.0 .. 1.0 (relative to page dimensions)
    QString target;        // e.g. "https://arxiv.org/abs/1706.03762" or "#23"
    int targetPage = -1;   // 1-indexed page number if internal jump, or -1 if URL
};

struct PdfWord {
    QRectF normalizedRect; // 0.0 .. 1.0
    QString text;
    int lineIndex = 0;
};

struct PdfTextSelection {
    int startWordIndex = -1;
    int endWordIndex = -1;
    QString text;
    QVector<QRectF> normalizedLineRects;
    QPointF startHandle;
    QPointF endHandle;
    QRectF enclosingRect;
    bool isValid() const noexcept { return !text.isEmpty() && !normalizedLineRects.isEmpty(); }
};

class PdfPageService {
public:
    static PdfPageService& instance();

    /// Renders or retrieves a cached page image for a PDF file (1-indexed pageNumber)
    QByteArray getPageImage(const QString& filePath, const QString& bookId, int pageNumber, int dpi = 150);

    /// Retrieves a cached page image if it exists in memory or disk cache; returns empty immediately if not cached (non-blocking)
    QByteArray getCachedPageImage(const QString& filePath, const QString& bookId, int pageNumber, int dpi = 150);

    /// Total pages in a PDF document
    int getPageCount(const QString& filePath);

    /// Extracts plaintext for a single PDF page (1-indexed) on demand for TTS or search
    QString getPageText(const QString& filePath, int pageNumber);

    /// Gets page dimensions in points (e.g. 612x792) from pdfinfo
    QSizeF getPageSizePoints(const QString& filePath);

    /// Extracts plaintext for a bounded region on a page (normalized coordinates 0.0 .. 1.0)
    QString getPageRegionText(const QString& filePath, int pageNumber, const QRectF& normalizedRect);

    /// Retrieves interactive links on the page (both web URLs and internal document jumps)
    QVector<PdfLink> getPageLinks(const QString& filePath, int pageNumber);

    /// Queries if a link exists at the given normalized coordinate (0.0 .. 1.0) on the page
    PdfLink linkAt(const QString& filePath, int pageNumber, const QPointF& normalizedPos);

    /// Retrieves word-level layout for the page with normalized coordinates (0.0 .. 1.0)
    QVector<PdfWord> getPageWords(const QString& filePath, int pageNumber);

    /// Selects words on a page between two normalized positions (0.0 .. 1.0)
    PdfTextSelection selectWords(const QString& filePath, int pageNumber, const QPointF& startNormPos, const QPointF& endNormPos);

    /// Selects the word at the given normalized position (0.0 .. 1.0)
    PdfTextSelection selectWordAt(const QString& filePath, int pageNumber, const QPointF& normPos);

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

    QRecursiveMutex m_cacheMutex;
    QMutex m_renderMutex;
    QWaitCondition m_renderWaitCond;
    QSet<QString> m_activeRenders;
    QMap<QString, QByteArray> m_memCache; // Key: "bookId:pageNum:dpi", max 60 entries
    QList<QString> m_lruKeys;
    QMap<QString, QVector<PdfLink>> m_linkCache; // Key: "filePath:pageNum"
    QMap<QString, QVector<PdfWord>> m_wordsCache; // Key: "filePath:pageNum"
};

} // namespace LunaRead
