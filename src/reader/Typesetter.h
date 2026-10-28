#pragma once

#include <QFont>
#include <QMarginsF>
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
        const QString& highlightedSentence = QString()
    );

    /// Formats raw chapter text into classical book HTML with chapter titles, drop-caps, and indented paragraphs
    static QString formatChapterToBookHtml(const QString& rawText, const QString& title = QString(), bool dropCap = false);

    /// Generates standard publishing CSS stylesheet conforming to font, spacing, and theme palette
    QString generateBookStyleSheet(const ReaderThemeTokens& tokens) const;

    /// Queries if an interactive hyperlink or anchor exists at position pos on the page
    [[nodiscard]] QString anchorAt(const QPointF& pos, const QRectF& pageBounds, const ParsedPage& page) const;

    /// Invalidate layout cache (called when font, margins, or theme changes)
    void invalidateCache();

private:
    std::shared_ptr<QTextDocument> getOrCreateDocument(const ChapterContent& chapter, const QSizeF& printableSize, const ReaderThemeTokens& tokens);

    QFont m_font;
    qreal m_lineHeight = 1.6;
    qreal m_paragraphSpacing = 8.0;
    QMarginsF m_margins{48.0, 44.0, 48.0, 44.0};
    qreal m_paragraphIndent = 24.0; // Standard 1.8em first line indent
    Qt::Alignment m_alignment = Qt::AlignJustify;
    bool m_dropCaps = true;

    // Cache to guarantee 60 FPS smooth rendering without re-parsing HTML
    std::shared_ptr<QTextDocument> m_cachedDoc;
    int m_cachedChapterIndex = -1;
    QSizeF m_cachedPrintableSize;
    QString m_cachedTokensKey;
    QString m_cachedContentHtml;
};

} // namespace LunaRead
