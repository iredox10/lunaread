#include "AutoScrollOverlayWidget.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>

namespace LunaRead {

AutoScrollOverlayWidget::AutoScrollOverlayWidget(QWidget* parent)
    : QWidget(parent)
    , m_tokens(ReaderThemeTokens::night())
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);

    // Subtle drop shadow elevation
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 110));
    setGraphicsEffect(shadow);

    if (parent) {
        parent->installEventFilter(this);
    }

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setTimerType(Qt::PreciseTimer);
    m_scrollTimer->setInterval(16); // ~60 ticks/second for smooth animation
    connect(m_scrollTimer, &QTimer::timeout, this, &AutoScrollOverlayWidget::onScrollTick);

    setupUi();
    setupStyle();
    updateSpeedLabel();
    updatePlayPauseUi();
}

AutoScrollOverlayWidget::~AutoScrollOverlayWidget() {
    if (m_scrollTimer && m_scrollTimer->isActive()) {
        m_scrollTimer->stop();
    }
    if (parentWidget()) {
        parentWidget()->removeEventFilter(this);
    }
}

int AutoScrollOverlayWidget::currentSpeed() const noexcept {
    if (m_mode == AutoScrollMode::PixelByPixel) return m_pixelSpeed;
    if (m_mode == AutoScrollMode::LineByLine) return m_lineSpeed;
    return m_pageIntervalSec;
}

void AutoScrollOverlayWidget::setMode(AutoScrollMode mode) {
    if (m_mode == mode) return;
    m_mode = mode;
    m_subPixelAccumulator = 0.0;
    m_lineAccumulator = 0.0;
    m_pageSecAccumulator = 0.0;
    updateModeButtonUi();
    updateSpeedLabel();
    emit modeChanged(m_mode);
}

void AutoScrollOverlayWidget::setPixelSpeed(int pxPerSec) {
    const int clamped = std::clamp(pxPerSec, MinPixelSpeed, MaxPixelSpeed);
    if (m_pixelSpeed == clamped) return;
    m_pixelSpeed = clamped;
    if (m_mode == AutoScrollMode::PixelByPixel) {
        updateSpeedLabel();
        emit speedChanged(m_pixelSpeed);
    }
}

void AutoScrollOverlayWidget::setLineSpeed(int linesPerSec) {
    const int clamped = std::clamp(linesPerSec, MinLineSpeed, MaxLineSpeed);
    if (m_lineSpeed == clamped) return;
    m_lineSpeed = clamped;
    if (m_mode == AutoScrollMode::LineByLine) {
        updateSpeedLabel();
        emit speedChanged(m_lineSpeed);
    }
}

void AutoScrollOverlayWidget::setPageIntervalSec(int sec) {
    const int clamped = std::clamp(sec, MinPageIntervalSec, MaxPageIntervalSec);
    if (m_pageIntervalSec == clamped) return;
    m_pageIntervalSec = clamped;
    if (m_mode == AutoScrollMode::PageByPage) {
        updateSpeedLabel();
        emit speedChanged(m_pageIntervalSec);
    }
}

void AutoScrollOverlayWidget::setSpeed(int speed) {
    if (m_mode == AutoScrollMode::PixelByPixel) {
        setPixelSpeed(speed);
    } else if (m_mode == AutoScrollMode::LineByLine) {
        setLineSpeed(speed);
    } else {
        setPageIntervalSec(speed);
    }
}

void AutoScrollOverlayWidget::setLineHeight(int height) {
    if (height > 0) {
        m_lineHeight = height;
    }
}

void AutoScrollOverlayWidget::updateTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    setupStyle();
    update();
}

void AutoScrollOverlayWidget::reposition(const QSize& parentSize, bool forceReset) {
    if (parentSize.width() <= 0 || parentSize.height() <= 0) return;

    if (!m_hasCustomPosition || forceReset) {
        // Position HUD near the bottom right of the reader viewport
        constexpr int marginX = 24;
        constexpr int marginY = 24;
        const int targetX = std::max(8, parentSize.width() - width() - marginX);
        const int targetY = std::max(8, parentSize.height() - height() - marginY);
        move(targetX, targetY);
        m_hasCustomPosition = false;
    } else {
        // Keep clamped inside viewport if previously dragged by user
        constexpr int minX = 8;
        constexpr int minY = 8;
        const int maxX = std::max(minX, parentSize.width() - width() - 8);
        const int maxY = std::max(minY, parentSize.height() - height() - 8);
        move(std::clamp(x(), minX, maxX), std::clamp(y(), minY, maxY));
    }
    raise();
}

void AutoScrollOverlayWidget::resetToDefaultPosition() {
    m_hasCustomPosition = false;
    if (parentWidget()) {
        reposition(parentWidget()->size(), true);
    }
}

void AutoScrollOverlayWidget::play() {
    if (m_isActive) return;
    m_isActive = true;
    m_subPixelAccumulator = 0.0;
    m_lineAccumulator = 0.0;
    m_elapsedTimer.start();
    m_scrollTimer->start();
    updatePlayPauseUi();
    emit autoScrollToggled(true);
}

void AutoScrollOverlayWidget::pause() {
    if (!m_isActive) return;
    m_isActive = false;
    m_scrollTimer->stop();
    updatePlayPauseUi();
    emit autoScrollToggled(false);
}

void AutoScrollOverlayWidget::togglePlayPause() {
    if (m_isActive) {
        pause();
    } else {
        play();
    }
}

void AutoScrollOverlayWidget::stop() {
    pause();
    m_subPixelAccumulator = 0.0;
    m_lineAccumulator = 0.0;
}

void AutoScrollOverlayWidget::showAndStart() {
    show();
    play();
}

void AutoScrollOverlayWidget::increaseSpeed() {
    if (m_mode == AutoScrollMode::PixelByPixel) {
        setPixelSpeed(m_pixelSpeed + PixelSpeedStep);
    } else if (m_mode == AutoScrollMode::LineByLine) {
        setLineSpeed(m_lineSpeed + LineSpeedStep);
    } else {
        setPageIntervalSec(m_pageIntervalSec - PageIntervalStep);
    }
}

void AutoScrollOverlayWidget::decreaseSpeed() {
    if (m_mode == AutoScrollMode::PixelByPixel) {
        setPixelSpeed(m_pixelSpeed - PixelSpeedStep);
    } else if (m_mode == AutoScrollMode::LineByLine) {
        setLineSpeed(m_lineSpeed - LineSpeedStep);
    } else {
        setPageIntervalSec(m_pageIntervalSec + PageIntervalStep);
    }
}

void AutoScrollOverlayWidget::toggleMode() {
    if (m_mode == AutoScrollMode::PixelByPixel) {
        setMode(AutoScrollMode::LineByLine);
    } else if (m_mode == AutoScrollMode::LineByLine) {
        setMode(AutoScrollMode::PageByPage);
    } else {
        setMode(AutoScrollMode::PixelByPixel);
    }
}

void AutoScrollOverlayWidget::dismiss() {
    stop();
    hide();
    emit closeRequested();
}

void AutoScrollOverlayWidget::onScrollTick() {
    if (!m_isActive) return;

    qint64 elapsedMs = m_elapsedTimer.restart();
    if (elapsedMs <= 0) return;
    if (elapsedMs > 200) {
        // Prevent huge jump after system freeze or window move
        elapsedMs = 16;
    }

    if (m_mode == AutoScrollMode::PixelByPixel) {
        // Continuous smooth roll based on pixel speed (px/sec)
        const double deltaPixels = (static_cast<double>(m_pixelSpeed) * static_cast<double>(elapsedMs)) / 1000.0;
        m_subPixelAccumulator += deltaPixels;

        const int step = static_cast<int>(m_subPixelAccumulator);
        if (step > 0) {
            m_subPixelAccumulator -= step;
            emit scrollStepRequested(step);
        }
    } else if (m_mode == AutoScrollMode::LineByLine) {
        // Line-by-line discrete roll based on line speed (lines/sec)
        const double deltaLines = (static_cast<double>(m_lineSpeed) * static_cast<double>(elapsedMs)) / 1000.0;
        m_lineAccumulator += deltaLines;

        if (m_lineAccumulator >= 1.0) {
            const int linesToScroll = static_cast<int>(m_lineAccumulator);
            m_lineAccumulator -= linesToScroll;
            const int pixelDelta = linesToScroll * m_lineHeight;
            if (pixelDelta > 0) {
                emit scrollStepRequested(pixelDelta);
            }
        }
    } else {
        // Page-by-page timed turn (seconds per page)
        m_pageSecAccumulator += static_cast<double>(elapsedMs) / 1000.0;
        if (m_pageSecAccumulator >= static_cast<double>(m_pageIntervalSec)) {
            m_pageSecAccumulator = 0.0;
            emit pageTurnRequested(1);
        }
    }
}

void AutoScrollOverlayWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = rect().adjusted(1, 1, -1, -1);
    constexpr qreal cornerRadius = 18.0;

    QPainterPath path;
    path.addRoundedRect(bounds, cornerRadius, cornerRadius);

    // Semi-transparent frosted glass fill
    QColor bgColor;
    QColor outlineColor;
    if (m_tokens.isDark) {
        bgColor = QColor(22, 27, 34, 225);
        outlineColor = QColor(255, 255, 255, 38);
    } else {
        bgColor = QColor(246, 248, 250, 230);
        outlineColor = QColor(0, 0, 0, 32);
    }
    painter.fillPath(path, bgColor);

    // Top subtle highlight sheen
    QPainterPath glossPath;
    const QRectF glossRect(bounds.x(), bounds.y(), bounds.width(), bounds.height() * 0.45);
    glossPath.addRoundedRect(glossRect, cornerRadius, cornerRadius);

    QLinearGradient gloss(0, glossRect.top(), 0, glossRect.bottom());
    if (m_tokens.isDark) {
        gloss.setColorAt(0.0, QColor(255, 255, 255, 22));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    } else {
        gloss.setColorAt(0.0, QColor(255, 255, 255, 65));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    }
    painter.fillPath(glossPath, gloss);

    // Subtle 1px frosted outline border
    QPen borderPen(outlineColor, 1.2);
    painter.strokePath(path, borderPen);
}

bool AutoScrollOverlayWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize) {
            auto* resizeEvent = static_cast<QResizeEvent*>(event);
            reposition(resizeEvent->size());
        } else if (isVisible()) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    // Tap on screen (outside this HUD) toggles pause/resume
                    if (!geometry().contains(mouseEvent->pos())) {
                        if (m_isActive && m_tapToPauseEnabled) {
                            pause();
                            return true; // Consume event to prevent accidental page turn or highlight
                        } else if (!m_isActive && m_tapToResumeEnabled) {
                            play();
                            return true; // Consume event to resume auto-scroll
                        }
                    }
                }
            } else if (event->type() == QEvent::KeyPress) {
                if (m_shortcutsEnabled) {
                    auto* keyEvent = static_cast<QKeyEvent*>(event);
                    switch (keyEvent->key()) {
                    case Qt::Key_Space:
                        togglePlayPause();
                        return true;
                    case Qt::Key_Up:
                        increaseSpeed();
                        return true;
                    case Qt::Key_Down:
                        decreaseSpeed();
                        return true;
                    case Qt::Key_M:
                        toggleMode();
                        return true;
                    case Qt::Key_Escape:
                        dismiss();
                        return true;
                    default:
                        break;
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AutoScrollOverlayWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (parentWidget()) {
        reposition(parentWidget()->size(), !m_hasCustomPosition);
    }
    if (m_startOnShow && !m_isActive) {
        play();
    }
}

void AutoScrollOverlayWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (m_isActive) {
        pause();
    }
}

void AutoScrollOverlayWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragClickOffset = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AutoScrollOverlayWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        if (parentWidget()) {
            const QPoint newPos = mapToParent(event->pos() - m_dragClickOffset);
            constexpr int minX = 8;
            constexpr int minY = 8;
            const int maxX = std::max(minX, parentWidget()->width() - width() - 8);
            const int maxY = std::max(minY, parentWidget()->height() - height() - 8);

            move(std::clamp(newPos.x(), minX, maxX), std::clamp(newPos.y(), minY, maxY));
            m_hasCustomPosition = true;
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void AutoScrollOverlayWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void AutoScrollOverlayWidget::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0) {
        increaseSpeed();
    } else if (event->angleDelta().y() < 0) {
        decreaseSpeed();
    }
    event->accept();
}

void AutoScrollOverlayWidget::keyPressEvent(QKeyEvent* event) {
    if (m_shortcutsEnabled) {
        switch (event->key()) {
        case Qt::Key_Space:
            togglePlayPause();
            event->accept();
            return;
        case Qt::Key_Up:
            increaseSpeed();
            event->accept();
            return;
        case Qt::Key_Down:
            decreaseSpeed();
            event->accept();
            return;
        case Qt::Key_M:
            toggleMode();
            event->accept();
            return;
        case Qt::Key_Escape:
            dismiss();
            event->accept();
            return;
        default:
            break;
        }
    }
    QWidget::keyPressEvent(event);
}

void AutoScrollOverlayWidget::setupUi() {
    setObjectName("autoScrollOverlay");
    setFixedSize(306, 44);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(6);

    // 1. Play / Pause button
    m_playPauseBtn = new QPushButton(this);
    m_playPauseBtn->setObjectName("autoScrollPlayPauseBtn");
    m_playPauseBtn->setFixedSize(30, 30);
    m_playPauseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &AutoScrollOverlayWidget::togglePlayPause);
    layout->addWidget(m_playPauseBtn);

    // Section divider
    auto* div1 = new QFrame(this);
    div1->setObjectName("autoScrollDivider");
    div1->setFrameShape(QFrame::VLine);
    layout->addWidget(div1);

    // 2. Mode toggle button
    m_modeBtn = new QPushButton(this);
    m_modeBtn->setObjectName("autoScrollModeBtn");
    m_modeBtn->setFixedSize(48, 28);
    m_modeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_modeBtn, &QPushButton::clicked, this, &AutoScrollOverlayWidget::toggleMode);
    layout->addWidget(m_modeBtn);

    // Section divider
    auto* div2 = new QFrame(this);
    div2->setObjectName("autoScrollDivider");
    div2->setFrameShape(QFrame::VLine);
    layout->addWidget(div2);

    // 3. Speed minus button
    m_speedMinusBtn = new QPushButton(this);
    m_speedMinusBtn->setObjectName("autoScrollSpeedMinusBtn");
    m_speedMinusBtn->setFixedSize(26, 26);
    m_speedMinusBtn->setCursor(Qt::PointingHandCursor);
    m_speedMinusBtn->setToolTip(tr("Decrease Speed (Down Arrow / Wheel Down)"));
    connect(m_speedMinusBtn, &QPushButton::clicked, this, &AutoScrollOverlayWidget::decreaseSpeed);
    layout->addWidget(m_speedMinusBtn);

    // 4. Speed readout label
    m_speedLabel = new QLabel(this);
    m_speedLabel->setObjectName("autoScrollSpeedLabel");
    m_speedLabel->setAlignment(Qt::AlignCenter);
    m_speedLabel->setFixedWidth(64);
    layout->addWidget(m_speedLabel);

    // 5. Speed plus button
    m_speedPlusBtn = new QPushButton(this);
    m_speedPlusBtn->setObjectName("autoScrollSpeedPlusBtn");
    m_speedPlusBtn->setFixedSize(26, 26);
    m_speedPlusBtn->setCursor(Qt::PointingHandCursor);
    m_speedPlusBtn->setToolTip(tr("Increase Speed (Up Arrow / Wheel Up)"));
    connect(m_speedPlusBtn, &QPushButton::clicked, this, &AutoScrollOverlayWidget::increaseSpeed);
    layout->addWidget(m_speedPlusBtn);

    // Section divider
    auto* div3 = new QFrame(this);
    div3->setObjectName("autoScrollDivider");
    div3->setFrameShape(QFrame::VLine);
    layout->addWidget(div3);

    // 6. Close / Dismiss button
    m_closeBtn = new QPushButton(this);
    m_closeBtn->setObjectName("autoScrollCloseBtn");
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("Close Auto-Scroll (Esc)"));
    connect(m_closeBtn, &QPushButton::clicked, this, &AutoScrollOverlayWidget::dismiss);
    layout->addWidget(m_closeBtn);
}

void AutoScrollOverlayWidget::setupStyle() {
    const QString accentHex = m_tokens.accentColor.isValid()
        ? m_tokens.accentColor.name()
        : QString("#3B82F6");

    const QString textHex = m_tokens.isDark ? "#E5E7EB" : "#1F2937";
    const QString mutedHex = m_tokens.isDark ? "#9CA3AF" : "#6B7280";
    const QString pillBg = m_tokens.isDark ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.05)";
    const QString pillBorder = m_tokens.isDark ? "rgba(255, 255, 255, 0.14)" : "rgba(0, 0, 0, 0.10)";
    const QString hoverBg = m_tokens.isDark ? "rgba(255, 255, 255, 0.16)" : "rgba(0, 0, 0, 0.10)";
    const QString pressedBg = m_tokens.isDark ? "rgba(255, 255, 255, 0.24)" : "rgba(0, 0, 0, 0.16)";

    setStyleSheet(QString(R"(
        QWidget#autoScrollOverlay {
            background: transparent;
        }
        QPushButton {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 6px;
            color: %1;
            font-size: 11px;
            font-weight: 600;
            padding: 2px 4px;
        }
        QPushButton:hover {
            background-color: %5;
            border-color: %7;
        }
        QPushButton:pressed {
            background-color: %6;
        }
        QPushButton#autoScrollPlayPauseBtn {
            background-color: %7;
            border: 1px solid %7;
            border-radius: 15px;
            color: #FFFFFF;
        }
        QPushButton#autoScrollPlayPauseBtn:hover {
            background-color: %7;
            border-color: #FFFFFF;
        }
        QPushButton#autoScrollCloseBtn {
            background: transparent;
            border: none;
            border-radius: 12px;
            color: %2;
        }
        QPushButton#autoScrollCloseBtn:hover {
            background-color: rgba(239, 68, 68, 0.20);
            color: #EF4444;
        }
        QLabel#autoScrollSpeedLabel {
            color: %1;
            font-size: 11px;
            font-weight: 700;
            background: transparent;
            padding: 0 2px;
        }
        QFrame#autoScrollDivider {
            background-color: %4;
            max-width: 1px;
            min-width: 1px;
            border: none;
        }
    )")
    .arg(textHex)
    .arg(mutedHex)
    .arg(pillBg)
    .arg(pillBorder)
    .arg(hoverBg)
    .arg(pressedBg)
    .arg(accentHex));

    // Update icons with matching theme color
    const QColor iconColor = m_tokens.isDark ? QColor(229, 231, 235) : QColor(31, 41, 55);
    const QColor closeIconColor = m_tokens.isDark ? QColor(156, 163, 175) : QColor(107, 114, 128);

    m_speedMinusBtn->setIcon(drawMinusIcon(iconColor));
    m_speedPlusBtn->setIcon(drawPlusIcon(iconColor));
    m_closeBtn->setIcon(drawCloseIcon(closeIconColor));
    updateModeButtonUi();
    updatePlayPauseUi();
}

void AutoScrollOverlayWidget::updatePlayPauseUi() {
    if (!m_playPauseBtn) return;
    if (m_isActive) {
        m_playPauseBtn->setIcon(drawPauseIcon(Qt::white));
        m_playPauseBtn->setToolTip(tr("Pause Auto-Scroll (Space or Tap screen)"));
    } else {
        m_playPauseBtn->setIcon(drawPlayIcon(Qt::white));
        m_playPauseBtn->setToolTip(tr("Start Auto-Scroll (Space or Tap screen)"));
    }
}

void AutoScrollOverlayWidget::updateSpeedLabel() {
    if (!m_speedLabel) return;
    if (m_mode == AutoScrollMode::PixelByPixel) {
        m_speedLabel->setText(QString("%1 px/s").arg(m_pixelSpeed));
        m_speedLabel->setToolTip(tr("Continuous Roll Speed: %1 px/s\nRange: 5 to 200 px/s (Default 30 px/s)").arg(m_pixelSpeed));
    } else if (m_mode == AutoScrollMode::LineByLine) {
        m_speedLabel->setText(m_lineSpeed == 1 ? tr("1 line/s") : tr("%1 lines/s").arg(m_lineSpeed));
        m_speedLabel->setToolTip(tr("Line Roll Speed: %1 lines/sec\nRange: 1 to 15 lines/s (Default 2 lines/s)").arg(m_lineSpeed));
    } else {
        m_speedLabel->setText(QString("%1s / pg").arg(m_pageIntervalSec));
        m_speedLabel->setToolTip(tr("Timed Page Turn Interval: %1 seconds per page\nRange: 3 to 90 seconds").arg(m_pageIntervalSec));
    }
}

void AutoScrollOverlayWidget::updateModeButtonUi() {
    if (!m_modeBtn) return;
    const QColor iconColor = m_tokens.isDark ? QColor(229, 231, 235) : QColor(31, 41, 55);
    m_modeBtn->setIcon(drawModeIcon(m_mode, iconColor));
    if (m_mode == AutoScrollMode::PixelByPixel) {
        m_modeBtn->setText(tr("PX"));
        m_modeBtn->setToolTip(tr("Mode: Pixel-by-pixel continuous roll\nClick or press 'M' to switch to Line roll"));
    } else if (m_mode == AutoScrollMode::LineByLine) {
        m_modeBtn->setText(tr("LN"));
        m_modeBtn->setToolTip(tr("Mode: Line-by-line roll\nClick or press 'M' to switch to Page turn"));
    } else {
        m_modeBtn->setText(tr("PG"));
        m_modeBtn->setToolTip(tr("Mode: Timed Page-by-page turn\nClick or press 'M' to switch to Pixel roll"));
    }
}

QIcon AutoScrollOverlayWidget::drawPlayIcon(const QColor& color) const {
    constexpr int s = 20;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    QPainterPath path;
    path.moveTo(6.5, 4.5);
    path.lineTo(15.5, 10.0);
    path.lineTo(6.5, 15.5);
    path.closeSubpath();
    p.drawPath(path);

    return QIcon(pix);
}

QIcon AutoScrollOverlayWidget::drawPauseIcon(const QColor& color) const {
    constexpr int s = 20;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    p.drawRoundedRect(QRectF(5.5, 4.5, 3.0, 11.0), 1.2, 1.2);
    p.drawRoundedRect(QRectF(11.5, 4.5, 3.0, 11.0), 1.2, 1.2);

    return QIcon(pix);
}

QIcon AutoScrollOverlayWidget::drawMinusIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(4.0, 8.0), QPointF(12.0, 8.0));

    return QIcon(pix);
}

QIcon AutoScrollOverlayWidget::drawPlusIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(4.0, 8.0), QPointF(12.0, 8.0));
    p.drawLine(QPointF(8.0, 4.0), QPointF(8.0, 12.0));

    return QIcon(pix);
}

QIcon AutoScrollOverlayWidget::drawModeIcon(AutoScrollMode mode, const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    if (mode == AutoScrollMode::PixelByPixel) {
        // Continuous smooth flow indicator arrow
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(8.0, 3.5), QPointF(8.0, 12.5));
        p.drawLine(QPointF(5.5, 10.0), QPointF(8.0, 12.5));
        p.drawLine(QPointF(10.5, 10.0), QPointF(8.0, 12.5));
    } else if (mode == AutoScrollMode::LineByLine) {
        // Line-by-line discrete paragraph lines
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(3.0, 4.5), QPointF(13.0, 4.5));
        p.drawLine(QPointF(3.0, 8.5), QPointF(13.0, 8.5));
        p.drawLine(QPointF(3.0, 12.5), QPointF(13.0, 12.5));
    } else {
        // Page-by-page open book / flip icon
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawRect(QRectF(3.5, 3.5, 9.0, 9.0));
        p.drawLine(QPointF(8.0, 3.5), QPointF(8.0, 12.5));
    }

    return QIcon(pix);
}

QIcon AutoScrollOverlayWidget::drawCloseIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(4.0, 4.0), QPointF(12.0, 12.0));
    p.drawLine(QPointF(12.0, 4.0), QPointF(4.0, 12.0));

    return QIcon(pix);
}

} // namespace LunaRead
