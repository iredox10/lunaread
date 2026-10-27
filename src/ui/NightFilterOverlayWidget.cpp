#include "NightFilterOverlayWidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <algorithm>

namespace LunaRead {

NightFilterOverlayWidget::NightFilterOverlayWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    setupHud();
}

void NightFilterOverlayWidget::setupHud() {
    m_hudCard = new QFrame(this);
    m_hudCard->setFixedSize(290, 195);
    m_hudCard->setStyleSheet(
        "QFrame {"
        "  background-color: rgba(30, 41, 59, 235);"
        "  border: 1px solid rgba(255, 255, 255, 35);"
        "  border-radius: 12px;"
        "}"
        "QLabel { color: #F1F5F9; font-size: 10pt; background: transparent; border: none; }"
        "QSlider::groove:horizontal {"
        "  height: 6px; background: rgba(255, 255, 255, 40); border-radius: 3px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #3B82F6; border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #FFFFFF; width: 16px; margin-top: -5px; margin-bottom: -5px; border-radius: 8px;"
        "}"
    );

    auto* layout = new QVBoxLayout(m_hudCard);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    // Title row with close button
    auto* titleRow = new QHBoxLayout();
    auto* titleLabel = new QLabel(tr("🌙 <b>Night Care Filter</b>"), m_hudCard);
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();

    m_closeBtn = new QPushButton(QStringLiteral("✕"), m_hudCard);
    m_closeBtn->setFixedSize(22, 22);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet("QPushButton { border: none; color: #94A3B8; font-weight: bold; background: transparent; }"
                              "QPushButton:hover { color: #EF4444; }");
    connect(m_closeBtn, &QPushButton::clicked, this, &NightFilterOverlayWidget::hideControls);
    titleRow->addWidget(m_closeBtn);
    layout->addLayout(titleRow);

    // Brightness row
    auto* brightHeader = new QHBoxLayout();
    brightHeader->addWidget(new QLabel(tr("Brightness:"), m_hudCard));
    brightHeader->addStretch();
    m_brightnessValLabel = new QLabel(QStringLiteral("100%"), m_hudCard);
    brightHeader->addWidget(m_brightnessValLabel);
    layout->addLayout(brightHeader);

    m_brightnessSlider = new QSlider(Qt::Horizontal, m_hudCard);
    m_brightnessSlider->setRange(15, 100);
    m_brightnessSlider->setValue(m_brightness);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &NightFilterOverlayWidget::setBrightness);
    layout->addWidget(m_brightnessSlider);

    // Warmness row
    auto* warmHeader = new QHBoxLayout();
    warmHeader->addWidget(new QLabel(tr("Amber Warmth:"), m_hudCard));
    warmHeader->addStretch();
    m_warmnessValLabel = new QLabel(QStringLiteral("35%"), m_hudCard);
    warmHeader->addWidget(m_warmnessValLabel);
    layout->addLayout(warmHeader);

    m_warmnessSlider = new QSlider(Qt::Horizontal, m_hudCard);
    m_warmnessSlider->setRange(0, 100);
    m_warmnessSlider->setValue(m_warmness);
    connect(m_warmnessSlider, &QSlider::valueChanged, this, &NightFilterOverlayWidget::setWarmness);
    layout->addWidget(m_warmnessSlider);

    // Bottom buttons
    auto* btnRow = new QHBoxLayout();
    m_toggleBtn = new QPushButton(m_enabled ? tr("Turn Off") : tr("Turn On"), m_hudCard);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setStyleSheet("QPushButton { background-color: #2563EB; color: white; border: none; border-radius: 6px; padding: 6px 12px; font-weight: bold; }"
                              "QPushButton:hover { background-color: #1D4ED8; }");
    connect(m_toggleBtn, &QPushButton::clicked, this, &NightFilterOverlayWidget::toggleFilter);
    btnRow->addWidget(m_toggleBtn);

    m_resetBtn = new QPushButton(tr("Reset"), m_hudCard);
    m_resetBtn->setCursor(Qt::PointingHandCursor);
    m_resetBtn->setStyleSheet("QPushButton { background-color: rgba(255, 255, 255, 20); color: #E2E8F0; border: 1px solid rgba(255, 255, 255, 40); border-radius: 6px; padding: 6px 12px; }"
                             "QPushButton:hover { background-color: rgba(255, 255, 255, 35); }");
    connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
        m_brightnessSlider->setValue(100);
        m_warmnessSlider->setValue(35);
    });
    btnRow->addWidget(m_resetBtn);
    layout->addLayout(btnRow);

    m_hudCard->hide();
}

void NightFilterOverlayWidget::updateHudPosition() {
    if (m_hudCard && !m_draggingHud) {
        m_hudCard->move(width() - m_hudCard->width() - 24, 70);
    }
}

void NightFilterOverlayWidget::setFilterEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        m_toggleBtn->setText(m_enabled ? tr("Turn Off") : tr("Turn On"));
        emit filterChanged(m_enabled, m_brightness, m_warmness);
        update();
    }
}

void NightFilterOverlayWidget::toggleFilter() {
    setFilterEnabled(!m_enabled);
}

void NightFilterOverlayWidget::setBrightness(int percentage) {
    m_brightness = std::clamp(percentage, 15, 100);
    if (m_brightnessValLabel) {
        m_brightnessValLabel->setText(QString("%1%").arg(m_brightness));
    }
    if (m_brightnessSlider && m_brightnessSlider->value() != m_brightness) {
        m_brightnessSlider->setValue(m_brightness);
    }
    if (m_enabled) {
        update();
        emit filterChanged(m_enabled, m_brightness, m_warmness);
    }
}

void NightFilterOverlayWidget::setWarmness(int percentage) {
    m_warmness = std::clamp(percentage, 0, 100);
    if (m_warmnessValLabel) {
        m_warmnessValLabel->setText(QString("%1%").arg(m_warmness));
    }
    if (m_warmnessSlider && m_warmnessSlider->value() != m_warmness) {
        m_warmnessSlider->setValue(m_warmness);
    }
    if (m_enabled) {
        update();
        emit filterChanged(m_enabled, m_brightness, m_warmness);
    }
}

void NightFilterOverlayWidget::showControls() {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    updateHudPosition();
    m_hudCard->show();
    m_hudCard->raise();
}

void NightFilterOverlayWidget::hideControls() {
    m_hudCard->hide();
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void NightFilterOverlayWidget::toggleControls() {
    if (m_hudCard->isVisible()) hideControls();
    else showControls();
}

void NightFilterOverlayWidget::paintEvent(QPaintEvent* /*event*/) {
    if (!m_enabled) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 1. Amber blue light reduction tint
    if (m_warmness > 0) {
        int amberAlpha = static_cast<int>(m_warmness * 0.9); // up to ~90/255
        QColor amberColor(255, 140, 0, amberAlpha);
        painter.fillRect(rect(), amberColor);
    }

    // 2. Extra software dimming
    if (m_brightness < 100) {
        int dimAlpha = static_cast<int>((100 - m_brightness) * 2.15); // up to ~182/255
        QColor dimColor(0, 0, 0, dimAlpha);
        painter.fillRect(rect(), dimColor);
    }
}

void NightFilterOverlayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateHudPosition();
}

void NightFilterOverlayWidget::mousePressEvent(QMouseEvent* event) {
    if (m_hudCard && m_hudCard->isVisible()) {
        if (m_hudCard->geometry().contains(event->pos())) {
            m_draggingHud = true;
            m_dragStartPos = event->pos() - m_hudCard->pos();
            event->accept();
            return;
        } else {
            // Click outside dismisses HUD controls while keeping filter active
            hideControls();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void NightFilterOverlayWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_draggingHud && m_hudCard) {
        m_hudCard->move(event->pos() - m_dragStartPos);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void NightFilterOverlayWidget::mouseReleaseEvent(QMouseEvent* event) {
    m_draggingHud = false;
    QWidget::mouseReleaseEvent(event);
}

} // namespace LunaRead
