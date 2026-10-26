#include "ReadingRulerWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

namespace LunaRead {

ReadingRulerWidget::ReadingRulerWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_smoothTimer = new QTimer(this);
    m_smoothTimer->setInterval(16); // ~60 FPS
    connect(m_smoothTimer, &QTimer::timeout, this, &ReadingRulerWidget::onSmoothTimerTick);

    if (parent) {
        parent->installEventFilter(this);
        setGeometry(0, 0, parent->width(), parent->height());
    }
}

void ReadingRulerWidget::setRulerY(qreal y) {
    const qreal maxPosY = static_cast<qreal>(std::max(0, height() - m_rulerHeight));
    const qreal clampedY = std::clamp(y, 0.0, maxPosY);
    if (qFuzzyCompare(m_currentY, clampedY) && qFuzzyCompare(m_targetY, clampedY)) {
        return;
    }
    m_targetY = clampedY;
    m_currentY = clampedY;
    if (m_smoothTimer && m_smoothTimer->isActive()) {
        m_smoothTimer->stop();
    }
    emit rulerYChanged(m_currentY);
    update();
}

void ReadingRulerWidget::setTargetY(qreal targetY) {
    const qreal maxPosY = static_cast<qreal>(std::max(0, height() - m_rulerHeight));
    const qreal clamped = std::clamp(targetY, 0.0, maxPosY);
    if (qFuzzyCompare(m_targetY, clamped)) {
        return;
    }
    m_targetY = clamped;
    if (!m_smoothMovement) {
        m_currentY = m_targetY;
        emit rulerYChanged(m_currentY);
        update();
    } else {
        if (!m_smoothTimer->isActive()) {
            m_smoothTimer->start();
        }
    }
}

void ReadingRulerWidget::setRulerHeight(int height) {
    const int clamped = std::clamp(height, MinHeight, MaxHeight);
    if (m_rulerHeight == clamped) return;

    m_rulerHeight = clamped;
    clampRulerPosition();
    emit rulerHeightChanged(m_rulerHeight);
    update();
}

QRectF ReadingRulerWidget::rulerRect() const {
    return QRectF(0, m_currentY, static_cast<qreal>(width()), static_cast<qreal>(m_rulerHeight));
}

void ReadingRulerWidget::setOpacity(qreal opacity) {
    const qreal clamped = std::clamp(opacity, MinOpacity, MaxOpacity);
    if (qFuzzyCompare(m_opacity, clamped)) return;

    m_opacity = clamped;
    emit opacityChanged(m_opacity);
    update();
}

void ReadingRulerWidget::setOpacityPercent(int percent) {
    setOpacity(static_cast<qreal>(percent) / 100.0);
}

void ReadingRulerWidget::setRulerColor(RulerColor color) {
    if (m_rulerColor == color) return;
    m_rulerColor = color;
    emit rulerColorChanged(m_rulerColor);
    update();
}

void ReadingRulerWidget::setCustomColor(const QColor& color) {
    if (m_customColor == color && m_rulerColor == RulerColor::Custom) return;
    m_customColor = color;
    m_rulerColor = RulerColor::Custom;
    emit customColorChanged(m_customColor);
    emit rulerColorChanged(m_rulerColor);
    update();
}

QColor ReadingRulerWidget::presetColor(RulerColor color) {
    switch (color) {
        case RulerColor::GoldenYellow:
            return QColor(250, 204, 21); // Tailwind yellow-400 / golden amber
        case RulerColor::SoftCyan:
            return QColor(56, 189, 248); // Tailwind sky-400 / soft cyan
        case RulerColor::SubtleGrey:
            return QColor(156, 163, 175); // Tailwind gray-400 / subtle reading grey
        case RulerColor::Custom:
            return QColor(250, 204, 21);
    }
    return QColor(250, 204, 21);
}

QColor ReadingRulerWidget::effectiveColor() const {
    if (m_rulerColor == RulerColor::Custom) {
        return m_customColor;
    }
    return presetColor(m_rulerColor);
}

QColor ReadingRulerWidget::effectiveBorderColor() const {
    switch (m_rulerColor) {
        case RulerColor::GoldenYellow:
            return QColor(217, 119, 6); // Amber border
        case RulerColor::SoftCyan:
            return QColor(14, 165, 233); // Sky border
        case RulerColor::SubtleGrey:
            return QColor(107, 114, 128); // Slate border
        case RulerColor::Custom:
            return m_customColor.darker(130);
    }
    return QColor(217, 119, 6);
}

void ReadingRulerWidget::setMode(RulerMode mode) {
    if (m_mode == mode) return;
    m_mode = mode;
    if (m_mode == RulerMode::FixedOrKeyboard && m_smoothTimer && m_smoothTimer->isActive()) {
        m_smoothTimer->stop();
    }
    emit modeChanged(m_mode);
    update();
}

void ReadingRulerWidget::setDimSurroundings(bool dim) {
    if (m_dimSurroundings == dim) return;
    m_dimSurroundings = dim;
    emit dimSurroundingsChanged(m_dimSurroundings);
    update();
}

void ReadingRulerWidget::setDimOpacity(qreal opacity) {
    const qreal clamped = std::clamp(opacity, 0.0, 1.0);
    if (qFuzzyCompare(m_dimOpacity, clamped)) return;
    m_dimOpacity = clamped;
    emit dimOpacityChanged(m_dimOpacity);
    update();
}

void ReadingRulerWidget::setSmoothMovement(bool smooth) {
    if (m_smoothMovement == smooth) return;
    m_smoothMovement = smooth;
    if (!m_smoothMovement && m_smoothTimer && m_smoothTimer->isActive()) {
        m_smoothTimer->stop();
        m_currentY = m_targetY;
        update();
    }
    emit smoothMovementChanged(m_smoothMovement);
}

void ReadingRulerWidget::setSmoothDamping(qreal damping) {
    m_smoothDamping = std::clamp(damping, 0.05, 1.0);
}

void ReadingRulerWidget::setGuideLineThickness(int thickness) {
    const int clamped = std::clamp(thickness, 1, 8);
    if (m_guideLineThickness == clamped) return;
    m_guideLineThickness = clamped;
    emit guideLineThicknessChanged(m_guideLineThickness);
    update();
}

void ReadingRulerWidget::setStepSize(int step) {
    m_stepSize = std::clamp(step, 4, 120);
}

void ReadingRulerWidget::setPassClicksThrough(bool pass) {
    if (m_passClicksThrough == pass) return;
    m_passClicksThrough = pass;
    emit passClicksThroughChanged(m_passClicksThrough);
}

void ReadingRulerWidget::toggleRuler() {
    setRulerVisible(!isVisible());
}

void ReadingRulerWidget::showRuler() {
    setRulerVisible(true);
}

void ReadingRulerWidget::hideRuler() {
    setRulerVisible(false);
}

void ReadingRulerWidget::setRulerVisible(bool visible) {
    if (isVisible() == visible) return;
    setVisible(visible);
    if (visible) {
        if (parentWidget()) {
            setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
            clampRulerPosition();
        }
        raise();
    }
    emit rulerToggled(visible);
}

void ReadingRulerWidget::stepUp() {
    moveRuler(-static_cast<qreal>(m_stepSize));
}

void ReadingRulerWidget::stepDown() {
    moveRuler(static_cast<qreal>(m_stepSize));
}

void ReadingRulerWidget::moveRuler(qreal deltaY) {
    setRulerY(m_currentY + deltaY);
}

void ReadingRulerWidget::jumpToTop() {
    setRulerY(0.0);
}

void ReadingRulerWidget::jumpToBottom() {
    const qreal maxPosY = static_cast<qreal>(std::max(0, height() - m_rulerHeight));
    setRulerY(maxPosY);
}

void ReadingRulerWidget::cycleColor() {
    switch (m_rulerColor) {
        case RulerColor::GoldenYellow:
            setRulerColor(RulerColor::SoftCyan);
            break;
        case RulerColor::SoftCyan:
            setRulerColor(RulerColor::SubtleGrey);
            break;
        case RulerColor::SubtleGrey:
            setRulerColor(RulerColor::GoldenYellow);
            break;
        case RulerColor::Custom:
            setRulerColor(RulerColor::GoldenYellow);
            break;
    }
}

void ReadingRulerWidget::cycleMode() {
    if (m_mode == RulerMode::FollowMouse) {
        setMode(RulerMode::FixedOrKeyboard);
    } else {
        setMode(RulerMode::FollowMouse);
    }
}

void ReadingRulerWidget::increaseHeight(int delta) {
    setRulerHeight(m_rulerHeight + delta);
}

void ReadingRulerWidget::decreaseHeight(int delta) {
    setRulerHeight(m_rulerHeight - delta);
}

void ReadingRulerWidget::increaseOpacity(qreal delta) {
    setOpacity(m_opacity + delta);
}

void ReadingRulerWidget::decreaseOpacity(qreal delta) {
    setOpacity(m_opacity - delta);
}

void ReadingRulerWidget::onSmoothTimerTick() {
    const qreal diff = m_targetY - m_currentY;
    if (std::abs(diff) < 0.5) {
        m_currentY = m_targetY;
        m_smoothTimer->stop();
    } else {
        m_currentY += diff * m_smoothDamping;
    }
    emit rulerYChanged(m_currentY);
    update();
}

void ReadingRulerWidget::clampRulerPosition() {
    const qreal maxPosY = static_cast<qreal>(std::max(0, height() - m_rulerHeight));
    m_currentY = std::clamp(m_currentY, 0.0, maxPosY);
    m_targetY = std::clamp(m_targetY, 0.0, maxPosY);
}

void ReadingRulerWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal w = static_cast<qreal>(width());
    const qreal h = static_cast<qreal>(height());
    const qreal ry = m_currentY;
    const qreal rh = static_cast<qreal>(m_rulerHeight);

    // 1. Semi-transparent upper and lower dimmed regions
    if (m_dimSurroundings) {
        const int dimAlpha = static_cast<int>(std::clamp(m_dimOpacity, 0.0, 1.0) * 255.0);
        const QColor dimColor(0, 0, 0, dimAlpha);

        if (ry > 0.0) {
            painter.fillRect(QRectF(0.0, 0.0, w, ry), dimColor);
        }
        const qreal lowerY = ry + rh;
        if (lowerY < h) {
            painter.fillRect(QRectF(0.0, lowerY, w, h - lowerY), dimColor);
        }
    }

    // 2. Focus Strip Highlight
    const QRectF strip(0.0, ry, w, rh);
    QColor fill = effectiveColor();
    fill.setAlphaF(static_cast<float>(std::clamp(m_opacity, 0.0, 1.0)));
    painter.fillRect(strip, fill);

    // 3. Crisp Guide Lines along strip boundaries
    QColor border = effectiveBorderColor();
    const qreal borderAlpha = std::clamp(m_opacity * 1.8 + 0.25, 0.20, 1.0);
    border.setAlphaF(static_cast<float>(borderAlpha));

    QPen pen(border, static_cast<qreal>(m_guideLineThickness));
    pen.setCapStyle(Qt::FlatCap);
    painter.setPen(pen);
    painter.drawLine(QPointF(0.0, ry), QPointF(w, ry));
    painter.drawLine(QPointF(0.0, ry + rh), QPointF(w, ry + rh));

    // 4. Grip handle in fixed mode
    if (m_mode == RulerMode::FixedOrKeyboard) {
        drawGripHandles(painter, strip);
    }
}

void ReadingRulerWidget::drawGripHandles(QPainter& painter, const QRectF& rect) {
    if (m_mode != RulerMode::FixedOrKeyboard) return;

    const qreal dotRadius = 1.5;
    const qreal x = rect.right() - 20.0;
    const qreal cy = rect.center().y();

    QColor gripCol = effectiveBorderColor();
    gripCol.setAlphaF(0.45f);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gripCol);

    painter.drawEllipse(QPointF(x, cy - 6.0), dotRadius, dotRadius);
    painter.drawEllipse(QPointF(x, cy), dotRadius, dotRadius);
    painter.drawEllipse(QPointF(x, cy + 6.0), dotRadius, dotRadius);
}

void ReadingRulerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_mode == RulerMode::FixedOrKeyboard) {
        const QRectF strip = rulerRect();
        if (strip.contains(event->position())) {
            m_isDragging = true;
            m_dragOffsetY = event->position().y() - m_currentY;
            event->accept();
            return;
        }
    }

    // Unhandled mouse click: pass to parent reader
    forwardMouseEventToUnderlying(event);
    event->ignore();
}

void ReadingRulerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        const qreal newY = event->position().y() - m_dragOffsetY;
        setRulerY(newY);
        event->accept();
        return;
    }

    if (m_mode == RulerMode::FollowMouse) {
        const qreal target = event->position().y() - static_cast<qreal>(m_rulerHeight) / 2.0;
        setTargetY(target);
        event->accept();
        return;
    }

    event->ignore();
}

void ReadingRulerWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        event->accept();
        return;
    }

    // Pass unhandled mouse release to parent reader
    forwardMouseEventToUnderlying(event);
    event->ignore();
}

void ReadingRulerWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    forwardMouseEventToUnderlying(event);
    event->ignore();
}

void ReadingRulerWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            increaseHeight();
        } else if (event->angleDelta().y() < 0) {
            decreaseHeight();
        }
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        if (event->angleDelta().y() > 0) {
            increaseOpacity();
        } else if (event->angleDelta().y() < 0) {
            decreaseOpacity();
        }
        event->accept();
        return;
    }

    if (m_mode == RulerMode::FixedOrKeyboard && (event->modifiers() & Qt::AltModifier)) {
        if (event->angleDelta().y() > 0) {
            stepUp();
        } else if (event->angleDelta().y() < 0) {
            stepDown();
        }
        event->accept();
        return;
    }

    // Pass unhandled wheel events to parent reader (for scroll or page turning)
    forwardWheelEventToUnderlying(event);
    event->ignore();
}

void ReadingRulerWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Up:
        case Qt::Key_K:
        case Qt::Key_W:
            stepUp();
            event->accept();
            return;
        case Qt::Key_Down:
        case Qt::Key_J:
        case Qt::Key_S:
            stepDown();
            event->accept();
            return;
        case Qt::Key_PageUp:
            moveRuler(-static_cast<qreal>(m_rulerHeight));
            event->accept();
            return;
        case Qt::Key_PageDown:
            moveRuler(static_cast<qreal>(m_rulerHeight));
            event->accept();
            return;
        case Qt::Key_Home:
            jumpToTop();
            event->accept();
            return;
        case Qt::Key_End:
            jumpToBottom();
            event->accept();
            return;
        case Qt::Key_BracketLeft:
            decreaseHeight();
            event->accept();
            return;
        case Qt::Key_BracketRight:
            increaseHeight();
            event->accept();
            return;
        case Qt::Key_Minus:
            decreaseOpacity();
            event->accept();
            return;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            increaseOpacity();
            event->accept();
            return;
        case Qt::Key_C:
            cycleColor();
            event->accept();
            return;
        case Qt::Key_M:
            cycleMode();
            event->accept();
            return;
        default:
            break;
    }

    // Forward any unhandled key to parent
    if (parentWidget()) {
        QKeyEvent forwarded(
            event->type(),
            event->key(),
            event->modifiers(),
            event->nativeScanCode(),
            event->nativeVirtualKey(),
            event->nativeModifiers(),
            event->text(),
            event->isAutoRepeat(),
            static_cast<quint16>(event->count())
        );
        QCoreApplication::sendEvent(parentWidget(), &forwarded);
    }
    event->ignore();
}

bool ReadingRulerWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize) {
            auto* re = static_cast<QResizeEvent*>(event);
            setGeometry(0, 0, re->size().width(), re->size().height());
            clampRulerPosition();
        } else if (event->type() == QEvent::MouseMove && isVisible() && m_mode == RulerMode::FollowMouse) {
            auto* me = static_cast<QMouseEvent*>(event);
            const qreal target = me->position().y() - static_cast<qreal>(m_rulerHeight) / 2.0;
            setTargetY(target);
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ReadingRulerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    clampRulerPosition();
}

void ReadingRulerWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (parentWidget()) {
        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
        clampRulerPosition();
    }
    raise();
}

void ReadingRulerWidget::forwardMouseEventToUnderlying(QMouseEvent* event) {
    if (!m_passClicksThrough || m_isForwardingEvent || !parentWidget()) {
        return;
    }
    m_isForwardingEvent = true;

    const QPointF parentPos = mapTo(parentWidget(), event->position());
    QWidget* target = nullptr;

    const auto children = parentWidget()->children();
    for (QObject* child : children) {
        if (auto* w = qobject_cast<QWidget*>(child)) {
            if (w != this && w->isVisible() && !w->testAttribute(Qt::WA_TransparentForMouseEvents)) {
                if (w->geometry().contains(parentPos.toPoint())) {
                    target = w;
                    break;
                }
            }
        }
    }
    if (!target) {
        target = parentWidget();
    }

    const QPointF targetPos = (target == parentWidget()) ? parentPos : target->mapFrom(parentWidget(), parentPos.toPoint());

    QMouseEvent forwarded(
        event->type(),
        targetPos,
        event->scenePosition(),
        event->globalPosition(),
        event->button(),
        event->buttons(),
        event->modifiers(),
        event->pointingDevice()
    );

    QCoreApplication::sendEvent(target, &forwarded);
    m_isForwardingEvent = false;
}

void ReadingRulerWidget::forwardWheelEventToUnderlying(QWheelEvent* event) {
    if (m_isForwardingEvent || !parentWidget()) {
        return;
    }
    m_isForwardingEvent = true;

    const QPointF parentPos = mapTo(parentWidget(), event->position());
    QWidget* target = nullptr;

    const auto children = parentWidget()->children();
    for (QObject* child : children) {
        if (auto* w = qobject_cast<QWidget*>(child)) {
            if (w != this && w->isVisible() && !w->testAttribute(Qt::WA_TransparentForMouseEvents)) {
                if (w->geometry().contains(parentPos.toPoint())) {
                    target = w;
                    break;
                }
            }
        }
    }
    if (!target) {
        target = parentWidget();
    }

    const QPointF targetPos = (target == parentWidget()) ? parentPos : target->mapFrom(parentWidget(), parentPos.toPoint());

    QWheelEvent forwarded(
        targetPos,
        event->globalPosition(),
        event->pixelDelta(),
        event->angleDelta(),
        event->buttons(),
        event->modifiers(),
        event->phase(),
        event->inverted(),
        event->source(),
        event->pointingDevice()
    );

    QCoreApplication::sendEvent(target, &forwarded);
    m_isForwardingEvent = false;
}

} // namespace LunaRead
