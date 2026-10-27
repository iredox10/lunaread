#include "SpeedReadingDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <QKeyEvent>
#include <QRegularExpression>
#include <algorithm>

namespace LunaRead {

SpeedReadingDialog::SpeedReadingDialog(const QString& text, int initialWpm, QWidget* parent)
    : QDialog(parent), m_wpm(initialWpm), m_tokens(ReaderThemeTokens::parchment()) {
    setWindowTitle(tr("⚡ Rapid Serial Visual Presentation (Speed Reader)"));
    resize(760, 480);
    setModal(true);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &SpeedReadingDialog::onTick);

    setupUi();
    setText(text);
    applyTheme(m_tokens);
}

void SpeedReadingDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(18);

    // 1. Top HUD / Info Bar
    auto* topLayout = new QHBoxLayout();
    m_progressLabel = new QLabel(tr("Word 0 of 0 (0%)"), this);
    m_progressLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    topLayout->addWidget(m_progressLabel);

    topLayout->addStretch();

    m_etaLabel = new QLabel(tr("⏱️ ~0 min remaining"), this);
    m_etaLabel->setStyleSheet("font-size: 13px; color: #78909c;");
    topLayout->addWidget(m_etaLabel);

    mainLayout->addLayout(topLayout);

    // 2. RSVP Focus Box (Framed view with crosshair guide markers)
    auto* focusFrame = new QFrame(this);
    focusFrame->setObjectName("rsvpFrame");
    focusFrame->setMinimumHeight(160);
    focusFrame->setFrameShape(QFrame::StyledPanel);

    auto* frameLayout = new QVBoxLayout(focusFrame);
    frameLayout->setContentsMargins(20, 20, 20, 20);

    // Guide marker line top
    auto* topGuide = new QFrame(focusFrame);
    topGuide->setFixedHeight(2);
    topGuide->setStyleSheet("background-color: rgba(255, 82, 82, 0.4);");
    frameLayout->addWidget(topGuide);

    frameLayout->addStretch();

    // Central RSVP display
    m_orpDisplay = new QLabel(tr("Press Play to Start"), focusFrame);
    m_orpDisplay->setAlignment(Qt::AlignCenter);
    m_orpDisplay->setTextFormat(Qt::RichText);
    m_orpDisplay->setFont(QFont("Liberation Mono", 32, QFont::Bold));
    frameLayout->addWidget(m_orpDisplay);

    frameLayout->addStretch();

    // Guide marker line bottom
    auto* btmGuide = new QFrame(focusFrame);
    btmGuide->setFixedHeight(2);
    btmGuide->setStyleSheet("background-color: rgba(255, 82, 82, 0.4);");
    frameLayout->addWidget(btmGuide);

    mainLayout->addWidget(focusFrame);

    // 3. Surrounding Context Snippet
    m_contextPreview = new QLabel(tr("Ready."), this);
    m_contextPreview->setAlignment(Qt::AlignCenter);
    m_contextPreview->setWordWrap(true);
    m_contextPreview->setStyleSheet("font-size: 12px; font-style: italic; color: #90a4ae; min-height: 38px;");
    mainLayout->addWidget(m_contextPreview);

    // 4. Scrubbing Progress Slider
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 0);
    connect(m_progressSlider, &QSlider::sliderMoved, this, &SpeedReadingDialog::onSliderMoved);
    mainLayout->addWidget(m_progressSlider);

    // 5. Playback Controls Bar
    auto* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(12);

    m_restartBtn = new QPushButton(tr("🔄 Restart"), this);
    m_restartBtn->setCursor(Qt::PointingHandCursor);
    connect(m_restartBtn, &QPushButton::clicked, this, &SpeedReadingDialog::restart);
    controlsLayout->addWidget(m_restartBtn);

    m_stepBackBtn = new QPushButton(tr("⏮ 10 Words"), this);
    m_stepBackBtn->setCursor(Qt::PointingHandCursor);
    connect(m_stepBackBtn, &QPushButton::clicked, this, [this]() { stepBack(10); });
    controlsLayout->addWidget(m_stepBackBtn);

    m_playPauseBtn = new QPushButton(tr("▶ Play (Space)"), this);
    m_playPauseBtn->setCursor(Qt::PointingHandCursor);
    m_playPauseBtn->setMinimumWidth(130);
    m_playPauseBtn->setStyleSheet("font-weight: bold; padding: 8px 16px; font-size: 14px;");
    connect(m_playPauseBtn, &QPushButton::clicked, this, &SpeedReadingDialog::togglePlayPause);
    controlsLayout->addWidget(m_playPauseBtn);

    m_stepForwardBtn = new QPushButton(tr("10 Words ⏭"), this);
    m_stepForwardBtn->setCursor(Qt::PointingHandCursor);
    connect(m_stepForwardBtn, &QPushButton::clicked, this, [this]() { stepForward(10); });
    controlsLayout->addWidget(m_stepForwardBtn);

    controlsLayout->addStretch();

    // Chunk size selector
    auto* chunkLabel = new QLabel(tr("Chunk:"), this);
    m_chunkCombo = new QComboBox(this);
    m_chunkCombo->addItem(tr("1 Word"), 1);
    m_chunkCombo->addItem(tr("2 Words"), 2);
    m_chunkCombo->addItem(tr("3 Words"), 3);
    connect(m_chunkCombo, &QComboBox::currentIndexChanged, this, &SpeedReadingDialog::onChunkSizeChanged);
    controlsLayout->addWidget(chunkLabel);
    controlsLayout->addWidget(m_chunkCombo);

    mainLayout->addLayout(controlsLayout);

    // 6. WPM Speed Adjustment Slider
    auto* wpmLayout = new QHBoxLayout();
    m_wpmLabel = new QLabel(QString("Speed: %1 WPM").arg(m_wpm), this);
    m_wpmLabel->setMinimumWidth(110);
    m_wpmLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    wpmLayout->addWidget(m_wpmLabel);

    m_wpmSlider = new QSlider(Qt::Horizontal, this);
    m_wpmSlider->setRange(100, 1000);
    m_wpmSlider->setSingleStep(25);
    m_wpmSlider->setPageStep(50);
    m_wpmSlider->setValue(m_wpm);
    connect(m_wpmSlider, &QSlider::valueChanged, this, &SpeedReadingDialog::onWpmChanged);
    wpmLayout->addWidget(m_wpmSlider);

    // Quick Presets
    auto* btn250 = new QPushButton("250", this);
    btn250->setMaximumWidth(45);
    connect(btn250, &QPushButton::clicked, this, [this]() { setWpm(250); });
    wpmLayout->addWidget(btn250);

    auto* btn400 = new QPushButton("400", this);
    btn400->setMaximumWidth(45);
    connect(btn400, &QPushButton::clicked, this, [this]() { setWpm(400); });
    wpmLayout->addWidget(btn400);

    auto* btn600 = new QPushButton("600", this);
    btn600->setMaximumWidth(45);
    connect(btn600, &QPushButton::clicked, this, [this]() { setWpm(600); });
    wpmLayout->addWidget(btn600);

    mainLayout->addLayout(wpmLayout);
}

void SpeedReadingDialog::setText(const QString& text) {
    m_words.clear();
    const QStringList tokens = text.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    for (const auto& w : tokens) {
        if (!w.trimmed().isEmpty()) {
            m_words.append(w.trimmed());
        }
    }

    m_currentIndex = 0;
    m_progressSlider->setRange(0, std::max(0, static_cast<int>(m_words.size()) - 1));
    m_progressSlider->setValue(0);
    updateWordDisplay();
    updateProgressDisplay();
}

void SpeedReadingDialog::setWpm(int wpm) {
    m_wpm = std::clamp(wpm, 100, 1200);
    if (m_wpmSlider && m_wpmSlider->value() != m_wpm) {
        m_wpmSlider->setValue(m_wpm);
    }
    if (m_wpmLabel) {
        m_wpmLabel->setText(QString("Speed: %1 WPM").arg(m_wpm));
    }
}

void SpeedReadingDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    setStyleSheet(QString(
        "QDialog { background-color: %1; color: %2; }"
        "QFrame#rsvpFrame { background-color: %3; border: 1px solid %4; border-radius: 8px; }"
        "QPushButton { background-color: %5; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: %6; }"
        "QComboBox { background-color: %5; color: %2; border: 1px solid %4; border-radius: 4px; padding: 4px 8px; }"
        "QSlider::groove:horizontal { height: 6px; background: %4; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: %7; width: 14px; margin: -4px 0; border-radius: 7px; }"
    ).arg(tokens.backgroundColor.name(),
         tokens.textColor.name(),
         tokens.isDark ? "#1a1a1a" : "#ffffff",
         tokens.isDark ? "#333333" : "#dddddd",
         tokens.isDark ? "#2a2a2a" : "#f0f0f0",
         tokens.isDark ? "#383838" : "#e4e4e4",
         tokens.accentColor.name()));

    updateWordDisplay();
}

void SpeedReadingDialog::play() {
    if (m_words.isEmpty()) return;
    m_isPlaying = true;
    m_playPauseBtn->setText(tr("⏸ Pause (Space)"));
    m_timer->start(calculateNextDelayMs());
}

void SpeedReadingDialog::pause() {
    m_isPlaying = false;
    m_timer->stop();
    m_playPauseBtn->setText(tr("▶ Play (Space)"));
}

void SpeedReadingDialog::togglePlayPause() {
    if (m_isPlaying) {
        pause();
    } else {
        if (m_currentIndex >= m_words.size() - 1) {
            m_currentIndex = 0;
        }
        play();
    }
}

void SpeedReadingDialog::stepBack(int words) {
    m_currentIndex = std::max(0, m_currentIndex - words);
    m_progressSlider->setValue(m_currentIndex);
    updateWordDisplay();
    updateProgressDisplay();
}

void SpeedReadingDialog::stepForward(int words) {
    m_currentIndex = std::min(static_cast<int>(m_words.size()) - 1, m_currentIndex + words);
    m_progressSlider->setValue(m_currentIndex);
    updateWordDisplay();
    updateProgressDisplay();
}

void SpeedReadingDialog::restart() {
    pause();
    m_currentIndex = 0;
    m_progressSlider->setValue(0);
    updateWordDisplay();
    updateProgressDisplay();
}

void SpeedReadingDialog::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Space:
            togglePlayPause();
            event->accept();
            break;
        case Qt::Key_Left:
            stepBack(10);
            event->accept();
            break;
        case Qt::Key_Right:
            stepForward(10);
            event->accept();
            break;
        case Qt::Key_Up:
            setWpm(m_wpm + 25);
            event->accept();
            break;
        case Qt::Key_Down:
            setWpm(m_wpm - 25);
            event->accept();
            break;
        case Qt::Key_R:
            restart();
            event->accept();
            break;
        case Qt::Key_Escape:
            pause();
            QDialog::keyPressEvent(event);
            break;
        default:
            QDialog::keyPressEvent(event);
            break;
    }
}

void SpeedReadingDialog::onTick() {
    if (!m_isPlaying) return;

    m_currentIndex += m_chunkSize;
    if (m_currentIndex >= m_words.size()) {
        m_currentIndex = std::max(0, static_cast<int>(m_words.size()) - 1);
        pause();
        updateWordDisplay();
        updateProgressDisplay();
        return;
    }

    m_progressSlider->setValue(m_currentIndex);
    updateWordDisplay();
    updateProgressDisplay();

    m_timer->start(calculateNextDelayMs());
}

void SpeedReadingDialog::onSliderMoved(int position) {
    m_currentIndex = std::clamp(position, 0, std::max(0, static_cast<int>(m_words.size()) - 1));
    updateWordDisplay();
    updateProgressDisplay();
}

void SpeedReadingDialog::onWpmChanged(int wpm) {
    setWpm(wpm);
}

void SpeedReadingDialog::onChunkSizeChanged(int index) {
    m_chunkSize = m_chunkCombo->itemData(index).toInt();
    if (m_chunkSize < 1) m_chunkSize = 1;
}

int SpeedReadingDialog::calculateNextDelayMs() const {
    if (m_words.isEmpty() || m_currentIndex >= m_words.size()) {
        return 200;
    }

    // Base delay per word chunk
    double baseMs = (60000.0 / m_wpm) * m_chunkSize;

    // Punctuation multiplier
    const QString& currentWord = m_words[m_currentIndex];
    if (currentWord.endsWith('.') || currentWord.endsWith('!') || currentWord.endsWith('?')) {
        baseMs *= 2.0;
    } else if (currentWord.endsWith(',') || currentWord.endsWith(';') || currentWord.endsWith(':') || currentWord.endsWith('-')) {
        baseMs *= 1.5;
    } else if (currentWord.length() > 10) {
        baseMs *= 1.25;
    }

    return std::max(30, static_cast<int>(baseMs));
}

QString SpeedReadingDialog::formatOrpWord(const QString& word, const QColor& accentColor, const QColor& textColor) {
    if (word.isEmpty()) return QString();

    int len = word.length();
    int orpIndex = 0;
    if (len >= 2 && len <= 5) orpIndex = 1;
    else if (len >= 6 && len <= 9) orpIndex = 2;
    else if (len >= 10 && len <= 13) orpIndex = 3;
    else if (len > 13) orpIndex = 4;

    QString prefix = word.left(orpIndex).toHtmlEscaped();
    QChar orpChar = word.at(orpIndex);
    QString suffix = word.mid(orpIndex + 1).toHtmlEscaped();

    return QString("<span style='color: %1;'>%2</span>"
                   "<span style='color: #FF5252; text-decoration: underline; font-weight: 900;'>%3</span>"
                   "<span style='color: %1;'>%4</span>")
        .arg(textColor.name(), prefix, QString(orpChar).toHtmlEscaped(), suffix);
}

void SpeedReadingDialog::updateWordDisplay() {
    if (m_words.isEmpty()) {
        m_orpDisplay->setText(tr("No text to display"));
        m_contextPreview->setText(QString());
        return;
    }

    if (m_currentIndex < 0 || m_currentIndex >= m_words.size()) {
        m_currentIndex = 0;
    }

    QString chunkFormatted;
    for (int i = 0; i < m_chunkSize && (m_currentIndex + i) < m_words.size(); ++i) {
        if (i > 0) chunkFormatted.append(" ");
        chunkFormatted.append(formatOrpWord(m_words[m_currentIndex + i], m_tokens.accentColor, m_tokens.textColor));
    }

    m_orpDisplay->setText(chunkFormatted);

    // Build context snippet (-5 words ... +5 words)
    int start = std::max(0, m_currentIndex - 5);
    int end = std::min(static_cast<int>(m_words.size()), m_currentIndex + 6);
    QString context;
    for (int i = start; i < end; ++i) {
        if (i == m_currentIndex) {
            context.append(QString(" <b><u>%1</u></b> ").arg(m_words[i].toHtmlEscaped()));
        } else {
            context.append(m_words[i].toHtmlEscaped()).append(" ");
        }
    }
    m_contextPreview->setText(QString("... %1 ...").arg(context.trimmed()));
}

void SpeedReadingDialog::updateProgressDisplay() {
    int total = static_cast<int>(m_words.size());
    int current = m_currentIndex + 1;
    double pct = (total > 0) ? (static_cast<double>(current) / total) * 100.0 : 0.0;

    m_progressLabel->setText(tr("Word %1 of %2 (%3%)").arg(current).arg(total).arg(pct, 0, 'f', 1));

    int wordsLeft = std::max(0, total - current);
    double minutesLeft = (m_wpm > 0) ? (static_cast<double>(wordsLeft) / m_wpm) : 0.0;
    m_etaLabel->setText(tr("⏱️ ~%1 min remaining").arg(minutesLeft, 0, 'f', 1));

    emit wordIndexChanged(m_currentIndex, total);
}

} // namespace LunaRead
