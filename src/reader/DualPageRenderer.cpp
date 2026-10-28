#include "DualPageRenderer.h"
#include "Typesetter.h"
#include "../features/ImageCropService.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>

namespace LunaRead {

DualPageRenderer::DualPageRenderer(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
}

void DualPageRenderer::setTypesetter(Typesetter* typesetter) {
    m_typesetter = typesetter;
    update();
}

void DualPageRenderer::setBookInfo(const QString& bookTitle, const QString& chapterTitle) {
    m_bookTitle = bookTitle;
    m_chapterTitle = chapterTitle;
    update();
}

void DualPageRenderer::setActiveSentence(const QString& sentence) {
    if (m_activeSentence != sentence) {
        m_activeSentence = sentence;
        update();
    }
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

void DualPageRenderer::setRightToLeft(bool rtl) {
    if (m_rightToLeft != rtl) {
        m_rightToLeft = rtl;
        update();
    }
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

    // In Japanese Manga / RTL mode:
    // Page 1 (m_leftPage) is placed on the RIGHT half.
    // Page 2 (m_rightPage) is placed on the LEFT half.
    const ParsedPage* leftSlotPage = m_rightToLeft ? (m_rightPage.has_value() ? &m_rightPage.value() : nullptr) : &m_leftPage;
    const ParsedPage* rightSlotPage = m_rightToLeft ? &m_leftPage : (m_rightPage.has_value() ? &m_rightPage.value() : nullptr);

    if (m_typesetter) {
        if (leftSlotPage) {
            m_typesetter->renderPage(&painter, leftRect, *leftSlotPage, m_tokens, m_bookTitle, m_chapterTitle, true, m_activeSentence);
        } else {
            painter.fillRect(leftRect, m_tokens.backgroundColor);
        }

        if (rightSlotPage) {
            m_typesetter->renderPage(&painter, rightRect, *rightSlotPage, m_tokens, m_bookTitle, m_chapterTitle, false, m_activeSentence);
        } else {
            painter.fillRect(rightRect, m_tokens.backgroundColor);
        }
    } else {
        auto renderFallbackPage = [&](const QRectF& rect, const ParsedPage* p) {
            painter.fillRect(rect, m_tokens.backgroundColor);
            if (!p) return;

            if (p->isImagePage) {
                if (!p->imageBytes.isEmpty()) {
                    QPixmap pm;
                    pm.loadFromData(p->imageBytes);
                    if (!pm.isNull()) {
                        pm = ImageCropService::instance().cropMargins(pm);
                        QPixmap scaled = pm.scaled(rect.adjusted(20, 20, -20, -20).size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        qreal x = rect.left() + (rect.width() - scaled.width()) / 2.0;
                        qreal y = rect.top() + (rect.height() - scaled.height()) / 2.0;
                        painter.drawPixmap(QPointF(x, y), scaled);
                        return;
                    }
                }
                painter.setPen(m_tokens.headerFooterColor);
                painter.setFont(QFont("Noto Serif", 11));
                painter.drawText(rect, Qt::AlignCenter, QString("Rendering page %1...").arg(p->chapterIndex + 1));
                return;
            } else if (!p->text.isEmpty()) {
                painter.setPen(m_tokens.textColor);
                painter.setFont(QFont("Noto Serif", 13));
                painter.drawText(rect.adjusted(30, 30, -30, -30), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, p->text);
            }
        };

        renderFallbackPage(leftRect, leftSlotPage);
        renderFallbackPage(rightRect, rightSlotPage);
    }

    // Spine Gutter Valley Shadow in the center (simulating open physical book)
    QLinearGradient gutterGrad(halfW - 24.0, 0, halfW + 24.0, 0);
    gutterGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
    gutterGrad.setColorAt(0.5, QColor(0, 0, 0, m_tokens.isDark ? 90 : 50));
    gutterGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(QRectF(halfW - 24.0, 0, 48.0, h), gutterGrad);

    // Central Crease Line
    painter.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
    painter.drawLine(QPointF(halfW, 0), QPointF(halfW, h));

    if (m_rightToLeft) {
        painter.setPen(QColor(m_tokens.accentColor.red(), m_tokens.accentColor.green(), m_tokens.accentColor.blue(), 200));
        painter.setFont(QFont("Noto Serif", 9, QFont::Bold));
        painter.drawText(QRectF(16, 12, 220, 22), Qt::AlignLeft | Qt::AlignVCenter, QString("📖 Manga Mode (RTL: ← Next)"));
    }
}

void DualPageRenderer::mousePressEvent(QMouseEvent* event) {
    if (m_rightToLeft) {
        // In Manga RTL mode, clicking the left edge advances forward (+2), right edge goes back (-2)
        if (event->position().x() < width() * 0.4) {
            emit pageTurnRequested(2);
        } else if (event->position().x() > width() * 0.6) {
            emit pageTurnRequested(-2);
        }
    } else {
        // Standard LTR mode
        if (event->position().x() > width() * 0.6) {
            emit pageTurnRequested(2);
        } else if (event->position().x() < width() * 0.4) {
            emit pageTurnRequested(-2);
        }
    }
}

} // namespace LunaRead
