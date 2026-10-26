#include "DogEarBookmarkWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QEasingCurve>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

namespace LunaRead {

DogEarBookmarkWidget::DogEarBookmarkWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_foldAnimation = new QVariantAnimation(this);
    m_foldAnimation->setDuration(260);
    connect(m_foldAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_foldProgress = val.toReal();
        update();
    });

    m_hoverAnimation = new QVariantAnimation(this);
    m_hoverAnimation->setDuration(160);
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_hoverProgress = val.toReal();
        update();
    });

    updateTooltip();
}

QSize DogEarBookmarkWidget::sizeHint() const {
    return {48, 48};
}

QSize DogEarBookmarkWidget::minimumSizeHint() const {
    return {32, 32};
}

void DogEarBookmarkWidget::setStyle(BookmarkStyle newStyle) {
    if (m_style != newStyle) {
        m_style = newStyle;
        update();
    }
}

void DogEarBookmarkWidget::setRibbonColor(const QColor& color) {
    if (m_ribbonColor != color) {
        m_ribbonColor = color;
        update();
    }
}

void DogEarBookmarkWidget::setPaperColor(const QColor& color) {
    if (m_paperColor != color) {
        m_paperColor = color;
        update();
    }
}

void DogEarBookmarkWidget::setTheme(const ReaderThemeTokens& tokens) {
    m_paperColor = tokens.backgroundColor;
    m_isDarkTheme = tokens.isDark;
    update();
}

void DogEarBookmarkWidget::setFoldProgress(qreal progress) {
    m_foldProgress = std::clamp(progress, 0.0, 1.0);
    update();
}

void DogEarBookmarkWidget::setHoverProgress(qreal progress) {
    m_hoverProgress = std::clamp(progress, 0.0, 1.0);
    update();
}

void DogEarBookmarkWidget::setBookmarked(bool bookmarked) {
    setBookmarked(bookmarked, true);
}

void DogEarBookmarkWidget::setBookmarked(bool bookmarked, bool animate) {
    if (m_bookmarked == bookmarked) {
        // Ensure visual state matches
        if (!animate && m_foldProgress != (bookmarked ? 1.0 : 0.0)) {
            m_foldAnimation->stop();
            m_foldProgress = bookmarked ? 1.0 : 0.0;
            update();
        }
        return;
    }

    m_bookmarked = bookmarked;
    updateTooltip();

    if (animate) {
        m_foldAnimation->stop();
        m_foldAnimation->setStartValue(m_foldProgress);
        m_foldAnimation->setEndValue(bookmarked ? 1.0 : 0.0);
        m_foldAnimation->setEasingCurve(bookmarked ? QEasingCurve::OutBack : QEasingCurve::InCubic);
        m_foldAnimation->setDuration(bookmarked ? 280 : 220);
        m_foldAnimation->start();
    } else {
        m_foldAnimation->stop();
        m_foldProgress = bookmarked ? 1.0 : 0.0;
        update();
    }
}

void DogEarBookmarkWidget::toggle() {
    setBookmarked(!m_bookmarked, true);
    emit bookmarkToggled(m_bookmarked);
}

void DogEarBookmarkWidget::updateTooltip() {
    setToolTip(m_bookmarked ? tr("Remove bookmark") : tr("Bookmark page"));
}

void DogEarBookmarkWidget::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    if (!m_bookmarked) {
        m_hoverAnimation->stop();
        m_hoverAnimation->setStartValue(m_hoverProgress);
        m_hoverAnimation->setEndValue(1.0);
        m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
        m_hoverAnimation->start();
    }
}

void DogEarBookmarkWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (m_hoverProgress > 0.0) {
        m_hoverAnimation->stop();
        m_hoverAnimation->setStartValue(m_hoverProgress);
        m_hoverAnimation->setEndValue(0.0);
        m_hoverAnimation->setEasingCurve(QEasingCurve::InCubic);
        m_hoverAnimation->start();
    }
}

void DogEarBookmarkWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void DogEarBookmarkWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        toggle();
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void DogEarBookmarkWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF r = rect();

    // 1. Draw subtle grey fold outline on hover when unbookmarked
    if (!m_bookmarked && m_hoverProgress > 0.001) {
        drawHoverFoldOutline(painter, r, m_hoverProgress);
    }

    // 2. Draw active folded ear / silk ribbon if fold progress > 0
    if (m_foldProgress > 0.001) {
        switch (m_style) {
            case BookmarkStyle::Ribbon:
                drawHangingRibbon(painter, r, m_foldProgress);
                break;
            case BookmarkStyle::FoldedPaper:
                drawFoldedPaperCorner(painter, r, m_foldProgress);
                break;
            case BookmarkStyle::CornerRibbon:
                drawCornerRibbon(painter, r, m_foldProgress);
                break;
        }
    }
}

void DogEarBookmarkWidget::drawHangingRibbon(QPainter& painter, const QRectF& r, qreal progress) {
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal ribbonW = std::clamp(w * 0.44, 16.0, 26.0);
    const qreal rightMargin = 6.0;
    const qreal rx = w - rightMargin - ribbonW;
    const qreal maxLen = h * 0.88;
    const qreal curLen = maxLen * progress;

    if (curLen < 2.0) return;

    const qreal notch = std::min(ribbonW * 0.38, curLen * 0.5);

    QPainterPath ribbonPath;
    ribbonPath.moveTo(rx, 0.0);
    ribbonPath.lineTo(rx + ribbonW, 0.0);
    ribbonPath.lineTo(rx + ribbonW, curLen);
    ribbonPath.lineTo(rx + ribbonW * 0.5, curLen - notch);
    ribbonPath.lineTo(rx, curLen);
    ribbonPath.closeSubpath();

    // Soft realistic drop shadows
    painter.save();
    QTransform t1;
    t1.translate(-1.5, 2.5);
    painter.fillPath(t1.map(ribbonPath), QColor(0, 0, 0, static_cast<int>(35 * progress)));

    QTransform t2;
    t2.translate(-0.8, 1.5);
    painter.fillPath(t2.map(ribbonPath), QColor(0, 0, 0, static_cast<int>(55 * progress)));

    QTransform t3;
    t3.translate(-0.3, 0.8);
    painter.fillPath(t3.map(ribbonPath), QColor(0, 0, 0, static_cast<int>(40 * progress)));
    painter.restore();

    // Silk sheen horizontal gradient
    QLinearGradient silkGrad(rx, 0.0, rx + ribbonW, 0.0);
    const QColor baseRed = m_ribbonColor;
    const QColor darkEdge = baseRed.darker(165);
    const QColor midRed = baseRed.darker(110);
    const QColor sheenRed = baseRed.lighter(135);
    const QColor hotSheen = baseRed.lighter(150);

    silkGrad.setColorAt(0.00, darkEdge);
    silkGrad.setColorAt(0.12, midRed);
    silkGrad.setColorAt(0.40, sheenRed);
    silkGrad.setColorAt(0.55, hotSheen);
    silkGrad.setColorAt(0.70, sheenRed);
    silkGrad.setColorAt(0.90, midRed);
    silkGrad.setColorAt(1.00, darkEdge);

    painter.fillPath(ribbonPath, silkGrad);

    // Vertical fabric luster and woven embroidered stitches
    painter.save();
    painter.setClipPath(ribbonPath);

    QLinearGradient vertGrad(rx, 0.0, rx, curLen);
    vertGrad.setColorAt(0.0, QColor(255, 255, 255, 80));
    vertGrad.setColorAt(0.2, QColor(255, 255, 255, 40));
    vertGrad.setColorAt(0.8, QColor(255, 255, 255, 15));
    vertGrad.setColorAt(1.0, QColor(0, 0, 0, 50));
    painter.fillRect(QRectF(rx, 0.0, ribbonW, curLen), vertGrad);

    if (curLen > 10.0) {
        // Delicate gold stitch lines
        QPen stitchPen(QColor(255, 215, 0, static_cast<int>(140 * progress)), 0.8, Qt::DotLine);
        painter.setPen(stitchPen);
        painter.drawLine(QPointF(rx + 2.0, 1.0), QPointF(rx + 2.0, curLen - notch * 0.8));
        painter.drawLine(QPointF(rx + ribbonW - 2.0, 1.0), QPointF(rx + ribbonW - 2.0, curLen - notch * 0.8));

        // Refined gold ribbon bar near notch
        if (curLen >= maxLen * 0.75) {
            const qreal barY = curLen - notch - 4.0;
            if (barY > 8.0) {
                const QRectF barRect(rx + 3.0, barY, ribbonW - 6.0, 2.0);
                QLinearGradient goldGrad(barRect.left(), 0.0, barRect.right(), 0.0);
                goldGrad.setColorAt(0.0, QColor(218, 165, 32));
                goldGrad.setColorAt(0.5, QColor(255, 235, 140));
                goldGrad.setColorAt(1.0, QColor(218, 165, 32));
                painter.setPen(Qt::NoPen);
                painter.setBrush(goldGrad);
                painter.drawRoundedRect(barRect, 0.8, 0.8);
            }
        }
    }
    painter.restore();

    // Fine contour stroke
    painter.setPen(QPen(baseRed.darker(170), 0.75));
    painter.drawPath(ribbonPath);
}

void DogEarBookmarkWidget::drawFoldedPaperCorner(QPainter& painter, const QRectF& r, qreal progress) {
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal maxFold = std::min(w, h) * 0.78;
    const qreal foldSize = maxFold * progress;

    if (foldSize < 2.0) return;

    const QPointF A(w - foldSize, 0.0);
    const QPointF B(w, foldSize);
    const QPointF C_prime(w - foldSize, foldSize);
    const QPointF corner(w, 0.0);

    // 1. Under-fold shadow in recessed triangle where paper folded away
    QPainterPath recessPath;
    recessPath.moveTo(A);
    recessPath.lineTo(corner);
    recessPath.lineTo(B);
    recessPath.closeSubpath();

    const QColor recessColor = m_isDarkTheme
        ? QColor(10, 10, 12, static_cast<int>(200 * progress))
        : m_paperColor.darker(122);
    painter.fillPath(recessPath, recessColor);

    // 2. Realistic drop shadow cast by folded flap onto reading page
    QPainterPath flapPath;
    flapPath.moveTo(A);
    flapPath.lineTo(B);
    flapPath.lineTo(C_prime);
    flapPath.closeSubpath();

    painter.save();
    QTransform t1;
    t1.translate(-2.0, 2.5);
    painter.fillPath(t1.map(flapPath), QColor(0, 0, 0, static_cast<int>(40 * progress)));

    QTransform t2;
    t2.translate(-0.8, 1.2);
    painter.fillPath(t2.map(flapPath), QColor(0, 0, 0, static_cast<int>(60 * progress)));
    painter.restore();

    // 3. Flap 3D paper gradient fill
    QLinearGradient flapGrad(A.x() + foldSize * 0.5, A.y() + foldSize * 0.5, C_prime.x(), C_prime.y());
    if (m_isDarkTheme) {
        flapGrad.setColorAt(0.0, m_paperColor.lighter(115));
        flapGrad.setColorAt(0.5, m_paperColor.lighter(135));
        flapGrad.setColorAt(1.0, m_paperColor.lighter(110));
    } else {
        flapGrad.setColorAt(0.0, m_paperColor.darker(110));
        flapGrad.setColorAt(0.5, m_paperColor.lighter(103));
        flapGrad.setColorAt(1.0, m_paperColor.darker(106));
    }
    painter.fillPath(flapPath, flapGrad);

    // 4. Crease curvature highlight along fold line AB
    const QColor creaseHighlight = m_isDarkTheme
        ? QColor(255, 255, 255, static_cast<int>(90 * progress))
        : QColor(255, 255, 255, static_cast<int>(180 * progress));
    painter.setPen(QPen(creaseHighlight, 1.2));
    painter.drawLine(A, B);

    // 5. Crisp flap edge contours along AC' and C'B
    const QColor flapEdgeColor = m_isDarkTheme
        ? QColor(0, 0, 0, static_cast<int>(120 * progress))
        : QColor(0, 0, 0, static_cast<int>(45 * progress));
    painter.setPen(QPen(flapEdgeColor, 0.75));
    painter.drawLine(A, C_prime);
    painter.drawLine(C_prime, B);
}

void DogEarBookmarkWidget::drawCornerRibbon(QPainter& painter, const QRectF& r, qreal progress) {
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal maxCorner = std::min(w, h) * 0.85;
    const qreal cornerSize = maxCorner * progress;

    if (cornerSize < 2.0) return;

    const QPointF P1(w - cornerSize, 0.0);
    const QPointF P2(w, 0.0);
    const QPointF P3(w, cornerSize);

    QPainterPath cornerPath;
    cornerPath.moveTo(P1);
    cornerPath.lineTo(P2);
    cornerPath.lineTo(P3);
    cornerPath.closeSubpath();

    // Soft drop shadow along hypotenuse
    painter.save();
    QTransform t1;
    t1.translate(-2.0, 2.0);
    painter.fillPath(t1.map(cornerPath), QColor(0, 0, 0, static_cast<int>(40 * progress)));

    QTransform t2;
    t2.translate(-0.8, 0.8);
    painter.fillPath(t2.map(cornerPath), QColor(0, 0, 0, static_cast<int>(60 * progress)));
    painter.restore();

    // Diagonal silk gradient
    QLinearGradient silk(w, 0.0, w - cornerSize, cornerSize);
    silk.setColorAt(0.00, m_ribbonColor.darker(150));
    silk.setColorAt(0.25, m_ribbonColor);
    silk.setColorAt(0.55, m_ribbonColor.lighter(135));
    silk.setColorAt(0.75, m_ribbonColor);
    silk.setColorAt(1.00, m_ribbonColor.darker(130));
    painter.fillPath(cornerPath, silk);

    // Crease fold edge highlight
    painter.setPen(QPen(QColor(255, 255, 255, static_cast<int>(130 * progress)), 1.2));
    painter.drawLine(P1, P3);

    // Embroidered golden stitch line inset by 3px
    if (cornerSize > 12.0) {
        painter.setPen(QPen(QColor(255, 215, 0, static_cast<int>(160 * progress)), 0.8, Qt::DotLine));
        painter.drawLine(QPointF(P1.x() + 3.0, P1.y() + 3.0), QPointF(P3.x() - 3.0, P3.y() - 3.0));
    }
}

void DogEarBookmarkWidget::drawHoverFoldOutline(QPainter& painter, const QRectF& r, qreal hover) {
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal hoverFold = std::clamp(std::min(w, h) * 0.62, 18.0, 30.0);

    const QPointF H1(w - hoverFold, 0.0);
    const QPointF H2(w, 0.0);
    const QPointF H3(w, hoverFold);

    QPainterPath hoverPath;
    hoverPath.moveTo(H1);
    hoverPath.lineTo(H2);
    hoverPath.lineTo(H3);
    hoverPath.closeSubpath();

    // Translucent corner tint
    const QColor fillColor = m_isDarkTheme
        ? QColor(255, 255, 255, static_cast<int>(20 * hover))
        : QColor(100, 100, 100, static_cast<int>(18 * hover));
    painter.fillPath(hoverPath, fillColor);

    // Subtle grey dashed crease line
    const QColor penColor = m_isDarkTheme
        ? QColor(200, 200, 200, static_cast<int>(160 * hover))
        : QColor(120, 120, 120, static_cast<int>(150 * hover));
    painter.setPen(QPen(penColor, 1.2, Qt::DashLine));
    painter.drawLine(H1, H3);

    // Delicate subtle fold plus hint
    const qreal cx = w - hoverFold * 0.42;
    const qreal cy = hoverFold * 0.42;
    const qreal arm = 2.5;
    painter.setPen(QPen(penColor, 1.0, Qt::SolidLine));
    painter.drawLine(QPointF(cx - arm, cy), QPointF(cx + arm, cy));
    painter.drawLine(QPointF(cx, cy - arm), QPointF(cx, cy + arm));
}

} // namespace LunaRead
