#pragma once

#include <QDialog>
#include <QWidget>
#include <QString>
#include <QList>
#include <QTime>
#include "../features/ReadingProfileManager.h"
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QSlider;
class QLabel;
class QPushButton;
class QCheckBox;
class QTimeEdit;
class QButtonGroup;
class QFrame;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Live typography and eye-care preview card for a reading profile.
 */
class ProfilePreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProfilePreviewWidget(QWidget* parent = nullptr);
    ~ProfilePreviewWidget() override = default;

    void setProfile(const ReadingProfile& profile);
    [[nodiscard]] const ReadingProfile& profile() const noexcept { return m_profile; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ReadingProfile m_profile;
};

/**
 * @brief Modal management dialog for reading profiles and automated Day/Night scheduling.
 *
 * Implements Moon+ Reader's iconic dual profile system:
 * - Quick Day Mode vs Night Mode toggle buttons
 * - Profile selector dropdown and "Save As New Profile" flow
 * - Sliders for software brightness dimming, blue light reduction filter, page margins, and font size
 * - Checkbox for automatic sunset/sunrise time-of-day profile switching
 * - Live real-time preview card rendering active theme, typography, and filters
 */
class ReadingProfileDialog : public QDialog {
    Q_OBJECT

public:
    explicit ReadingProfileDialog(QWidget* parent = nullptr);
    ~ReadingProfileDialog() override = default;

    [[nodiscard]] ReadingProfile currentProfile() const { return m_currentProfile; }
    void setProfile(const ReadingProfile& profile);

signals:
    void profileApplied(const ReadingProfile& profile);

public slots:
    void applyCurrentProfile();

private slots:
    void onProfileComboChanged(int index);
    void onQuickDayClicked();
    void onQuickNightClicked();
    void onSaveAsNewClicked();
    void onSaveProfileClicked();
    void onDeleteProfileClicked();
    void onAutoScheduleToggled(bool checked);
    void onDayTimeChanged(const QTime& time);
    void onNightTimeChanged(const QTime& time);
    void onSliderValueChanged();
    void onThemeModeSelected(ReaderThemeMode mode);
    void onFontFamilyChanged(int index);
    void onDecreaseFontSizeClicked();
    void onIncreaseFontSizeClicked();
    void onManagerActiveProfileChanged(const ReadingProfile& profile);
    void onManagerProfilesChanged();

private:
    void setupUi();
    void populateFontFamilies();
    void refreshProfileList();
    void updateUiFromProfile(const ReadingProfile& profile);
    void updateProfileFromUi();
    void updateQuickButtonsState();

    ReadingProfile m_currentProfile;
    bool m_updatingUi = false;

    // Quick Day vs Night header buttons
    QPushButton* m_quickDayBtn = nullptr;
    QPushButton* m_quickNightBtn = nullptr;

    // Profile selector bar
    QComboBox* m_profileCombo = nullptr;
    QPushButton* m_saveAsNewBtn = nullptr;
    QPushButton* m_deleteProfileBtn = nullptr;

    // Live preview
    ProfilePreviewWidget* m_previewWidget = nullptr;

    // Typography & Theme controls
    QButtonGroup* m_themeButtonGroup = nullptr;
    QList<QPushButton*> m_themeSwatchButtons;
    QComboBox* m_fontFamilyCombo = nullptr;

    QSlider* m_fontSizeSlider = nullptr;
    QLabel* m_fontSizeValueLabel = nullptr;
    QPushButton* m_fontSizeDecreaseBtn = nullptr;
    QPushButton* m_fontSizeIncreaseBtn = nullptr;

    QSlider* m_lineHeightSlider = nullptr;
    QLabel* m_lineHeightValueLabel = nullptr;

    QSlider* m_marginHSlider = nullptr;
    QLabel* m_marginHValueLabel = nullptr;

    QSlider* m_marginVSlider = nullptr;
    QLabel* m_marginVValueLabel = nullptr;

    // Eye care filter controls
    QSlider* m_brightnessDimSlider = nullptr;
    QLabel* m_brightnessDimValueLabel = nullptr;

    QSlider* m_blueLightSlider = nullptr;
    QLabel* m_blueLightValueLabel = nullptr;

    // Auto schedule controls
    QCheckBox* m_autoScheduleCheck = nullptr;
    QFrame* m_scheduleSettingsCard = nullptr;
    QTimeEdit* m_dayTimeEdit = nullptr;
    QTimeEdit* m_nightTimeEdit = nullptr;

    // Bottom action bar
    QPushButton* m_saveProfileBtn = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
};

} // namespace LunaRead
