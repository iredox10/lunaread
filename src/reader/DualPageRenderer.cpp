#include "DualPageRenderer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>

namespace LunaRead {

DualPageRenderer::DualPageRenderer(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
}

void DualPageRenderer::setPages(const ParsedPage& left, const std::optional<ParsedPage>& right, const ReaderThemeTokens& tokens) {
    m_leftPage = left;
    m_rightPage = right;
    m_tokens = tokens;
    update();
}

void DualPageRenderer::updateTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    update();
}

void DualPageRenderer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    qreal w = width();
    qreal h = height();
    qreal halfW = w / 2.0;

    QRectF leftRect(0, 0, halfW, h);
    QRectF rightRect(halfW, 0, halfW, h);

    // Left Page
    painter.fillRect(leftRect, m_tokens.backgroundColor);
    if (m_leftPage.isImagePage && !m_leftPage.imageBytes.isEmpty()) {
        QPixmap pm;
        pm.loadFromData(m_leftPage.imageBytes);
        if (!pm.isNull()) {
            QPixmap scaled = pm.scaled(leftRect.adjusted(20, 20, -20, -20).size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            qreal x = leftRect.left() + (leftRect.width() - scaled.width()) / 2.0;
            qreal y = leftRect.top() + (leftRect.height() - scaled.height()) / 2.0;
            painter.drawPixmap(QPointF(x, y), scaled);
        }
    } else {
        painter.setPen(m_tokens.textColor);
        painter.setFont(QFont("Roboto", 13));
        painter.drawText(leftRect.adjusted(30, 30, -30, -30), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_leftPage.text);
    }

    // Right Page
    painter.fillRect(rightRect, m_tokens.backgroundColor);
    if (m_rightPage.has_value()) {
        if (m_rightPage->isImagePage && !m_rightPage->imageBytes.isEmpty()) {
            QPixmap pm;
            pm.loadFromData(m_rightPage->imageBytes);
            if (!pm.isNull()) {
                QPixmap scaled = pm.scaled(rightRect.adjusted(20, 20, -20, -20).size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                qreal x = rightRect.left() + (rightRect.width() - scaled.width()) / 2.0;
                qreal y = rightRect.top() + (rightRect.height() - scaled.height()) / 2.0;
                painter.drawPixmap(QPointF(x, y), scaled);
            }
        } else {
            painter.setPen(m_tokens.textColor);
            painter.setFont(QFont("Roboto", 13));
            painter.drawText(rightRect.adjusted(30, 30, -30, -30), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_rightPage->text);
        }
    }

    // Spine Gutter Valley Shadow in the center
    QLinearGradient gutterGrad(halfW - 20.0, 0, halfW + 20.0, 0);
    gutterGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
    gutterGrad.setColorAt(0.5, QColor(0, 0, 0, m_tokens.isDark ? 90 : 50));
    gutterGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(QRectF(halfW - 20.0, 0, 40.0, h), gutterGrad);

    // Central Crease Line
    painter.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
    painter.drawLine(QPointF(halfW, 0), QPointF(halfW, h));
}

void DualPageRenderer::mousePressEvent(QMouseEvent* event) {
    if (event->position().x() > width() * 0.6) {
        emit pageTurnRequested(2); // Turn forward 2 pages
    } else if (event->position().x() < width() * 0.4) {
        emit pageTurnRequested(-2); // Turn backward 2 pages
    }
}

} // namespace LunaRead
