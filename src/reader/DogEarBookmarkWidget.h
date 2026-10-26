#pragma once

#include <QWidget>
#include <QVariantAnimation>
#include <QColor>
#include <QPainterPath>
#include <QEnterEvent>
#include "../core/ThemeTokens.h"

namespace LunaRead {

/**
 * @brief Visual presentation style of the corner bookmark
 */
enum class BookmarkStyle {
    Ribbon,         ///< Hanging crimson silk ribbon bookmark with swallowtail notch
    FoldedPaper,    ///< Realistic folded paper corner triangle (dog-ear flap)
    CornerRibbon    ///< Diagonal crimson silk corner ribbon wrap
};

/**
 * @brief Moon+ Reader's iconic Dog-Ear / Corner Bookmark Ribbon.
 *
 * Positioned in the top-right corner of the ReaderViewport (default 48x48 px).
 * - Bookmarked state: Active red silk ribbon or folded dog-ear flap with realistic
 *   soft drop shadows, silk luster sheen gradients, and embroidered stitching.
 * - Unbookmarked state: Subtle grey dashed fold crease line on hover.
 * - Clicking toggles bookmark state, playing a smooth fold/tuck animation
 *   and emitting bookmarkToggled(bool isBookmarked).
 */
class DogEarBookmarkWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool bookmarked READ isBookmarked WRITE setBookmarked NOTIFY bookmarkToggled)
    Q_PROPERTY(qreal foldProgress READ foldProgress WRITE setFoldProgress)
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit DogEarBookmarkWidget(QWidget* parent = nullptr);
    ~DogEarBookmarkWidget() override = default;

    /// Bookmark state inquiry
    [[nodiscard]] bool isBookmarked() const noexcept { return m_bookmarked; }

    /// Bookmark presentation style
    [[nodiscard]] BookmarkStyle style() const noexcept { return m_style; }
    void setStyle(BookmarkStyle style);

    /// Appearance customization
    [[nodiscard]] QColor ribbonColor() const noexcept { return m_ribbonColor; }
    void setRibbonColor(const QColor& color);

    [[nodiscard]] QColor paperColor() const noexcept { return m_paperColor; }
    void setPaperColor(const QColor& color);

    void setTheme(const ReaderThemeTokens& tokens);

    /// Animation progress properties (0.0 to 1.0)
    [[nodiscard]] qreal foldProgress() const noexcept { return m_foldProgress; }
    void setFoldProgress(qreal progress);

    [[nodiscard]] qreal hoverProgress() const noexcept { return m_hoverProgress; }
    void setHoverProgress(qreal progress);

    /// Size hints
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public slots:
    /// Set bookmark state with optional fold animation
    void setBookmarked(bool bookmarked);
    void setBookmarked(bool bookmarked, bool animate);

    /// Toggle bookmark state and emit bookmarkToggled
    void toggle();

signals:
    void bookmarkToggled(bool isBookmarked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void updateTooltip();
    void drawHangingRibbon(QPainter& painter, const QRectF& r, qreal progress);
    void drawFoldedPaperCorner(QPainter& painter, const QRectF& r, qreal progress);
    void drawCornerRibbon(QPainter& painter, const QRectF& r, qreal progress);
    void drawHoverFoldOutline(QPainter& painter, const QRectF& r, qreal hover);

    bool m_bookmarked = false;
    BookmarkStyle m_style = BookmarkStyle::Ribbon;

    qreal m_foldProgress = 0.0;
    qreal m_hoverProgress = 0.0;

    QVariantAnimation* m_foldAnimation = nullptr;
    QVariantAnimation* m_hoverAnimation = nullptr;

    QColor m_ribbonColor{211, 47, 47};     ///< Iconic Moon+ crimson silk (#D32F2F)
    QColor m_paperColor{251, 240, 217};    ///< Page paper color (#FBF0D9)
    bool m_isDarkTheme = false;
};

} // namespace LunaRead
