#pragma once

#include <QFont>
#include <QHash>
#include <QMarginsF>
#include <QPixmap>
#include <QSizeF>
#include <QVector>
#include <QTextDocument>
#include <QPainter>
#include <memory>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class Typesetter {
public:
    enum class PageFitMode {
        FitPage,    // Standard: Fit page height & width inside viewport preserving aspect ratio
        FitWidth,   // Zoom to fit full viewport width (100% width, no side margins)
        FillScreen  // Zoom to fill entire screen (aspect fill)
    };

    Typesetter();
    ~Typesetter() = default;

    void setFont(const QFont& font);
    void setLineHeight(qreal multiplier);
    void setParagraphSpacing(qreal spacing);
    void setMargins(const QMarginsF& margins);
    void setParagraphIndent(qreal indentPx);
    void setAlignment(Qt::Alignment alignment);
    void setDropCapsEnabled(bool enabled) { m_dropCaps = enabled; invalidateCache(); }

    [[nodiscard]] const QFont& font() const noexcept { return m_font; }
    [[nodiscard]] qreal lineHeight() const noexcept { return m_lineHeight; }
    [[nodiscard]] qreal paragraphSpacing() const noexcept { return m_paragraphSpacing; }
    [[nodiscard]] const QMarginsF& margins() const noexcept { return m_margins; }
    [[nodiscard]] qreal paragraphIndent() const noexcept { return m_paragraphIndent; }
    [[nodiscard]] Qt::Alignment alignment() const noexcept { return m_alignment; }
    [[nodiscard]] bool dropCapsEnabled() const noexcept { return m_dropCaps; }
    void setFitMode(PageFitMode mode) { m_fitMode = mode; }
    [[nodiscard]] PageFitMode fitMode() const noexcept { return m_fitMode; }

    /// Vertical pan (in pixels) applied to image/fixed-layout pages that are taller than the viewport.
    void setImagePanY(qreal offset) { m_imagePanY = offset; }
    [[nodiscard]] qreal imagePanY() const noexcept { return m_imagePanY; }

    /// Viewport rect actually occupied by an image page, honouring fit mode and vertical pan.
    [[nodiscard]] QRectF imageTargetRect(const QRectF& pageBounds, const QSize& imgSize) const;

    /// How far an image page can be panned vertically at the current fit mode.
    [[nodiscard]] qreal imagePanRange(const ParsedPage& page, const QRectF& pageBounds);

    /// Decoded, margin-cropped page raster (cached between repaints).
    [[nodiscard]] QPixmap preparedPagePixmap(int chapterIndex, const QByteArray& imageBytes);

    /// Blends a scanned page into the active theme: light themes tint the paper
    /// with the theme background, dark themes invert it so night reading stays legible.
    static void applyPageTint(QPainter* painter, const QRectF& rect, const ReaderThemeTokens& tokens);

    /// Paginates chapter into viewport-constrained pages according to publishing standards.
    QVector<ParsedPage> paginate(
        const ChapterContent& chapter,
        const QSizeF& viewportSize,
        int startGlobalPageIndex = 0,
        const ReaderThemeTokens& tokens = ReaderThemeTokens::parchment()
    );

    /// Renders a single book page according to fine publishing standards (classical margins, running header/footer).
    void renderPage(
        QPainter* painter,
        const QRectF& pageBounds,
        const ParsedPage& page,
        const ReaderThemeTokens& tokens,
        const QString& bookTitle = QString(),
        const QString& chapterTitle = QString(),
        bool isVerso = false,
        const QString& highlightedSentence = QString(),
        const QVector<AnnotationRecord>& annotations = {}
    );

    /// Formats raw chapter text into classical book HTML with chapter titles, drop-caps, and indented paragraphs
    static QString formatChapterToBookHtml(const QString& rawText, const QString& title = QString(), bool dropCap = false);

    /// Calculates destination sub-pixel viewport rect for an image page preserving aspect ratio
    static QRectF calculateImageTargetRect(const QRectF& pageBounds, const QSize& imgSize, PageFitMode fitMode = PageFitMode::FitPage);

    /// Extracts text spanning startPos to endPos on a text page
    [[nodiscard]] QString selectedTextForRange(const QPointF& startPos, const QPointF& endPos, const QRectF& pageBounds, const ParsedPage& page) const;

    /// Generates standard publishing CSS stylesheet conforming to font, spacing, and theme palette
    QString generateBookStyleSheet(const ReaderThemeTokens& tokens) const;

    /// Queries if an interactive hyperlink or anchor exists at position pos on the page
    [[nodiscard]] QString anchorAt(const QPointF& pos, const QRectF& pageBounds, const ParsedPage& page) const;

    /// Invalidate layout cache (called when font, margins, or theme changes)
    void invalidateCache();

private:
    struct CachedDocument {
        std::shared_ptr<QTextDocument> doc;
        QSizeF printableSize;
        QString tokensKey;
        QString contentHtml;
    };

    struct CachedPagePixmap {
        QPixmap pixmap;
        const void* dataKey = nullptr;
        qsizetype dataSize = 0;
    };

    std::shared_ptr<QTextDocument> getOrCreateDocument(const ChapterContent& chapter, const QSizeF& printableSize, const ReaderThemeTokens& tokens);
    [[nodiscard]] std::shared_ptr<QTextDocument> documentFor(int chapterIndex) const;

    QFont m_font;
    qreal m_lineHeight = 1.6;
    qreal m_paragraphSpacing = 8.0;
    QMarginsF m_margins{48.0, 44.0, 48.0, 44.0};
    qreal m_paragraphIndent = 24.0; // Standard 1.8em first line indent
    Qt::Alignment m_alignment = Qt::AlignJustify;
    bool m_dropCaps = true;
    PageFitMode m_fitMode = PageFitMode::FitPage;

    // LRU cache of typeset chapter documents so a two-page spread can paint
    // consecutive chapters (PDF pages) without re-laying out either one.
    QHash<int, CachedDocument> m_docCache;
    QList<int> m_docCacheOrder;

    // Decoding a 300 DPI page PNG and scanning its margins is far too slow for
    // every repaint, so the prepared raster is kept around per page.
    QHash<int, CachedPagePixmap> m_pixmapCache;
    QList<int> m_pixmapCacheOrder;
    qreal m_imagePanY = 0.0;
};

} // namespace LunaRead
