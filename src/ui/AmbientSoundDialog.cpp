#include "AmbientSoundDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>

namespace LunaRead {

AmbientSoundDialog::AmbientSoundDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Ambient Reading Sounds & White Noise"));
    resize(520, 520);
    setupUi();

    connect(&AmbientSoundService::instance(), &AmbientSoundService::playbackStateChanged,
            this, &AmbientSoundDialog::onPlaybackStateChanged);
    connect(&AmbientSoundService::instance(), &AmbientSoundService::sleepTimerTick,
            this, &AmbientSoundDialog::onSleepTimerTick);
}

void AmbientSoundDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    auto* header = new QLabel(
        tr("<b>Relaxing Background Soundscapes</b><br>"
           "<span style='color: #64748B;'>Procedurally generated soothing white noise to enhance reading focus.</span>"), this);
    mainLayout->addWidget(header);

    // Sound Selection Grid
    auto* gridGroup = new QGroupBox(tr("Select Soundscape"), this);
    auto* gridLayout = new QGridLayout(gridGroup);
    gridLayout->setSpacing(10);

    m_soundGroup = new QButtonGroup(this);

    struct SoundCardInfo {
        SoundType type;
        QString icon;
        QString name;
    };

    const std::vector<SoundCardInfo> sounds = {
        {SoundType::Rainfall, QStringLiteral("🌧️"), tr("Gentle Rain")},
        {SoundType::ForestStream, QStringLiteral("🍃"), tr("Forest Stream")},
        {SoundType::CafeAmbience, QStringLiteral("☕"), tr("Cafe Ambience")},
        {SoundType::PinkNoise, QStringLiteral("📻"), tr("Pink Noise")},
        {SoundType::WhiteNoise, QStringLiteral("⚪"), tr("White Noise")},
        {SoundType::OceanWaves, QStringLiteral("🌊"), tr("Ocean Waves")}
    };

    SoundType activeSound = AmbientSoundService::instance().currentSound();

    for (size_t i = 0; i < sounds.size(); ++i) {
        int r = static_cast<int>(i / 2);
        int c = static_cast<int>(i % 2);

        auto* btn = new QPushButton(QStringLiteral("%1  %2").arg(sounds[i].icon, sounds[i].name), gridGroup);
        btn->setCheckable(true);
        btn->setFixedHeight(44);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #F8FAFC;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 8px;"
            "  font-size: 10.5pt;"
            "  font-weight: 500;"
            "  color: #1E293B;"
            "}"
            "QPushButton:hover {"
            "  background-color: #F1F5F9;"
            "  border-color: #94A3B8;"
            "}"
            "QPushButton:checked {"
            "  background-color: #EFF6FF;"
            "  border: 2px solid #2563EB;"
            "  color: #1D4ED8;"
            "  font-weight: bold;"
            "}"
        );

        if (sounds[i].type == activeSound) {
            btn->setChecked(true);
        }

        m_soundGroup->addButton(btn, static_cast<int>(sounds[i].type));
        gridLayout->addWidget(btn, r, c);
    }

    connect(m_soundGroup, &QButtonGroup::idClicked, this, &AmbientSoundDialog::onSoundSelected);
    mainLayout->addWidget(gridGroup);

    // Playback and Volume Box
    auto* playBox = new QGroupBox(tr("Playback Controls"), this);
    auto* playLayout = new QVBoxLayout(playBox);
    playLayout->setSpacing(12);

    auto* topPlayRow = new QHBoxLayout();
    m_playPauseBtn = new QPushButton(this);
    m_playPauseBtn->setFixedHeight(38);
    m_playPauseBtn->setCursor(Qt::PointingHandCursor);
    updatePlayButtonState();
    connect(m_playPauseBtn, &QPushButton::clicked, this, &AmbientSoundDialog::onPlayPauseClicked);
    topPlayRow->addWidget(m_playPauseBtn, 1);
    playLayout->addLayout(topPlayRow);

    // Volume Slider
    auto* volRow = new QHBoxLayout();
    volRow->addWidget(new QLabel(tr("Volume:"), playBox));

    m_volumeSlider = new QSlider(Qt::Horizontal, playBox);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(AmbientSoundService::instance().volume());
    connect(m_volumeSlider, &QSlider::valueChanged, this, &AmbientSoundDialog::onVolumeChanged);
    volRow->addWidget(m_volumeSlider, 1);

    m_volumeValueLabel = new QLabel(QStringLiteral("%1%").arg(m_volumeSlider->value()), playBox);
    m_volumeValueLabel->setFixedWidth(36);
    volRow->addWidget(m_volumeValueLabel);
    playLayout->addLayout(volRow);

    mainLayout->addWidget(playBox);

    // Options (Page Turn Sound & Sleep Timer)
    auto* optGroup = new QGroupBox(tr("Sound Options"), this);
    auto* optLayout = new QVBoxLayout(optGroup);
    optLayout->setSpacing(8);

    m_pageFlipCheck = new QCheckBox(tr("Play crisp paper rustle sound effect on page turn"), optGroup);
    m_pageFlipCheck->setChecked(AmbientSoundService::instance().isPageFlipSoundEnabled());
    connect(m_pageFlipCheck, &QCheckBox::toggled, this, &AmbientSoundDialog::onPageFlipToggled);
    optLayout->addWidget(m_pageFlipCheck);

    auto* timerRow = new QHBoxLayout();
    timerRow->addWidget(new QLabel(tr("Sleep Timer:"), optGroup));
    m_sleepTimerCombo = new QComboBox(optGroup);
    m_sleepTimerCombo->addItem(tr("Continuous (No Timer)"), 0);
    m_sleepTimerCombo->addItem(tr("15 Minutes"), 15);
    m_sleepTimerCombo->addItem(tr("30 Minutes"), 30);
    m_sleepTimerCombo->addItem(tr("45 Minutes"), 45);
    m_sleepTimerCombo->addItem(tr("60 Minutes"), 60);

    int curMins = AmbientSoundService::instance().sleepTimerMinutes();
    int timerIdx = m_sleepTimerCombo->findData(curMins);
    if (timerIdx >= 0) m_sleepTimerCombo->setCurrentIndex(timerIdx);

    connect(m_sleepTimerCombo, &QComboBox::currentIndexChanged, this, &AmbientSoundDialog::onSleepTimerComboChanged);
    timerRow->addWidget(m_sleepTimerCombo, 1);

    m_timerStatusLabel = new QLabel(optGroup);
    m_timerStatusLabel->setStyleSheet("color: #64748B; font-size: 9pt;");
    timerRow->addWidget(m_timerStatusLabel);
    optLayout->addLayout(timerRow);

    mainLayout->addWidget(optGroup);

    // Bottom close button
    auto* closeBtn = new QPushButton(tr("Done"), this);
    closeBtn->setFixedHeight(34);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);
}

void AmbientSoundDialog::updatePlayButtonState() {
    bool playing = AmbientSoundService::instance().isPlaying();
    if (playing) {
        m_playPauseBtn->setText(QString::fromUtf8("⏸ Pause Ambient Sound"));
        m_playPauseBtn->setStyleSheet("background-color: #FEF2F2; color: #DC2626; border: 1px solid #FECACA; border-radius: 6px; font-weight: bold;");
    } else {
        m_playPauseBtn->setText(QString::fromUtf8("▶ Play Ambient Sound"));
        m_playPauseBtn->setStyleSheet("background-color: #2563EB; color: white; border: 1px solid #1D4ED8; border-radius: 6px; font-weight: bold;");
    }
}

void AmbientSoundDialog::onPlayPauseClicked() {
    AmbientSoundService::instance().togglePlayPause();
}

void AmbientSoundDialog::onSoundSelected(int id) {
    auto type = static_cast<SoundType>(id);
    AmbientSoundService::instance().play(type, m_volumeSlider->value());
}

void AmbientSoundDialog::onVolumeChanged(int val) {
    m_volumeValueLabel->setText(QStringLiteral("%1%").arg(val));
    AmbientSoundService::instance().setVolume(val);
}

void AmbientSoundDialog::onPageFlipToggled(bool checked) {
    AmbientSoundService::instance().setPageFlipSoundEnabled(checked);
}

void AmbientSoundDialog::onSleepTimerComboChanged(int index) {
    int mins = m_sleepTimerCombo->itemData(index).toInt();
    AmbientSoundService::instance().setSleepTimer(mins);
    if (mins == 0) {
        m_timerStatusLabel->clear();
    }
}

void AmbientSoundDialog::onPlaybackStateChanged(bool playing) {
    Q_UNUSED(playing);
    updatePlayButtonState();
}

void AmbientSoundDialog::onSleepTimerTick(int remainingSeconds) {
    if (remainingSeconds > 0) {
        int m = remainingSeconds / 60;
        int s = remainingSeconds % 60;
        m_timerStatusLabel->setText(QStringLiteral("⏱️ %1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0')));
    } else {
        m_timerStatusLabel->clear();
    }
}

} // namespace LunaRead
