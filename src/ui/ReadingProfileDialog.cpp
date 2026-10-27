#include "ReadingProfileDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTimeEdit>
#include <QButtonGroup>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QFontDatabase>
#include <algorithm>
#include <cmath>

namespace LunaRead {

// ============================================================================
// ProfilePreviewWidget
// ============================================================================

ProfilePreviewWidget::ProfilePreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setFixedHeight(110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ProfilePreviewWidget::setProfile(const ReadingProfile& profile) {
    m_profile = profile;
    update();
}

void ProfilePreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    ReaderThemeTokens tokens = ReaderThemeTokens::fromMode(m_profile.themeMode);

    // Background card
    p.setPen(QPen(QColor(0, 0, 0, 40), 1));
    p.setBrush(tokens.backgroundColor);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    // Text rendering
    QFont font(m_profile.fontFamily.isEmpty() ? QStringLiteral("Roboto") : m_profile.fontFamily,
              std::clamp(m_profile.fontSize, 10, 24));
    p.setFont(font);
    p.setPen(tokens.textColor);

    QRect textRect = rect().adjusted(16, 12, -16, -12);
    QString sampleText = tr("The quick brown fox jumps over the lazy dog.\n"
                            "LunaRead brings true desktop elegance to long-form reading.");
    p.drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, sampleText);

    // Blue light & Dimming simulation
    if (m_profile.blueLightFilterPercent > 0) {
        int alpha = static_cast<int>(m_profile.blueLightFilterPercent * 1.6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 160, 40, std::clamp(alpha, 0, 200)));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
    }
    if (m_profile.brightnessDimPercent > 0) {
        int alpha = static_cast<int>(m_profile.brightnessDimPercent * 2.2);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, std::clamp(alpha, 0, 220)));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
    }
}

// ============================================================================
// ReadingProfileDialog
// ============================================================================

ReadingProfileDialog::ReadingProfileDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Dual Reading Profiles & Night Care"));
    resize(640, 680);

    m_currentProfile = ReadingProfileManager::instance().activeProfile();
    setupUi();
    updateUiFromProfile(m_currentProfile);

    connect(&ReadingProfileManager::instance(), &ReadingProfileManager::activeProfileChanged,
            this, &ReadingProfileDialog::onManagerActiveProfileChanged);
    connect(&ReadingProfileManager::instance(), &ReadingProfileManager::profilesChanged,
            this, &ReadingProfileDialog::onManagerProfilesChanged);
}

void ReadingProfileDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(18, 16, 18, 16);

    // 1. Quick Day vs Night Mode Header Buttons
    auto* quickRow = new QHBoxLayout();
    quickRow->setSpacing(10);

    m_quickDayBtn = new QPushButton(tr("☀️ Switch to Day Mode"), this);
    m_quickDayBtn->setFixedHeight(36);
    m_quickDayBtn->setCursor(Qt::PointingHandCursor);
    connect(m_quickDayBtn, &QPushButton::clicked, this, &ReadingProfileDialog::onQuickDayClicked);
    quickRow->addWidget(m_quickDayBtn, 1);

    m_quickNightBtn = new QPushButton(tr("🌙 Switch to Night Mode"), this);
    m_quickNightBtn->setFixedHeight(36);
    m_quickNightBtn->setCursor(Qt::PointingHandCursor);
    connect(m_quickNightBtn, &QPushButton::clicked, this, &ReadingProfileDialog::onQuickNightClicked);
    quickRow->addWidget(m_quickNightBtn, 1);

    mainLayout->addLayout(quickRow);

    // 2. Profile Selector Row
    auto* profileRow = new QHBoxLayout();
    profileRow->setSpacing(8);

    profileRow->addWidget(new QLabel(tr("Active Profile:"), this));

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setFixedHeight(32);
    connect(m_profileCombo, &QComboBox::currentIndexChanged, this, &ReadingProfileDialog::onProfileComboChanged);
    profileRow->addWidget(m_profileCombo, 1);

    m_saveAsNewBtn = new QPushButton(tr("➕ Save As..."), this);
    m_saveAsNewBtn->setFixedHeight(32);
    m_saveAsNewBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveAsNewBtn, &QPushButton::clicked, this, &ReadingProfileDialog::onSaveAsNewClicked);
    profileRow->addWidget(m_saveAsNewBtn);

    m_deleteProfileBtn = new QPushButton(tr("🗑️ Delete"), this);
    m_deleteProfileBtn->setFixedHeight(32);
    m_deleteProfileBtn->setCursor(Qt::PointingHandCursor);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &ReadingProfileDialog::onDeleteProfileClicked);
    profileRow->addWidget(m_deleteProfileBtn);

    mainLayout->addLayout(profileRow);

    // 3. Live Preview Card
    m_previewWidget = new ProfilePreviewWidget(this);
    mainLayout->addWidget(m_previewWidget);

    // 4. Form Settings in Group Box
    auto* formGroup = new QGroupBox(tr("Profile Parameters"), this);
    auto* formLayout = new QFormLayout(formGroup);
    formLayout->setSpacing(8);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Font Family
    m_fontFamilyCombo = new QComboBox(formGroup);
    populateFontFamilies();
    connect(m_fontFamilyCombo, &QComboBox::currentIndexChanged, this, &ReadingProfileDialog::onFontFamilyChanged);
    formLayout->addRow(tr("Font:"), m_fontFamilyCombo);

    // Font Size
    auto* sizeRow = new QHBoxLayout();
    m_fontSizeDecreaseBtn = new QPushButton(QStringLiteral("A-"), formGroup);
    m_fontSizeDecreaseBtn->setFixedSize(30, 26);
    connect(m_fontSizeDecreaseBtn, &QPushButton::clicked, this, &ReadingProfileDialog::onDecreaseFontSizeClicked);
    sizeRow->addWidget(m_fontSizeDecreaseBtn);

    m_fontSizeSlider = new QSlider(Qt::Horizontal, formGroup);
    m_fontSizeSlider->setRange(10, 42);
    connect(m_fontSizeSlider, &QSlider::valueChanged, this, &ReadingProfileDialog::onSliderValueChanged);
    sizeRow->addWidget(m_fontSizeSlider, 1);

    m_fontSizeIncreaseBtn = new QPushButton(QStringLiteral("A+"), formGroup);
    m_fontSizeIncreaseBtn->setFixedSize(30, 26);
    connect(m_fontSizeIncreaseBtn, &QPushButton::clicked, this, &ReadingProfileDialog::onIncreaseFontSizeClicked);
    sizeRow->addWidget(m_fontSizeIncreaseBtn);

    m_fontSizeValueLabel = new QLabel(QStringLiteral("16pt"), formGroup);
    m_fontSizeValueLabel->setFixedWidth(36);
    sizeRow->addWidget(m_fontSizeValueLabel);
    formLayout->addRow(tr("Size:"), sizeRow);

    // Line Height
    auto* lhRow = new QHBoxLayout();
    m_lineHeightSlider = new QSlider(Qt::Horizontal, formGroup);
    m_lineHeightSlider->setRange(10, 25); // 1.0 to 2.5
    connect(m_lineHeightSlider, &QSlider::valueChanged, this, &ReadingProfileDialog::onSliderValueChanged);
    lhRow->addWidget(m_lineHeightSlider, 1);
    m_lineHeightValueLabel = new QLabel(QStringLiteral("1.5x"), formGroup);
    m_lineHeightValueLabel->setFixedWidth(36);
    lhRow->addWidget(m_lineHeightValueLabel);
    formLayout->addRow(tr("Line Height:"), lhRow);

    // Margins
    auto* marginRow = new QHBoxLayout();
    m_marginHSlider = new QSlider(Qt::Horizontal, formGroup);
    m_marginHSlider->setRange(8, 64);
    connect(m_marginHSlider, &QSlider::valueChanged, this, &ReadingProfileDialog::onSliderValueChanged);
    marginRow->addWidget(m_marginHSlider, 1);
    m_marginHValueLabel = new QLabel(QStringLiteral("24px"), formGroup);
    m_marginHValueLabel->setFixedWidth(36);
    marginRow->addWidget(m_marginHValueLabel);
    formLayout->addRow(tr("Margins:"), marginRow);

    // Brightness Dimming
    auto* dimRow = new QHBoxLayout();
    m_brightnessDimSlider = new QSlider(Qt::Horizontal, formGroup);
    m_brightnessDimSlider->setRange(0, 80);
    connect(m_brightnessDimSlider, &QSlider::valueChanged, this, &ReadingProfileDialog::onSliderValueChanged);
    dimRow->addWidget(m_brightnessDimSlider, 1);
    m_brightnessDimValueLabel = new QLabel(QStringLiteral("0%"), formGroup);
    m_brightnessDimValueLabel->setFixedWidth(36);
    dimRow->addWidget(m_brightnessDimValueLabel);
    formLayout->addRow(tr("Dimmer:"), dimRow);

    // Blue Light Filter
    auto* blueRow = new QHBoxLayout();
    m_blueLightSlider = new QSlider(Qt::Horizontal, formGroup);
    m_blueLightSlider->setRange(0, 80);
    connect(m_blueLightSlider, &QSlider::valueChanged, this, &ReadingProfileDialog::onSliderValueChanged);
    blueRow->addWidget(m_blueLightSlider, 1);
    m_blueLightValueLabel = new QLabel(QStringLiteral("0%"), formGroup);
    m_blueLightValueLabel->setFixedWidth(36);
    blueRow->addWidget(m_blueLightValueLabel);
    formLayout->addRow(tr("Blue Light:"), blueRow);

    mainLayout->addWidget(formGroup);

    // 5. Automatic Schedule Card
    m_scheduleSettingsCard = new QFrame(this);
    m_scheduleSettingsCard->setStyleSheet("QFrame { background: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 8px; }");
    auto* schedLayout = new QVBoxLayout(m_scheduleSettingsCard);
    schedLayout->setContentsMargins(12, 10, 12, 10);

    m_autoScheduleCheck = new QCheckBox(tr("Automatic Day / Night Profile Switching by Time of Day"), m_scheduleSettingsCard);
    m_autoScheduleCheck->setChecked(ReadingProfileManager::instance().isAutoScheduleEnabled());
    connect(m_autoScheduleCheck, &QCheckBox::toggled, this, &ReadingProfileDialog::onAutoScheduleToggled);
    schedLayout->addWidget(m_autoScheduleCheck);

    auto* timesRow = new QHBoxLayout();
    timesRow->addWidget(new QLabel(tr("Day Starts:"), m_scheduleSettingsCard));
    m_dayTimeEdit = new QTimeEdit(ReadingProfileManager::instance().dayStartTime(), m_scheduleSettingsCard);
    connect(m_dayTimeEdit, &QTimeEdit::timeChanged, this, &ReadingProfileDialog::onDayTimeChanged);
    timesRow->addWidget(m_dayTimeEdit);

    timesRow->addSpacing(16);
    timesRow->addWidget(new QLabel(tr("Night Starts:"), m_scheduleSettingsCard));
    m_nightTimeEdit = new QTimeEdit(ReadingProfileManager::instance().nightStartTime(), m_scheduleSettingsCard);
    connect(m_nightTimeEdit, &QTimeEdit::timeChanged, this, &ReadingProfileDialog::onNightTimeChanged);
    timesRow->addWidget(m_nightTimeEdit);
    timesRow->addStretch();

    schedLayout->addLayout(timesRow);
    mainLayout->addWidget(m_scheduleSettingsCard);

    // 6. Bottom Action Bar
    auto* botRow = new QHBoxLayout();
    m_saveProfileBtn = new QPushButton(tr("💾 Save to Profile"), this);
    connect(m_saveProfileBtn, &QPushButton::clicked, this, &ReadingProfileDialog::onSaveProfileClicked);
    botRow->addWidget(m_saveProfileBtn);

    botRow->addStretch();

    m_closeBtn = new QPushButton(tr("Close"), this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    botRow->addWidget(m_closeBtn);

    m_applyBtn = new QPushButton(tr("Apply Profile"), this);
    m_applyBtn->setStyleSheet("background-color: #2563EB; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(m_applyBtn, &QPushButton::clicked, this, &ReadingProfileDialog::applyCurrentProfile);
    botRow->addWidget(m_applyBtn);

    mainLayout->addLayout(botRow);

    refreshProfileList();
}

void ReadingProfileDialog::populateFontFamilies() {
    m_fontFamilyCombo->clear();
    QStringList preferred = {"Roboto", "Georgia", "Merriweather", "Palatino", "Open Sans", "Courier New"};
    for (const auto& fam : preferred) {
        m_fontFamilyCombo->addItem(fam);
    }
    const auto families = QFontDatabase::families();
    for (const auto& f : families) {
        if (!preferred.contains(f)) {
            m_fontFamilyCombo->addItem(f);
        }
    }
}

void ReadingProfileDialog::refreshProfileList() {
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    const auto profiles = ReadingProfileManager::instance().allProfiles();
    int activeIdx = 0;
    for (int i = 0; i < profiles.size(); ++i) {
        m_profileCombo->addItem(profiles[i].name);
        if (profiles[i].name == m_currentProfile.name) {
            activeIdx = i;
        }
    }
    m_profileCombo->setCurrentIndex(activeIdx);
    m_profileCombo->blockSignals(false);

    updateQuickButtonsState();
}

void ReadingProfileDialog::updateQuickButtonsState() {
    bool isDay = (m_currentProfile.name == QStringLiteral("Day Mode"));
    bool isNight = (m_currentProfile.name == QStringLiteral("Night Mode"));

    m_quickDayBtn->setStyleSheet(isDay ? "background-color: #FEF3C7; font-weight: bold; border: 2px solid #F59E0B;" : "");
    m_quickNightBtn->setStyleSheet(isNight ? "background-color: #1E293B; color: white; font-weight: bold; border: 2px solid #6366F1;" : "");

    // Built-in presets shouldn't be deleted
    bool canDelete = (m_currentProfile.name != QStringLiteral("Day Mode") &&
                      m_currentProfile.name != QStringLiteral("Night Mode") &&
                      m_currentProfile.name != QStringLiteral("OLED Pure Black"));
    m_deleteProfileBtn->setEnabled(canDelete);
}

void ReadingProfileDialog::setProfile(const ReadingProfile& profile) {
    m_currentProfile = profile;
    updateUiFromProfile(profile);
}

void ReadingProfileDialog::updateUiFromProfile(const ReadingProfile& p) {
    m_updatingUi = true;

    int fontIdx = m_fontFamilyCombo->findText(p.fontFamily);
    if (fontIdx >= 0) m_fontFamilyCombo->setCurrentIndex(fontIdx);

    m_fontSizeSlider->setValue(p.fontSize);
    m_fontSizeValueLabel->setText(QString::number(p.fontSize) + "pt");

    m_lineHeightSlider->setValue(static_cast<int>(std::round(p.lineHeight * 10)));
    m_lineHeightValueLabel->setText(QString::number(p.lineHeight, 'f', 1) + "x");

    m_marginHSlider->setValue(p.pageMarginHorizontal);
    m_marginHValueLabel->setText(QString::number(p.pageMarginHorizontal) + "px");

    m_brightnessDimSlider->setValue(p.brightnessDimPercent);
    m_brightnessDimValueLabel->setText(QString::number(p.brightnessDimPercent) + "%");

    m_blueLightSlider->setValue(p.blueLightFilterPercent);
    m_blueLightValueLabel->setText(QString::number(p.blueLightFilterPercent) + "%");

    m_previewWidget->setProfile(p);
    updateQuickButtonsState();

    m_updatingUi = false;
}

void ReadingProfileDialog::updateProfileFromUi() {
    if (m_updatingUi) return;

    m_currentProfile.fontFamily = m_fontFamilyCombo->currentText();
    m_currentProfile.fontSize = m_fontSizeSlider->value();
    m_currentProfile.lineHeight = m_lineHeightSlider->value() / 10.0;
    m_currentProfile.pageMarginHorizontal = m_marginHSlider->value();
    m_currentProfile.pageMarginVertical = m_marginHSlider->value();
    m_currentProfile.brightnessDimPercent = m_brightnessDimSlider->value();
    m_currentProfile.blueLightFilterPercent = m_blueLightSlider->value();

    m_previewWidget->setProfile(m_currentProfile);
}

void ReadingProfileDialog::onSliderValueChanged() {
    if (m_updatingUi) return;

    m_fontSizeValueLabel->setText(QString::number(m_fontSizeSlider->value()) + "pt");
    m_lineHeightValueLabel->setText(QString::number(m_lineHeightSlider->value() / 10.0, 'f', 1) + "x");
    m_marginHValueLabel->setText(QString::number(m_marginHSlider->value()) + "px");
    m_brightnessDimValueLabel->setText(QString::number(m_brightnessDimSlider->value()) + "%");
    m_blueLightValueLabel->setText(QString::number(m_blueLightSlider->value()) + "%");

    updateProfileFromUi();
}

void ReadingProfileDialog::onFontFamilyChanged(int index) {
    Q_UNUSED(index);
    updateProfileFromUi();
}

void ReadingProfileDialog::onDecreaseFontSizeClicked() {
    m_fontSizeSlider->setValue(m_fontSizeSlider->value() - 1);
}

void ReadingProfileDialog::onIncreaseFontSizeClicked() {
    m_fontSizeSlider->setValue(m_fontSizeSlider->value() + 1);
}

void ReadingProfileDialog::onProfileComboChanged(int index) {
    if (index < 0) return;
    QString name = m_profileCombo->itemText(index);
    auto p = ReadingProfileManager::instance().profile(name);
    if (p.has_value()) {
        m_currentProfile = *p;
        updateUiFromProfile(m_currentProfile);
    }
}

void ReadingProfileDialog::onQuickDayClicked() {
    auto p = ReadingProfileManager::instance().profile(QStringLiteral("Day Mode"));
    if (p.has_value()) {
        m_currentProfile = *p;
        updateUiFromProfile(m_currentProfile);
        refreshProfileList();
    }
}

void ReadingProfileDialog::onQuickNightClicked() {
    auto p = ReadingProfileManager::instance().profile(QStringLiteral("Night Mode"));
    if (p.has_value()) {
        m_currentProfile = *p;
        updateUiFromProfile(m_currentProfile);
        refreshProfileList();
    }
}

void ReadingProfileDialog::onSaveAsNewClicked() {
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Save Profile"), tr("Enter new profile name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        updateProfileFromUi();
        m_currentProfile.name = name.trimmed();
        ReadingProfileManager::instance().saveProfile(m_currentProfile);
        refreshProfileList();
    }
}

void ReadingProfileDialog::onSaveProfileClicked() {
    updateProfileFromUi();
    ReadingProfileManager::instance().saveProfile(m_currentProfile);
    QMessageBox::information(this, tr("Profile Saved"), tr("Saved profile settings for \"%1\".").arg(m_currentProfile.name));
}

void ReadingProfileDialog::onDeleteProfileClicked() {
    if (QMessageBox::question(this, tr("Delete Profile"), tr("Are you sure you want to delete profile \"%1\"?").arg(m_currentProfile.name)) == QMessageBox::Yes) {
        ReadingProfileManager::instance().deleteProfile(m_currentProfile.name);
        refreshProfileList();
        m_currentProfile = ReadingProfileManager::instance().activeProfile();
        updateUiFromProfile(m_currentProfile);
    }
}

void ReadingProfileDialog::onAutoScheduleToggled(bool checked) {
    ReadingProfileManager::instance().setAutoScheduleEnabled(checked);
}

void ReadingProfileDialog::onDayTimeChanged(const QTime& time) {
    ReadingProfileManager::instance().setDayStartTime(time);
}

void ReadingProfileDialog::onNightTimeChanged(const QTime& time) {
    ReadingProfileManager::instance().setNightStartTime(time);
}

void ReadingProfileDialog::onThemeModeSelected(ReaderThemeMode mode) {
    m_currentProfile.themeMode = mode;
    updateProfileFromUi();
}

void ReadingProfileDialog::onManagerActiveProfileChanged(const ReadingProfile& profile) {
    m_currentProfile = profile;
    updateUiFromProfile(profile);
}

void ReadingProfileDialog::onManagerProfilesChanged() {
    refreshProfileList();
}

void ReadingProfileDialog::applyCurrentProfile() {
    updateProfileFromUi();
    ReadingProfileManager::instance().setActiveProfileByObject(m_currentProfile);
    emit profileApplied(m_currentProfile);
    accept();
}

} // namespace LunaRead
