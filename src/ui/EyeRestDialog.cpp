#include "EyeRestDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <algorithm>

namespace LunaRead {

// ============================================================================
// EyeRelaxationGraphicWidget Implementation
// ============================================================================

EyeRelaxationGraphicWidget::EyeRelaxationGraphicWidget(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void EyeRelaxationGraphicWidget::setCountdown(int remainingSeconds, int totalSeconds) {
    m_remainingSeconds = std::max(0, remainingSeconds);
    m_totalSeconds = std::max(1, totalSeconds);
    update();
}

void EyeRelaxationGraphicWidget::setColors(const QColor& ringColor, const QColor& trackColor, const QColor& textColor) {
    m_ringColor = ringColor;
    m_trackColor = trackColor;
    m_textColor = textColor;
    update();
}

void EyeRelaxationGraphicWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal w = static_cast<qreal>(width());
    const qreal h = static_cast<qreal>(height());
    const QPointF center(w * 0.5, h * 0.5);
    const qreal radius = (std::min(w, h) * 0.5) - 10.0;

    // 1. Draw outer background track circle
    QPen trackPen(m_trackColor, 5.0, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);

    // 2. Draw animated countdown arc
    if (m_totalSeconds > 0 && m_remainingSeconds > 0) {
        qreal fraction = static_cast<qreal>(m_remainingSeconds) / static_cast<qreal>(m_totalSeconds);
        fraction = std::clamp(fraction, 0.0, 1.0);
        const int startAngle = 90 * 16; // 12 o'clock position
        const int spanAngle = static_cast<int>(-360.0 * 16.0 * fraction);

        QPen ringPen(m_ringColor, 6.0, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(ringPen);
        const QRectF arcRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
        painter.drawArc(arcRect, startAngle, spanAngle);
    }

    // 3. Draw Stylized Calming Eye Graphic & Countdown Info
    if (m_remainingSeconds > 0) {
        const qreal eyeY = center.y() - 14.0;
        const qreal eyeW = 34.0;
        const qreal eyeH = 18.0;

        // Eyelid upper curve
        QPainterPath eyelidPath;
        eyelidPath.moveTo(center.x() - eyeW * 0.5, eyeY);
        eyelidPath.cubicTo(center.x() - eyeW * 0.25, eyeY - eyeH * 0.5,
                           center.x() + eyeW * 0.25, eyeY - eyeH * 0.5,
                           center.x() + eyeW * 0.5, eyeY);
        // Eyelid lower curve
        eyelidPath.cubicTo(center.x() + eyeW * 0.25, eyeY + eyeH * 0.5,
                           center.x() - eyeW * 0.25, eyeY + eyeH * 0.5,
                           center.x() - eyeW * 0.5, eyeY);

        QPen eyePen(m_ringColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(eyePen);
        painter.setBrush(QColor(m_ringColor.red(), m_ringColor.green(), m_ringColor.blue(), 25));
        painter.drawPath(eyelidPath);

        // Iris
        QRadialGradient irisGrad(center.x(), eyeY, 6.0);
        irisGrad.setColorAt(0.0, m_ringColor.lighter(135));
        irisGrad.setColorAt(1.0, m_ringColor);
        painter.setPen(Qt::NoPen);
        painter.setBrush(irisGrad);
        painter.drawEllipse(QPointF(center.x(), eyeY), 5.5, 5.5);

        // Pupil & Celestial Glint
        painter.setBrush(QColor(15, 23, 42));
        painter.drawEllipse(QPointF(center.x(), eyeY), 2.5, 2.5);
        painter.setBrush(Qt::white);
        painter.drawEllipse(QPointF(center.x() + 1.2, eyeY - 1.2), 1.0, 1.0);

        // Countdown seconds display
        painter.setPen(m_textColor);
        QFont numFont = painter.font();
        numFont.setPointSize(18);
        numFont.setBold(true);
        painter.setFont(numFont);

        const QString timeStr = QString::number(m_remainingSeconds) + QStringLiteral("s");
        const QRectF textRect(center.x() - 40.0, center.y() + 6.0, 80.0, 24.0);
        painter.drawText(textRect, Qt::AlignCenter, timeStr);

        // Subtitle label
        QFont subFont = painter.font();
        subFont.setPointSize(8);
        subFont.setBold(false);
        painter.setFont(subFont);
        painter.setPen(QColor(m_textColor.red(), m_textColor.green(), m_textColor.blue(), 170));
        const QRectF subRect(center.x() - 40.0, center.y() + 28.0, 80.0, 14.0);
        painter.drawText(subRect, Qt::AlignCenter, QStringLiteral("rest"));
    } else {
        // Rest completed checkmark
        QPen checkPen(m_ringColor, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(checkPen);
        painter.setBrush(Qt::NoBrush);

        QPainterPath checkPath;
        checkPath.moveTo(center.x() - 14.0, center.y() - 2.0);
        checkPath.lineTo(center.x() - 4.0, center.y() + 8.0);
        checkPath.lineTo(center.x() + 16.0, center.y() - 12.0);
        painter.drawPath(checkPath);

        painter.setPen(m_ringColor);
        QFont doneFont = painter.font();
        doneFont.setPointSize(11);
        doneFont.setBold(true);
        painter.setFont(doneFont);

        const QRectF doneRect(center.x() - 50.0, center.y() + 14.0, 100.0, 20.0);
        painter.drawText(doneRect, Qt::AlignCenter, QStringLiteral("Rested!"));
    }
}

// ============================================================================
// EyeRestDialog Implementation
// ============================================================================

EyeRestDialog::EyeRestDialog(int sessionMinutesRead, QWidget* parent)
    : QDialog(parent),
      m_sessionMinutesRead(sessionMinutesRead),
      m_timer(new QTimer(this)) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    connect(m_timer, &QTimer::timeout, this, &EyeRestDialog::onTimerTick);

    setupUi();
    updateStyles();
    setSessionMinutesRead(sessionMinutesRead);
    updateTimerDisplay();
}

void EyeRestDialog::setSessionMinutesRead(int minutes) {
    m_sessionMinutesRead = minutes;
    if (m_sessionLabel) {
        m_sessionLabel->setText(tr("You've been reading continuously for %1 minutes.").arg(m_sessionMinutesRead));
    }
}

void EyeRestDialog::setCountdownSeconds(int seconds) {
    m_totalCountdownSeconds = std::max(1, seconds);
    m_remainingSeconds = m_totalCountdownSeconds;
    updateTimerDisplay();
}

void EyeRestDialog::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addStretch(1);

    auto* centerRow = new QHBoxLayout();
    centerRow->setContentsMargins(0, 0, 0, 0);
    centerRow->addStretch(1);

    m_centralCard = new QWidget(this);
    m_centralCard->setFixedWidth(440);
    m_centralCard->setAttribute(Qt::WA_TranslucentBackground, true);

    auto* cardLayout = new QVBoxLayout(m_centralCard);
    cardLayout->setContentsMargins(28, 28, 28, 24);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignCenter);

    // 1. Tag / Badge
    m_badgeLabel = new QLabel(tr("✦ 20-20-20 EYE HEALTH RULE ✦"), m_centralCard);
    m_badgeLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_badgeLabel);

    // 2. Title
    m_titleLabel = new QLabel(tr("Take a Break"), m_centralCard);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = font();
    titleFont.setPointSize(19);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    cardLayout->addWidget(m_titleLabel);

    // 3. Session info
    m_sessionLabel = new QLabel(m_centralCard);
    m_sessionLabel->setAlignment(Qt::AlignCenter);
    QFont sessionFont = font();
    sessionFont.setPointSize(10);
    m_sessionLabel->setFont(sessionFont);
    cardLayout->addWidget(m_sessionLabel);

    // 4. Calming eye graphic & countdown ring
    m_graphicWidget = new EyeRelaxationGraphicWidget(m_centralCard);
    m_graphicWidget->setFixedSize(150, 150);
    cardLayout->addWidget(m_graphicWidget, 0, Qt::AlignCenter);

    // 5. Advice / Instruction
    m_adviceLabel = new QLabel(
        tr("Look at an object at least 20 feet (6 meters) away for 20 seconds to relax your eye muscles and blink naturally."),
        m_centralCard);
    m_adviceLabel->setAlignment(Qt::AlignCenter);
    m_adviceLabel->setWordWrap(true);
    QFont adviceFont = font();
    adviceFont.setPointSize(9);
    m_adviceLabel->setFont(adviceFont);
    cardLayout->addWidget(m_adviceLabel);

    // 6. Action button row
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);

    m_skipBtn = new QPushButton(tr("I'm back / Skip"), m_centralCard);
    m_skipBtn->setFixedHeight(42);
    m_skipBtn->setCursor(Qt::PointingHandCursor);
    connect(m_skipBtn, &QPushButton::clicked, this, &EyeRestDialog::onSkipClicked);

    m_snoozeBtn = new QPushButton(tr("Snooze (15m)"), m_centralCard);
    m_snoozeBtn->setFixedHeight(42);
    m_snoozeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_snoozeBtn, &QPushButton::clicked, this, &EyeRestDialog::onSnoozeClicked);

    btnLayout->addWidget(m_skipBtn, 1);
    btnLayout->addWidget(m_snoozeBtn, 1);
    cardLayout->addLayout(btnLayout);

    // 7. Mute button
    m_muteBtn = new QPushButton(tr("Mute for this session"), m_centralCard);
    m_muteBtn->setCursor(Qt::PointingHandCursor);
    m_muteBtn->setFlat(true);
    connect(m_muteBtn, &QPushButton::clicked, this, &EyeRestDialog::onMuteClicked);
    cardLayout->addWidget(m_muteBtn, 0, Qt::AlignCenter);

    centerRow->addWidget(m_centralCard);
    centerRow->addStretch(1);

    outerLayout->addLayout(centerRow);
    outerLayout->addStretch(1);
}

void EyeRestDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    m_hasCustomTokens = true;
    updateStyles();
    update();
}

void EyeRestDialog::updateStyles() {
    const bool isLight = m_hasCustomTokens && !m_tokens.isDark;
    const QColor accent = m_hasCustomTokens ? m_tokens.accentColor : QColor(16, 185, 129);
    const QColor titleColor = isLight ? QColor(15, 23, 42) : QColor(241, 245, 249);
    const QColor subColor = isLight ? QColor(71, 85, 105) : QColor(148, 163, 184);

    // Badge styling
    m_badgeLabel->setStyleSheet(QStringLiteral(
        "background-color: rgba(16, 185, 129, 0.16); "
        "color: #10B981; "
        "border: 1px solid rgba(16, 185, 129, 0.35); "
        "border-radius: 11px; "
        "padding: 3px 12px; "
        "font-size: 8.5pt; "
        "font-weight: bold;"));

    // Title & text styling
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(titleColor.name()));
    m_sessionLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(subColor.name()));
    m_adviceLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent; line-height: 1.4;").arg(subColor.name()));

    // Buttons styling
    const QString primaryStyle = QStringLiteral(
        "QPushButton { "
        "    background-color: %1; "
        "    color: white; "
        "    font-weight: bold; "
        "    font-size: 10pt; "
        "    border: none; "
        "    border-radius: 8px; "
        "    padding: 8px 16px; "
        "} "
        "QPushButton:hover { background-color: %2; } "
        "QPushButton:pressed { background-color: %3; }")
        .arg(accent.name(), accent.lighter(115).name(), accent.darker(115).name());
    m_skipBtn->setStyleSheet(primaryStyle);

    const QString secondaryStyle = isLight
        ? QStringLiteral(
            "QPushButton { "
            "    background-color: rgba(0, 0, 0, 0.05); "
            "    color: #1E293B; "
            "    font-size: 10pt; "
            "    border: 1px solid rgba(0, 0, 0, 0.15); "
            "    border-radius: 8px; "
            "    padding: 8px 16px; "
            "} "
            "QPushButton:hover { background-color: rgba(0, 0, 0, 0.10); }")
        : QStringLiteral(
            "QPushButton { "
            "    background-color: rgba(255, 255, 255, 0.08); "
            "    color: #F1F5F9; "
            "    font-size: 10pt; "
            "    border: 1px solid rgba(255, 255, 255, 0.20); "
            "    border-radius: 8px; "
            "    padding: 8px 16px; "
            "} "
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.15); }");
    m_snoozeBtn->setStyleSheet(secondaryStyle);

    m_muteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { "
        "    background: transparent; "
        "    color: %1; "
        "    font-size: 9pt; "
        "    border: none; "
        "    text-decoration: underline; "
        "} "
        "QPushButton:hover { color: %2; }")
        .arg(subColor.name(), titleColor.name()));

    // Update graphic widget colors
    if (m_graphicWidget) {
        const QColor trackCol = isLight ? QColor(0, 0, 0, 25) : QColor(255, 255, 255, 30);
        m_graphicWidget->setColors(accent, trackCol, titleColor);
    }
}

void EyeRestDialog::startCountdown(int seconds) {
    m_totalCountdownSeconds = seconds > 0 ? seconds : 20;
    m_remainingSeconds = m_totalCountdownSeconds;
    m_skipBtn->setText(tr("I'm back / Skip"));
    updateTimerDisplay();
    if (!m_timer->isActive()) {
        m_timer->start(1000);
    }
}

void EyeRestDialog::pauseCountdown() {
    if (m_timer->isActive()) {
        m_timer->stop();
    }
}

void EyeRestDialog::resumeCountdown() {
    if (!m_timer->isActive() && m_remainingSeconds > 0) {
        m_timer->start(1000);
    }
}

void EyeRestDialog::resetCountdown() {
    startCountdown(m_totalCountdownSeconds);
}

void EyeRestDialog::updateTimerDisplay() {
    if (m_graphicWidget) {
        m_graphicWidget->setCountdown(m_remainingSeconds, m_totalCountdownSeconds);
    }
}

void EyeRestDialog::onTimerTick() {
    if (m_remainingSeconds > 0) {
        m_remainingSeconds--;
        updateTimerDisplay();

        if (m_remainingSeconds == 0) {
            m_timer->stop();
            m_skipBtn->setText(tr("Resume Reading"));
            emit restCompleted();
        }
    }
}

void EyeRestDialog::onSkipClicked() {
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    if (m_remainingSeconds > 0) {
        emit restSkipped();
    } else {
        emit restCompleted();
    }
    accept();
}

void EyeRestDialog::onSnoozeClicked() {
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    emit snoozeRequested(15);
    done(QDialog::Accepted);
}

void EyeRestDialog::onMuteClicked() {
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    emit muteRequested();
    reject();
}

void EyeRestDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Calming dark translucent backdrop to immediately diminish bright screen glare
    p.fillRect(rect(), QColor(10, 15, 26, 215));

    // 2. Rounded card background
    if (m_centralCard) {
        const QRect cardRect = m_centralCard->geometry();
        const bool isLight = m_hasCustomTokens && !m_tokens.isDark;

        const QColor cardBg = isLight ? QColor(255, 255, 255, 248)
                                      : QColor(24, 30, 44, 248);
        const QColor cardBorder = isLight ? QColor(226, 232, 240, 220)
                                          : QColor(255, 255, 255, 35);

        p.setPen(QPen(cardBorder, 1.5));
        p.setBrush(cardBg);
        p.drawRoundedRect(QRectF(cardRect), 20.0, 20.0);
    }
}

void EyeRestDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        onSkipClicked();
        event->accept();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        onSkipClicked();
        event->accept();
    } else {
        QDialog::keyPressEvent(event);
    }
}

void EyeRestDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
    } else if (width() < 480 || height() < 520) {
        resize(480, 520);
    }
    startCountdown(m_totalCountdownSeconds);
}

void EyeRestDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    update();
}

} // namespace LunaRead
