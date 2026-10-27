#pragma once

#include <QWidget>
#include <QVariantAnimation>
#include <QPixmap>
#include <QRectF>
#include "../core/Types.h"

namespace LunaRead {

enum class ShelfPresentationMode {
    Modern,
    Wooden
};

/**
 * @brief High-performance book card widget with smooth hover elevation lift,
 *        procedural cover rendering, format badge pills, and interactive favorite star.
 */
class BookCardWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal elevation READ elevation WRITE setElevation)
    Q_PROPERTY(qreal shadowIntensity READ shadowIntensity WRITE setShadowIntensity)

public:
    explicit BookCardWidget(const BookItem& book, QWidget* parent = nullptr);
    ~BookCardWidget() override = default;

    const BookItem& book() const { return m_book; }
    void updateBook(const BookItem& book);

    void setPresentationMode(ShelfPresentationMode mode);
    ShelfPresentationMode presentationMode() const { return m_mode; }

    qreal elevation() const { return m_elevation; }
    void setElevation(qreal val);

    qreal shadowIntensity() const { return m_shadowIntensity; }
    void setShadowIntensity(qreal val);

signals:
    void clicked(const QString& bookId);
    void favoriteToggled(const QString& bookId, bool isFavorite);
    void deleteRequested(const QString& bookId);
    void detailsRequested(const QString& bookId);
    void exportRequested(const QString& bookId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void startHoverAnimation(bool hovering);
    QColor formatBadgeColor(const QString& format) const;
    void paintCover(QPainter& p, const QRectF& coverRect);
    void paintCoverFallback(QPainter& p, const QRectF& coverRect);
    void paintFormatBadge(QPainter& p, const QRectF& coverRect);
    void paintFavoriteStar(QPainter& p, const QRectF& coverRect);
    void paintInfoButton(QPainter& p, const QRectF& coverRect);
    void paintProgressBar(QPainter& p, const QRectF& coverRect);

    BookItem m_book;
    ShelfPresentationMode m_mode = ShelfPresentationMode::Modern;

    bool m_isHovered = false;
    bool m_starHovered = false;
    bool m_infoHovered = false;
    qreal m_elevation = 0.0;          // Lift offset in pixels (0.0 to 6.0)
    qreal m_shadowIntensity = 0.0;    // Hover shadow boost (0.0 to 1.0)

    QVariantAnimation* m_hoverAnim = nullptr;
    QPixmap m_cachedCover;
    bool m_coverLoaded = false;
    QRectF m_starRect;
    QRectF m_infoRect;
};

} // namespace LunaRead
