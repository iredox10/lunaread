#include "DualPageRenderer.h"
#include "Typesetter.h"
#include "../features/ImageCropService.h"
#include "../features/PdfPageService.h"
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

void DualPageRenderer::setBookId(const QString& bookId) {
    m_bookId = bookId;
}

void DualPageRenderer::setFilePath(const QString& filePath) {
    m_filePath = filePath;
}

void DualPageRenderer::setAnnotations(const QVector<AnnotationRecord>& annotations) {
    m_annotations = annotations;
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

    // Two-page wide comic / manga spread detection
    if (m_leftPage.isImagePage && !m_leftPage.imageBytes.isEmpty()) {
        QPixmap pm;
        pm.loadFromData(m_leftPage.imageBytes);
        if (!pm.isNull() && pm.width() > pm.height() * 1.15) {
            painter.fillRect(rect(), m_tokens.backgroundColor);
            pm = ImageCropService::instance().cropMargins(pm);
            QRectF contentRect = rect().adjusted(8, 8, -8, -8);
            qreal scale = std::min(contentRect.width() / pm.width(), contentRect.height() / pm.height());
            qreal drawW = pm.width() * scale;
            qreal drawH = pm.height() * scale;
            qreal drawX = contentRect.left() + (contentRect.width() - drawW) / 2.0;
            qreal drawY = contentRect.top() + (contentRect.height() - drawH) / 2.0;
            QRectF targetRect(drawX, drawY, drawW, drawH);

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.fillRect(targetRect.adjusted(2, 3, 2, 3), QColor(0, 0, 0, 30));
            painter.fillRect(targetRect, Qt::white);
            painter.drawPixmap(targetRect, pm, pm.rect());
            painter.setPen(QPen(QColor(0, 0, 0, 40), 1.0));
            painter.drawRect(targetRect);
            painter.restore();
            return;
        }
    }

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
            m_typesetter->renderPage(&painter, leftRect, *leftSlotPage, m_tokens, m_bookTitle, m_chapterTitle, true, m_activeSentence, m_annotations);
        } else {
            painter.fillRect(leftRect, m_tokens.backgroundColor);
        }

        if (rightSlotPage) {
            m_typesetter->renderPage(&painter, rightRect, *rightSlotPage, m_tokens, m_bookTitle, m_chapterTitle, false, m_activeSentence, m_annotations);
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
                        QRectF contentRect = rect.adjusted(10, 10, -10, -10);
                        qreal scale = std::min(contentRect.width() / pm.width(), contentRect.height() / pm.height());
                        qreal drawW = pm.width() * scale;
                        qreal drawH = pm.height() * scale;
                        qreal drawX = contentRect.left() + (contentRect.width() - drawW) / 2.0;
                        qreal drawY = contentRect.top() + (contentRect.height() - drawH) / 2.0;
                        QRectF targetRect(drawX, drawY, drawW, drawH);

                        painter.save();
                        painter.setRenderHint(QPainter::Antialiasing, true);
                        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                        painter.fillRect(targetRect.adjusted(2, 3, 2, 3), QColor(0, 0, 0, 30));
                        painter.fillRect(targetRect, Qt::white);
                        painter.drawPixmap(targetRect, pm, pm.rect());
                        painter.setPen(QPen(QColor(0, 0, 0, 40), 1.0));
                        painter.drawRect(targetRect);
                        painter.restore();
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

    // Live Moon+ Reader selection feedback while dragging
    if (m_isSelecting && !m_selectionRect.isEmpty()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(m_selectionRect, QColor(255, 235, 59, 90)); // Moon+ Reader soft amber highlight
        painter.setPen(QPen(QColor(245, 158, 11, 220), 1.5, Qt::SolidLine));
        painter.drawRoundedRect(m_selectionRect, 3.0, 3.0);
        painter.restore();
    }
}

void DualPageRenderer::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->position();
        m_isSelecting = false;
        m_selectionRect = QRectF();
    }
}

void DualPageRenderer::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        qreal dist = (event->position() - m_pressPos).manhattanLength();
        if (dist > 6.0) {
            m_isSelecting = true;
            m_selectionRect = QRectF(m_pressPos, event->position()).normalized();
            update();
        }
    }
}

void DualPageRenderer::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    if (m_isSelecting) {
        m_isSelecting = false;
        QRectF sel = m_selectionRect;
        m_selectionRect = QRectF();
        update();

        if (sel.width() > 8.0 && sel.height() > 8.0) {
            qreal w = width();
            qreal h = height();
            qreal halfW = w / 2.0;

            QRectF leftRect(0, 0, halfW, h);
            QRectF rightRect(halfW, 0, halfW, h);

            const ParsedPage* leftSlotPage = m_rightToLeft ? (m_rightPage.has_value() ? &m_rightPage.value() : nullptr) : &m_leftPage;
            const ParsedPage* rightSlotPage = m_rightToLeft ? &m_leftPage : (m_rightPage.has_value() ? &m_rightPage.value() : nullptr);

            bool isLeftSlot = (sel.center().x() < halfW);
            const ParsedPage* slotPage = isLeftSlot ? leftSlotPage : rightSlotPage;
            QRectF slotRect = isLeftSlot ? leftRect : rightRect;

            if (slotPage) {
                if (slotPage->isImagePage) {
                    QPixmap pm;
                    if (!slotPage->imageBytes.isEmpty()) {
                        pm.loadFromData(slotPage->imageBytes);
                    }
                    QSize imgSize = pm.isNull() ? QSize(slotRect.width(), slotRect.height()) : pm.size();
                    QRectF targetRect = Typesetter::calculateImageTargetRect(slotRect, imgSize);
                    QRectF selInTarget = sel.intersected(targetRect);
                    if (selInTarget.width() > 4.0 && selInTarget.height() > 4.0) {
                        qreal nx = (selInTarget.x() - targetRect.x()) / targetRect.width();
                        qreal ny = (selInTarget.y() - targetRect.y()) / targetRect.height();
                        qreal nw = selInTarget.width() / targetRect.width();
                        qreal nh = selInTarget.height() / targetRect.height();
                        QRectF normRect(nx, ny, nw, nh);

                        QString text = PdfPageService::instance().getPageRegionText(m_filePath, slotPage->chapterIndex + 1, normRect);
                        if (text.trimmed().isEmpty()) {
                            text = QString("Page %1 Highlight").arg(slotPage->chapterIndex + 1);
                        }
                        emit selectionMade(text, normRect, slotPage->chapterIndex, slotPage->pageIndexInChapter, event->globalPosition().toPoint());
                    }
                } else if (m_typesetter) {
                    QString text = m_typesetter->selectedTextForRange(m_pressPos, event->position(), slotRect, *slotPage);
                    if (!text.trimmed().isEmpty()) {
                        emit selectionMade(text, QRectF(), slotPage->chapterIndex, slotPage->pageIndexInChapter, event->globalPosition().toPoint());
                    }
                }
            }
        }
        return;
    }

    // Pure Tap / Click navigation
    qreal x = event->position().x();
    qreal w = width();

    if (x >= w * 0.35 && x <= w * 0.65) {
        emit menuToggleRequested();
        return;
    }

    if (m_rightToLeft) {
        // In Manga RTL mode, clicking the left edge advances forward (+2), right edge goes back (-2)
        if (x < w * 0.35) {
            emit pageTurnRequested(2);
        } else if (x > w * 0.65) {
            emit pageTurnRequested(-2);
        }
    } else {
        // Standard LTR mode
        if (x > w * 0.65) {
            emit pageTurnRequested(2);
        } else if (x < w * 0.35) {
            emit pageTurnRequested(-2);
        }
    }
}

} // namespace LunaRead
