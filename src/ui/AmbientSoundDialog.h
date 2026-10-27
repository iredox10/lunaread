#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QButtonGroup>
#include "../features/AmbientSoundService.h"

namespace LunaRead {

/**
 * @brief Modal dialog to select ambient background noise, adjust volume, and configure page-turn sound effects.
 */
class AmbientSoundDialog : public QDialog {
    Q_OBJECT

public:
    explicit AmbientSoundDialog(QWidget* parent = nullptr);
    ~AmbientSoundDialog() override = default;

private slots:
    void onPlayPauseClicked();
    void onSoundSelected(int id);
    void onVolumeChanged(int val);
    void onPageFlipToggled(bool checked);
    void onSleepTimerComboChanged(int index);
    void onPlaybackStateChanged(bool playing);
    void onSleepTimerTick(int remainingSeconds);

private:
    void setupUi();
    void updatePlayButtonState();

    QButtonGroup* m_soundGroup = nullptr;
    QPushButton* m_playPauseBtn = nullptr;
    QSlider* m_volumeSlider = nullptr;
    QLabel* m_volumeValueLabel = nullptr;
    QCheckBox* m_pageFlipCheck = nullptr;
    QComboBox* m_sleepTimerCombo = nullptr;
    QLabel* m_timerStatusLabel = nullptr;
};

} // namespace LunaRead
