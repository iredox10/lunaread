#include "TtsControlBar.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

namespace LunaRead {

TtsControlBar::TtsControlBar(QWidget* parent)
    : QWidget(parent)
    , m_tokens(ReaderThemeTokens::night())
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);

    // Subtle drop shadow for floating elevation
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0, 0, 0, 110));
    setGraphicsEffect(shadow);

    if (parent) {
        parent->installEventFilter(this);
    }

    m_sleepCountdownTimer = new QTimer(this);
    m_sleepCountdownTimer->setInterval(1000);
    connect(m_sleepCountdownTimer, &QTimer::timeout, this, &TtsControlBar::onSleepTimerTick);

    setupUi();
    setupStyle();
    updateSentenceCounterUi(0, 0);
    updatePlayPauseUi(false);
}

TtsControlBar::TtsControlBar(TtsService* service, QWidget* parent)
    : TtsControlBar(parent)
{
    setTtsService(service);
}

TtsControlBar::~TtsControlBar() {
    if (m_sleepCountdownTimer && m_sleepCountdownTimer->isActive()) {
        m_sleepCountdownTimer->stop();
    }
}

void TtsControlBar::setTtsService(TtsService* service) {
    if (m_ttsService == service) return;

    if (m_ttsService) {
        disconnect(m_ttsService, nullptr, this, nullptr);
    }

    m_ttsService = service;

    if (m_ttsService) {
        connect(m_ttsService, &TtsService::activeSentenceChanged,
                this, &TtsControlBar::onActiveSentenceChanged);
        connect(m_ttsService, &TtsService::playbackStateChanged,
                this, &TtsControlBar::onPlaybackStateChanged);
        connect(m_ttsService, &TtsService::playbackFinished,
                this, &TtsControlBar::onPlaybackFinished);
        connect(m_ttsService, &TtsService::sentencesLoaded,
                this, &TtsControlBar::onSentencesLoaded);

        m_totalSentences = m_ttsService->sentenceCount();
        m_currentIndex = m_ttsService->currentIndex();
        m_currentRate = m_ttsService->playbackRate();

        setSpeechRate(m_currentRate);
        updatePlayPauseUi(m_ttsService->isPlaying());

        if (m_totalSentences > 0) {
            m_progressSlider->setRange(0, std::max(0, m_totalSentences - 1));
            m_progressSlider->setValue(m_currentIndex);
            updateSentenceCounterUi(m_currentIndex + 1, m_totalSentences);
            if (m_currentIndex >= 0 && m_currentIndex < m_ttsService->sentences().size()) {
                m_karaokePreviewLabel->setText(m_ttsService->sentences().at(m_currentIndex));
            }
        } else {
            m_progressSlider->setRange(0, 0);
            m_progressSlider->setValue(0);
            updateSentenceCounterUi(0, 0);
            m_karaokePreviewLabel->setText(tr("No text loaded for TTS"));
        }
    }
}

void TtsControlBar::updateTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    setupStyle();
    update();
}

void TtsControlBar::reposition(const QSize& parentSize) {
    if (parentSize.width() <= 0 || parentSize.height() <= 0) return;

    const int targetWidth = std::clamp(parentSize.width() - 48, 420, 640);
    const int targetHeight = 104;
    const int x = (parentSize.width() - targetWidth) / 2;
    const int y = parentSize.height() - targetHeight - 20;

    setGeometry(x, y, targetWidth, targetHeight);
    raise();
}

bool TtsControlBar::isPlaying() const noexcept {
    return m_ttsService ? m_ttsService->isPlaying() : false;
}

double TtsControlBar::speechRate() const noexcept {
    return m_ttsService ? m_ttsService->playbackRate() : m_currentRate;
}

QString TtsControlBar::activeSentenceText() const {
    return m_karaokePreviewLabel ? m_karaokePreviewLabel->text() : QString{};
}

void TtsControlBar::play() {
    if (m_ttsService) {
        m_ttsService->play();
    }
}

void TtsControlBar::pause() {
    if (m_ttsService) {
        m_ttsService->pause();
    }
}

void TtsControlBar::togglePlayPause() {
    if (!m_ttsService) return;
    if (m_ttsService->isPlaying()) {
        m_ttsService->pause();
    } else {
        m_ttsService->play();
    }
}

void TtsControlBar::stop() {
    if (m_ttsService) {
        m_ttsService->stop();
    }
}

void TtsControlBar::nextSentence() {
    if (m_ttsService) {
        m_ttsService->nextSentence();
    }
}

void TtsControlBar::prevSentence() {
    if (m_ttsService) {
        m_ttsService->prevSentence();
    }
}

void TtsControlBar::seekToSentence(int index) {
    if (m_ttsService) {
        m_ttsService->seekSentence(index);
    }
}

void TtsControlBar::setSpeechRate(double rate) {
    m_currentRate = rate;
    if (m_ttsService) {
        m_ttsService->setPlaybackRate(rate);
    }

    if (m_rateBtn) {
        QString text;
        if (std::abs(rate - 1.0) < 0.01) text = "1.0x";
        else if (std::abs(rate - 1.25) < 0.01) text = "1.25x";
        else if (std::abs(rate - 1.5) < 0.01) text = "1.5x";
        else if (std::abs(rate - 2.0) < 0.01) text = "2.0x";
        else text = QString::number(rate, 'f', 2) + "x";

        m_rateBtn->setText(text);
    }

    // Update checked action in menu
    if (m_rateMenu) {
        for (QAction* action : m_rateMenu->actions()) {
            if (std::abs(action->data().toDouble() - rate) < 0.01) {
                action->setChecked(true);
                break;
            }
        }
    }

    emit speechRateChanged(rate);
}

void TtsControlBar::setSleepTimer(SleepTimerOption option) {
    m_sleepOption = option;

    if (option == SleepTimerOption::Off) {
        m_sleepCountdownTimer->stop();
        m_sleepSecondsRemaining = 0;
    } else if (option == SleepTimerOption::EndOfChapter) {
        m_sleepCountdownTimer->stop();
        m_sleepSecondsRemaining = 0;
    } else {
        const int minutes = static_cast<int>(option);
        m_sleepSecondsRemaining = minutes * 60;
        m_sleepCountdownTimer->start();
    }

    updateSleepTimerUi();
    emit sleepTimerChanged(option);
}

void TtsControlBar::dismiss() {
    stop();
    hide();
    emit closeRequested();
}

void TtsControlBar::onActiveSentenceChanged(int index, const QString& sentence) {
    m_currentIndex = index;
    if (!m_isSliderDragging && m_progressSlider) {
        m_progressSlider->setValue(index);
    }

    updateSentenceCounterUi(index + 1, m_totalSentences);

    if (m_karaokePreviewLabel) {
        m_karaokePreviewLabel->setText(sentence);
        m_karaokePreviewLabel->setToolTip(sentence);
    }

    if (m_prevBtn) {
        m_prevBtn->setEnabled(index > 0);
    }
    if (m_nextBtn) {
        m_nextBtn->setEnabled(index + 1 < m_totalSentences);
    }

    emit sentenceHighlighted(index, sentence);
}

void TtsControlBar::onPlaybackStateChanged(bool isPlaying) {
    updatePlayPauseUi(isPlaying);
    emit playbackStateChanged(isPlaying);
}

void TtsControlBar::onPlaybackFinished() {
    updatePlayPauseUi(false);
    if (m_sleepOption == SleepTimerOption::EndOfChapter) {
        setSleepTimer(SleepTimerOption::Off);
        emit sleepTimerTriggered();
    }
}

void TtsControlBar::onSentencesLoaded(int count) {
    m_totalSentences = count;
    m_currentIndex = 0;

    if (m_progressSlider) {
        m_progressSlider->setRange(0, std::max(0, count - 1));
        m_progressSlider->setValue(0);
    }

    updateSentenceCounterUi(count > 0 ? 1 : 0, count);

    if (m_prevBtn) m_prevBtn->setEnabled(false);
    if (m_nextBtn) m_nextBtn->setEnabled(count > 1);
}

void TtsControlBar::onSleepTimerTick() {
    if (m_sleepSecondsRemaining > 0) {
        m_sleepSecondsRemaining--;
        updateSleepTimerUi();

        if (m_sleepSecondsRemaining == 0) {
            m_sleepCountdownTimer->stop();
            m_sleepOption = SleepTimerOption::Off;
            updateSleepTimerUi();
            stop();
            emit sleepTimerTriggered();
        }
    }
}

void TtsControlBar::onSliderValueChanged(int value) {
    if (m_isSliderDragging) {
        updateSentenceCounterUi(value + 1, m_totalSentences);
    }
}

void TtsControlBar::onProgressSliderReleased() {
    m_isSliderDragging = false;
    if (m_progressSlider) {
        seekToSentence(m_progressSlider->value());
    }
}

void TtsControlBar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = rect().adjusted(1, 1, -1, -1);
    constexpr qreal cornerRadius = 16.0;

    QPainterPath path;
    path.addRoundedRect(bounds, cornerRadius, cornerRadius);

    // Frosted glass background fill
    QColor bgColor;
    QColor outlineColor;
    if (m_tokens.isDark) {
        bgColor = QColor(22, 27, 34, 225);
        outlineColor = QColor(255, 255, 255, 36);
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
        gloss.setColorAt(0.0, QColor(255, 255, 255, 20));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    } else {
        gloss.setColorAt(0.0, QColor(255, 255, 255, 60));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    }
    painter.fillPath(glossPath, gloss);

    // Subtle 1px frosted border outline
    QPen borderPen(outlineColor, 1.2);
    painter.strokePath(path, borderPen);
}

bool TtsControlBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        auto* resizeEvent = static_cast<QResizeEvent*>(event);
        reposition(resizeEvent->size());
    }
    return QWidget::eventFilter(watched, event);
}

void TtsControlBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (parentWidget()) {
        reposition(parentWidget()->size());
    }
}

void TtsControlBar::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 10, 16, 10);
    mainLayout->setSpacing(6);

    // --- Top Info Row ---
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    m_counterLabel = new QLabel(this);
    m_counterLabel->setObjectName("ttsCounterLabel");
    QFont counterFont = m_counterLabel->font();
    counterFont.setBold(true);
    counterFont.setPointSize(10);
    m_counterLabel->setFont(counterFont);
    topRow->addWidget(m_counterLabel);

    m_karaokePreviewLabel = new QLabel(this);
    m_karaokePreviewLabel->setObjectName("ttsKaraokePreviewLabel");
    QFont previewFont = m_karaokePreviewLabel->font();
    previewFont.setItalic(true);
    previewFont.setPointSize(10);
    m_karaokePreviewLabel->setFont(previewFont);
    m_karaokePreviewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topRow->addWidget(m_karaokePreviewLabel, 1);

    m_closeBtn = new QPushButton(this);
    m_closeBtn->setObjectName("ttsCloseBtn");
    m_closeBtn->setFixedSize(26, 26);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("Dismiss TTS HUD"));
    connect(m_closeBtn, &QPushButton::clicked, this, &TtsControlBar::dismiss);
    topRow->addWidget(m_closeBtn);

    mainLayout->addLayout(topRow);

    // --- Middle Progress Bar / Slider ---
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setObjectName("ttsProgressSlider");
    m_progressSlider->setCursor(Qt::PointingHandCursor);
    m_progressSlider->setRange(0, 0);
    m_progressSlider->setValue(0);
    m_progressSlider->setFixedHeight(16);
    connect(m_progressSlider, &QSlider::sliderMoved, this, &TtsControlBar::onSliderValueChanged);
    connect(m_progressSlider, &QSlider::sliderPressed, this, [this]() {
        m_isSliderDragging = true;
    });
    connect(m_progressSlider, &QSlider::sliderReleased, this, &TtsControlBar::onProgressSliderReleased);
    mainLayout->addWidget(m_progressSlider);

    // --- Bottom Controls Row ---
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(8);

    // Left side: Rate and Sleep dropdowns
    m_rateBtn = new QPushButton("1.0x", this);
    m_rateBtn->setObjectName("ttsRateBtn");
    m_rateBtn->setCursor(Qt::PointingHandCursor);
    m_rateBtn->setFixedHeight(30);
    m_rateBtn->setToolTip(tr("Speech Rate"));
    buildSpeechRateMenu();
    bottomRow->addWidget(m_rateBtn);

    m_sleepBtn = new QPushButton(tr("Sleep: Off"), this);
    m_sleepBtn->setObjectName("ttsSleepBtn");
    m_sleepBtn->setCursor(Qt::PointingHandCursor);
    m_sleepBtn->setFixedHeight(30);
    m_sleepBtn->setToolTip(tr("Sleep Timer"));
    buildSleepTimerMenu();
    bottomRow->addWidget(m_sleepBtn);

    bottomRow->addStretch(1);

    // Center: Playback navigation buttons
    m_prevBtn = new QPushButton(this);
    m_prevBtn->setObjectName("ttsPrevBtn");
    m_prevBtn->setFixedSize(34, 34);
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setToolTip(tr("Previous Sentence"));
    connect(m_prevBtn, &QPushButton::clicked, this, &TtsControlBar::prevSentence);
    bottomRow->addWidget(m_prevBtn);

    m_playPauseBtn = new QPushButton(this);
    m_playPauseBtn->setObjectName("ttsPlayPauseBtn");
    m_playPauseBtn->setFixedSize(42, 42);
    m_playPauseBtn->setCursor(Qt::PointingHandCursor);
    m_playPauseBtn->setToolTip(tr("Play / Pause"));
    connect(m_playPauseBtn, &QPushButton::clicked, this, &TtsControlBar::togglePlayPause);
    bottomRow->addWidget(m_playPauseBtn);

    m_nextBtn = new QPushButton(this);
    m_nextBtn->setObjectName("ttsNextBtn");
    m_nextBtn->setFixedSize(34, 34);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setToolTip(tr("Next Sentence"));
    connect(m_nextBtn, &QPushButton::clicked, this, &TtsControlBar::nextSentence);
    bottomRow->addWidget(m_nextBtn);

    bottomRow->addStretch(1);

    // Right spacer placeholder balancing layout
    auto* spacerWidget = new QWidget(this);
    spacerWidget->setFixedWidth(m_rateBtn->sizeHint().width() + m_sleepBtn->sizeHint().width() + 8);
    spacerWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    bottomRow->addWidget(spacerWidget);

    mainLayout->addLayout(bottomRow);
}

void TtsControlBar::setupStyle() {
    const QString accentHex = m_tokens.accentColor.isValid()
        ? m_tokens.accentColor.name()
        : QString("#3B82F6");

    const QString textHex = m_tokens.isDark ? "#E5E7EB" : "#1F2937";
    const QString mutedHex = m_tokens.isDark ? "#9CA3AF" : "#6B7280";
    const QString hoverBg = m_tokens.isDark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.08)";
    const QString pressedBg = m_tokens.isDark ? "rgba(255, 255, 255, 0.20)" : "rgba(0, 0, 0, 0.16)";
    const QString pillBg = m_tokens.isDark ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.05)";
    const QString pillBorder = m_tokens.isDark ? "rgba(255, 255, 255, 0.15)" : "rgba(0, 0, 0, 0.12)";
    const QString menuBg = m_tokens.isDark ? "#1F2937" : "#FFFFFF";
    const QString menuBorder = m_tokens.isDark ? "#374151" : "#E5E7EB";

    setStyleSheet(QString(R"(
        QLabel#ttsCounterLabel {
            color: %1;
        }
        QLabel#ttsKaraokePreviewLabel {
            color: %2;
        }
        QPushButton#ttsCloseBtn {
            background-color: transparent;
            border: none;
            border-radius: 13px;
        }
        QPushButton#ttsCloseBtn:hover {
            background-color: rgba(239, 68, 68, 0.25);
        }
        QPushButton#ttsCloseBtn:pressed {
            background-color: rgba(239, 68, 68, 0.45);
        }
        QPushButton#ttsRateBtn, QPushButton#ttsSleepBtn {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 15px;
            padding: 4px 12px;
            color: %1;
            font-size: 11px;
            font-weight: 500;
        }
        QPushButton#ttsRateBtn:hover, QPushButton#ttsSleepBtn:hover {
            background-color: %5;
        }
        QPushButton#ttsRateBtn:pressed, QPushButton#ttsSleepBtn:pressed {
            background-color: %6;
        }
        QPushButton#ttsPrevBtn, QPushButton#ttsNextBtn {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 17px;
        }
        QPushButton#ttsPrevBtn:hover, QPushButton#ttsNextBtn:hover {
            background-color: %5;
        }
        QPushButton#ttsPrevBtn:pressed, QPushButton#ttsNextBtn:pressed {
            background-color: %6;
        }
        QPushButton#ttsPrevBtn:disabled, QPushButton#ttsNextBtn:disabled {
            opacity: 0.4;
        }
        QPushButton#ttsPlayPauseBtn {
            background-color: %7;
            border: none;
            border-radius: 21px;
        }
        QPushButton#ttsPlayPauseBtn:hover {
            background-color: %7;
            opacity: 0.9;
        }
        QPushButton#ttsPlayPauseBtn:pressed {
            background-color: %7;
            opacity: 0.75;
        }
        QSlider#ttsProgressSlider::groove:horizontal {
            height: 4px;
            background: %4;
            border-radius: 2px;
        }
        QSlider#ttsProgressSlider::sub-page:horizontal {
            background: %7;
            border-radius: 2px;
        }
        QSlider#ttsProgressSlider::handle:horizontal {
            background: #FFFFFF;
            width: 12px;
            height: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QSlider#ttsProgressSlider::handle:horizontal:hover {
            background: #F3F4F6;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QMenu {
            background-color: %8;
            border: 1px solid %9;
            border-radius: 8px;
            padding: 4px;
            color: %1;
        }
        QMenu::item {
            padding: 6px 18px 6px 24px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: %7;
            color: #FFFFFF;
        }
    )")
    .arg(textHex)
    .arg(mutedHex)
    .arg(pillBg)
    .arg(pillBorder)
    .arg(hoverBg)
    .arg(pressedBg)
    .arg(accentHex)
    .arg(menuBg)
    .arg(menuBorder));

    // Update icons with matching theme color
    const QColor iconColor = m_tokens.isDark ? QColor(229, 231, 235) : QColor(31, 41, 55);
    const QColor closeIconColor = m_tokens.isDark ? QColor(156, 163, 175) : QColor(107, 114, 128);

    m_closeBtn->setIcon(drawCloseIcon(closeIconColor));
    m_prevBtn->setIcon(drawPrevIcon(iconColor));
    m_nextBtn->setIcon(drawNextIcon(iconColor));
    m_sleepBtn->setIcon(drawMoonIcon(iconColor));

    updatePlayPauseUi(isPlaying());
}

void TtsControlBar::buildSpeechRateMenu() {
    m_rateMenu = new QMenu(this);
    m_rateActionGroup = new QActionGroup(this);
    m_rateActionGroup->setExclusive(true);

    const struct RateOption {
        QString label;
        double rate;
    } rates[] = {
        {"1.0x (Normal)", 1.0},
        {"1.25x", 1.25},
        {"1.5x", 1.5},
        {"2.0x", 2.0}
    };

    for (const auto& opt : rates) {
        auto* action = m_rateMenu->addAction(opt.label);
        action->setCheckable(true);
        action->setData(opt.rate);
        m_rateActionGroup->addAction(action);

        if (std::abs(opt.rate - 1.0) < 0.01) {
            action->setChecked(true);
        }

        connect(action, &QAction::triggered, this, [this, r = opt.rate]() {
            setSpeechRate(r);
        });
    }

    m_rateBtn->setMenu(m_rateMenu);
}

void TtsControlBar::buildSleepTimerMenu() {
    m_sleepMenu = new QMenu(this);
    m_sleepActionGroup = new QActionGroup(this);
    m_sleepActionGroup->setExclusive(true);

    const struct SleepMenuEntry {
        QString label;
        SleepTimerOption option;
    } entries[] = {
        {tr("Off"), SleepTimerOption::Off},
        {tr("5 min"), SleepTimerOption::Minutes5},
        {tr("10 min"), SleepTimerOption::Minutes10},
        {tr("15 min"), SleepTimerOption::Minutes15},
        {tr("30 min"), SleepTimerOption::Minutes30},
        {tr("End of Chapter"), SleepTimerOption::EndOfChapter}
    };

    for (const auto& entry : entries) {
        auto* action = m_sleepMenu->addAction(entry.label);
        action->setCheckable(true);
        action->setData(static_cast<int>(entry.option));
        m_sleepActionGroup->addAction(action);

        if (entry.option == SleepTimerOption::Off) {
            action->setChecked(true);
        }

        connect(action, &QAction::triggered, this, [this, opt = entry.option]() {
            setSleepTimer(opt);
        });
    }

    m_sleepBtn->setMenu(m_sleepMenu);
}

void TtsControlBar::updatePlayPauseUi(bool isPlaying) {
    if (!m_playPauseBtn) return;
    // Play/Pause icon is drawn crisp white on accent button
    const QColor iconColor(Qt::white);
    if (isPlaying) {
        m_playPauseBtn->setIcon(drawPauseIcon(iconColor));
        m_playPauseBtn->setToolTip(tr("Pause Playback (Space)"));
    } else {
        m_playPauseBtn->setIcon(drawPlayIcon(iconColor));
        m_playPauseBtn->setToolTip(tr("Start Playback (Space)"));
    }
}

void TtsControlBar::updateSentenceCounterUi(int current, int total) {
    if (!m_counterLabel) return;
    m_counterLabel->setText(tr("Sentence %1 / %2").arg(current).arg(total));
}

void TtsControlBar::updateSleepTimerUi() {
    if (!m_sleepBtn) return;

    if (m_sleepOption == SleepTimerOption::Off) {
        m_sleepBtn->setText(tr("Sleep: Off"));
    } else if (m_sleepOption == SleepTimerOption::EndOfChapter) {
        m_sleepBtn->setText(tr("Sleep: End of Ch."));
    } else {
        const int mins = m_sleepSecondsRemaining / 60;
        const int secs = m_sleepSecondsRemaining % 60;
        m_sleepBtn->setText(tr("Sleep: %1:%2")
                                .arg(mins, 2, 10, QChar('0'))
                                .arg(secs, 2, 10, QChar('0')));
    }

    // Keep checkmark in sync
    if (m_sleepMenu) {
        for (QAction* action : m_sleepMenu->actions()) {
            if (action->data().toInt() == static_cast<int>(m_sleepOption)) {
                action->setChecked(true);
                break;
            }
        }
    }
}

QIcon TtsControlBar::drawPlayIcon(const QColor& color) const {
    constexpr int s = 24;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    QPainterPath path;
    path.moveTo(8.0, 5.0);
    path.lineTo(19.0, 12.0);
    path.lineTo(8.0, 19.0);
    path.closeSubpath();
    p.drawPath(path);

    return QIcon(pix);
}

QIcon TtsControlBar::drawPauseIcon(const QColor& color) const {
    constexpr int s = 24;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    p.drawRoundedRect(QRectF(6.5, 5.5, 3.5, 13.0), 1.5, 1.5);
    p.drawRoundedRect(QRectF(14.0, 5.5, 3.5, 13.0), 1.5, 1.5);

    return QIcon(pix);
}

QIcon TtsControlBar::drawPrevIcon(const QColor& color) const {
    constexpr int s = 20;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    // Left bar
    p.drawRoundedRect(QRectF(3.5, 4.0, 2.0, 12.0), 1.0, 1.0);

    // Left arrow triangle
    QPainterPath triangle;
    triangle.moveTo(16.0, 4.5);
    triangle.lineTo(6.5, 10.0);
    triangle.lineTo(16.0, 15.5);
    triangle.closeSubpath();
    p.drawPath(triangle);

    return QIcon(pix);
}

QIcon TtsControlBar::drawNextIcon(const QColor& color) const {
    constexpr int s = 20;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    // Right arrow triangle
    QPainterPath triangle;
    triangle.moveTo(4.0, 4.5);
    triangle.lineTo(13.5, 10.0);
    triangle.lineTo(4.0, 15.5);
    triangle.closeSubpath();
    p.drawPath(triangle);

    // Right bar
    p.drawRoundedRect(QRectF(14.5, 4.0, 2.0, 12.0), 1.0, 1.0);

    return QIcon(pix);
}

QIcon TtsControlBar::drawCloseIcon(const QColor& color) const {
    constexpr int s = 18;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));

    p.drawLine(QPointF(4.5, 4.5), QPointF(13.5, 13.5));
    p.drawLine(QPointF(13.5, 4.5), QPointF(4.5, 13.5));

    return QIcon(pix);
}

QIcon TtsControlBar::drawSpeedIcon(const QColor& color) const {
    constexpr int s = 18;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);

    p.drawArc(QRectF(3.0, 3.0, 12.0, 12.0), 45 * 16, 270 * 16);
    p.drawLine(QPointF(9.0, 9.0), QPointF(13.0, 6.0));

    return QIcon(pix);
}

QIcon TtsControlBar::drawMoonIcon(const QColor& color) const {
    constexpr int s = 18;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    QPainterPath path;
    path.moveTo(13.5, 3.0);
    path.cubicTo(7.5, 3.5, 3.5, 8.5, 4.0, 14.0);
    path.cubicTo(4.2, 14.8, 4.5, 15.5, 4.9, 16.0);
    path.cubicTo(4.3, 14.5, 4.5, 11.5, 6.5, 9.0);
    path.cubicTo(8.5, 6.5, 11.5, 5.0, 14.5, 5.5);
    path.cubicTo(14.2, 4.5, 13.8, 3.6, 13.5, 3.0);
    path.closeSubpath();
    p.drawPath(path);

    return QIcon(pix);
}

} // namespace LunaRead
