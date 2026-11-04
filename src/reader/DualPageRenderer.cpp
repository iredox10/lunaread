#include "DualPageRenderer.h"
#include "Typesetter.h"
#include "../features/ImageCropService.h"
#include "../features/PdfPageService.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QLinearGradient>
#include <QApplication>

namespace LunaRead {

namespace {
class ScopedDualPageFit {
public:
    explicit ScopedDualPageFit(Typesetter* ts) : m_ts(ts) {
        if (m_ts) {
            m_savedFit = m_ts->fitMode();
            m_savedPan = m_ts->imagePanY();
            m_ts->setFitMode(Typesetter::PageFitMode::FitPage);
            m_ts->setImagePanY(0.0);
        }
    }
    ~ScopedDualPageFit() {
        if (m_ts) {
            m_ts->setFitMode(m_savedFit);
            m_ts->setImagePanY(m_savedPan);
        }
    }
private:
    Typesetter* m_ts = nullptr;
    Typesetter::PageFitMode m_savedFit = Typesetter::PageFitMode::FitPage;
    qreal m_savedPan = 0.0;
};
} // namespace

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

QRectF DualPageRenderer::imageTargetRectFor(const ParsedPage& page, const QRectF& slotRect) const {
    QSize imgSize;
    if (m_typesetter) {
        const QPixmap prepared = m_typesetter->preparedPagePixmap(page.chapterIndex, page.imageBytes);
        if (!prepared.isNull()) imgSize = prepared.size();
    }
    if (imgSize.isEmpty()) {
        QPixmap pm;
        if (!page.imageBytes.isEmpty()) {
            pm.loadFromData(page.imageBytes);
        }
        if (!pm.isNull()) {
            pm = ImageCropService::instance().cropMargins(pm);
        }
        imgSize = pm.isNull() ? slotRect.size().toSize() : pm.size();
    }
    const qreal z = m_typesetter ? m_typesetter->zoomFactor() : 1.0;
    return Typesetter::calculateImageTargetRect(slotRect, imgSize, Typesetter::PageFitMode::FitPage, z);
}

const AnnotationRecord* DualPageRenderer::annotationAt(const QPointF& pos, const ParsedPage& page, const QRectF& targetRect) const {
    for (auto it = m_annotations.crbegin(); it != m_annotations.crend(); ++it) {
        const AnnotationRecord& ann = *it;
        if (ann.type == QStringLiteral("bookmark")) continue;
        if (ann.chapterIndex != page.chapterIndex || ann.pageIndex != page.pageIndexInChapter) continue;
        if (!ann.normalizedRect.isValid() || ann.normalizedRect.isNull()) continue;

        const QRectF r(
            targetRect.left() + ann.normalizedRect.x() * targetRect.width(),
            targetRect.top() + ann.normalizedRect.y() * targetRect.height(),
            ann.normalizedRect.width() * targetRect.width(),
            ann.normalizedRect.height() * targetRect.height());
        if (r.adjusted(-3.0, -3.0, 3.0, 3.0).contains(pos)) {
            return &ann;
        }
    }
    return nullptr;
}

void DualPageRenderer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    ScopedDualPageFit fitGuard(m_typesetter);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Two-page wide comic / manga spread detection
    if (m_leftPage.isImagePage && !m_leftPage.imageBytes.isEmpty()) {
        QPixmap pm;
        if (m_typesetter) {
            pm = m_typesetter->preparedPagePixmap(m_leftPage.chapterIndex, m_leftPage.imageBytes, &m_tokens);
        }
        if (pm.isNull()) {
            pm.loadFromData(m_leftPage.imageBytes);
            if (!pm.isNull()) pm = ImageCropService::instance().cropMargins(pm);
        }
        if (!pm.isNull() && pm.width() > pm.height() * 1.15) {
            painter.fillRect(rect(), m_tokens.backgroundColor);
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
            painter.fillRect(targetRect, m_tokens.backgroundColor);
            painter.drawPixmap(targetRect, pm, pm.rect());
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
                        painter.fillRect(targetRect, m_tokens.backgroundColor);
                        painter.drawPixmap(targetRect, pm, pm.rect());
                        painter.restore();
                        return;
                    }
                }
                painter.setPen(m_tokens.headerFooterColor);
                painter.setFont(QFont("Noto Serif", 11));
                painter.drawText(rect, Qt::AlignCenter, QString("Preparing page %1...").arg(p->chapterIndex + 1));
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

    if (m_rightToLeft) {
        painter.setPen(QColor(m_tokens.accentColor.red(), m_tokens.accentColor.green(), m_tokens.accentColor.blue(), 200));
        painter.setFont(QFont("Noto Serif", 9, QFont::Bold));
        painter.drawText(QRectF(16, 12, 220, 22), Qt::AlignLeft | Qt::AlignVCenter, QString("📖 Manga Mode (RTL: ← Next)"));
    }

    // Live Moon+ Reader selection feedback while dragging
    if (m_currentSelection.isValid() && m_typesetter) {
        m_typesetter->renderSelection(&painter, m_currentSelection, m_tokens);
    }
}

void DualPageRenderer::mousePressEvent(QMouseEvent* event) {
    ScopedDualPageFit fitGuard(m_typesetter);
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->position();
        m_dragEnd = event->position();

        // Check if tapping on existing selection handles
        if (m_currentSelection.isValid()) {
            qreal dStart = (event->position() - m_currentSelection.startHandle).manhattanLength();
            qreal dEnd = (event->position() - m_currentSelection.endHandle).manhattanLength();
            if (dStart <= 22.0) {
                m_activeHandleDrag = HandleDrag::StartHandle;
                m_fixedSelectionAnchor = m_currentSelection.endHandle;
                m_isSelecting = true;
                return;
            } else if (dEnd <= 22.0) {
                m_activeHandleDrag = HandleDrag::EndHandle;
                m_fixedSelectionAnchor = m_currentSelection.startHandle;
                m_isSelecting = true;
                return;
            } else {
                m_currentSelection.clear();
                m_activeHandleDrag = HandleDrag::None;
                update();
            }
        }

        m_isSelecting = false;
        m_activeHandleDrag = HandleDrag::None;
    }
}

void DualPageRenderer::mouseMoveEvent(QMouseEvent* event) {
    ScopedDualPageFit fitGuard(m_typesetter);
    if (event->buttons() & Qt::LeftButton) {
        qreal dist = (event->position() - m_pressPos).manhattanLength();
        const qreal dragThreshold = std::max(10.0, static_cast<qreal>(QApplication::startDragDistance()));
        if (dist > dragThreshold || m_activeHandleDrag != HandleDrag::None) {
            m_isSelecting = true;
            m_dragEnd = event->position();

            qreal halfW = width() / 2.0;
            QRectF leftRect(0, 0, halfW, height());
            QRectF rightRect(halfW, 0, halfW, height());
            const ParsedPage* leftSlotPage = m_rightToLeft ? (m_rightPage.has_value() ? &m_rightPage.value() : nullptr) : &m_leftPage;
            const ParsedPage* rightSlotPage = m_rightToLeft ? &m_leftPage : (m_rightPage.has_value() ? &m_rightPage.value() : nullptr);

            QPointF anchor = (m_activeHandleDrag != HandleDrag::None) ? m_fixedSelectionAnchor : m_pressPos;
            const bool isLeftSlot = (anchor.x() < halfW);
            const ParsedPage* slotPage = isLeftSlot ? leftSlotPage : rightSlotPage;
            const QRectF slotRect = isLeftSlot ? leftRect : rightRect;

            if (slotPage && m_typesetter) {
                m_currentSelection = m_typesetter->selectionForRange(anchor, event->position(), slotRect, *slotPage, m_filePath);
                m_selectedSlot = isLeftSlot ? 0 : 1;
            }
            update();
        }
    } else {
        if (m_currentSelection.isValid()) {
            qreal dStart = (event->position() - m_currentSelection.startHandle).manhattanLength();
            qreal dEnd = (event->position() - m_currentSelection.endHandle).manhattanLength();
            if (dStart <= 18.0 || dEnd <= 18.0) {
                setCursor(Qt::SizeHorCursor);
                return;
            }
        }
        // Link hover state
        qreal halfW = width() / 2.0;
        QRectF leftRect(0, 0, halfW, height());
        QRectF rightRect(halfW, 0, halfW, height());
        const ParsedPage* leftSlotPage = m_rightToLeft ? (m_rightPage.has_value() ? &m_rightPage.value() : nullptr) : &m_leftPage;
        const ParsedPage* rightSlotPage = m_rightToLeft ? &m_leftPage : (m_rightPage.has_value() ? &m_rightPage.value() : nullptr);
        const bool isLeftSlot = (event->position().x() < halfW);
        const ParsedPage* slotPage = isLeftSlot ? leftSlotPage : rightSlotPage;
        const QRectF slotRect = isLeftSlot ? leftRect : rightRect;

        if (slotPage && m_typesetter) {
            QString href = m_typesetter->anchorAt(event->position(), slotRect, *slotPage, m_filePath);
            if (!href.isEmpty()) {
                setCursor(Qt::PointingHandCursor);
                setToolTip(href);
                return;
            }
        }
        setCursor(Qt::ArrowCursor);
        setToolTip(QString());
    }
}

void DualPageRenderer::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    ScopedDualPageFit fitGuard(m_typesetter);

    if (m_isSelecting) {
        m_isSelecting = false;
        m_activeHandleDrag = HandleDrag::None;

        if (m_currentSelection.isValid()) {
            QPoint globalP = mapToGlobal(m_currentSelection.endHandle.toPoint());
            if (!m_currentSelection.lineRects.isEmpty()) {
                globalP = mapToGlobal(QPoint(
                    static_cast<int>(m_currentSelection.lineRects.first().center().x()),
                    static_cast<int>(m_currentSelection.lineRects.first().top())
                ));
            }
            emit selectionMade(m_currentSelection.text, m_currentSelection.normalizedEnclosingRect,
                               m_currentSelection.chapterIndex, m_currentSelection.pageIndex, globalP);
            update();
            return;
        }
    }

    // Moon+ Reader: tapping an existing highlight opens its manage toolbar
    {
        if (m_leftPage.isImagePage && !m_leftPage.imageBytes.isEmpty()) {
            QPixmap probe;
            probe.loadFromData(m_leftPage.imageBytes);
            if (!probe.isNull() && probe.width() > probe.height() * 1.15) {
                const QRectF target = imageTargetRectFor(m_leftPage, rect());
                if (const auto* ann = annotationAt(event->position(), m_leftPage, target)) {
                    emit annotationTapped(ann->id, event->globalPosition().toPoint());
                    return;
                }
            }
        }

        qreal halfW = width() / 2.0;
        QRectF leftRect(0, 0, halfW, height());
        QRectF rightRect(halfW, 0, halfW, height());
        const ParsedPage* leftSlotPage = m_rightToLeft ? (m_rightPage.has_value() ? &m_rightPage.value() : nullptr) : &m_leftPage;
        const ParsedPage* rightSlotPage = m_rightToLeft ? &m_leftPage : (m_rightPage.has_value() ? &m_rightPage.value() : nullptr);
        const bool isLeftSlot = (event->position().x() < halfW);
        const ParsedPage* slotPage = isLeftSlot ? leftSlotPage : rightSlotPage;
        const QRectF slotRect = isLeftSlot ? leftRect : rightRect;

        if (slotPage && slotPage->isImagePage && !slotPage->imageBytes.isEmpty()) {
            const QRectF target = imageTargetRectFor(*slotPage, slotRect);
            if (const auto* ann = annotationAt(event->position(), *slotPage, target)) {
                emit annotationTapped(ann->id, event->globalPosition().toPoint());
                return;
            }
        }

        // Check for Hyperlink / Footnote / Page Jump Click in Dual Page mode
        if (slotPage && m_typesetter) {
            QString href = m_typesetter->anchorAt(event->position(), slotRect, *slotPage, m_filePath);
            if (!href.isEmpty()) {
                emit linkClicked(href, event->position());
                return;
            }
        }
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

void DualPageRenderer::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        ScopedDualPageFit fitGuard(m_typesetter);
        qreal halfW = width() / 2.0;
        QRectF leftRect(0, 0, halfW, height());
        QRectF rightRect(halfW, 0, halfW, height());
        const ParsedPage* leftSlotPage = m_rightToLeft ? (m_rightPage.has_value() ? &m_rightPage.value() : nullptr) : &m_leftPage;
        const ParsedPage* rightSlotPage = m_rightToLeft ? &m_leftPage : (m_rightPage.has_value() ? &m_rightPage.value() : nullptr);
        const bool isLeftSlot = (event->position().x() < halfW);
        const ParsedPage* slotPage = isLeftSlot ? leftSlotPage : rightSlotPage;
        const QRectF slotRect = isLeftSlot ? leftRect : rightRect;

        if (slotPage && m_typesetter) {
            BookTextSelection sel = m_typesetter->wordAt(event->position(), slotRect, *slotPage, m_filePath);
            if (sel.isValid()) {
                m_currentSelection = sel;
                m_selectedSlot = isLeftSlot ? 0 : 1;
                QPoint globalP = mapToGlobal(QPoint(
                    static_cast<int>(sel.lineRects.first().center().x()),
                    static_cast<int>(sel.lineRects.first().top())
                ));
                emit selectionMade(sel.text, sel.normalizedEnclosingRect, sel.chapterIndex, sel.pageIndex, globalP);
                update();
                event->accept();
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void DualPageRenderer::contextMenuEvent(QContextMenuEvent* event) {
    qreal halfW = width() / 2.0;
    const ParsedPage* leftSlotPage = m_rightToLeft ? (m_rightPage.has_value() ? &m_rightPage.value() : nullptr) : &m_leftPage;
    const ParsedPage* rightSlotPage = m_rightToLeft ? &m_leftPage : (m_rightPage.has_value() ? &m_rightPage.value() : nullptr);
    const bool isLeftSlot = (event->pos().x() < halfW);
    const ParsedPage* slotPage = isLeftSlot ? leftSlotPage : rightSlotPage;

    int chIdx = slotPage ? slotPage->chapterIndex : -1;
    int pgIdx = slotPage ? slotPage->pageIndexInChapter : -1;

    emit contextMenuRequested(event->globalPos(), chIdx, pgIdx);
    event->accept();
}

void DualPageRenderer::wheelEvent(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    if (delta == 0) return;
    if (delta < 0) {
        emit pageTurnRequested(2);
    } else {
        emit pageTurnRequested(-2);
    }
    event->accept();
}

} // namespace LunaRead
