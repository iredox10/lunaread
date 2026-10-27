#include "ReadingProfileManager.h"
#include <QSettings>
#include <algorithm>
#include <cmath>

namespace LunaRead {

// ============================================================================
// ReadingProfile Serialization & Helper Implementations
// ============================================================================

QString ReadingProfile::themeModeToString(ReaderThemeMode mode) {
    switch (mode) {
        case ReaderThemeMode::Parchment:
            return QStringLiteral("Parchment");
        case ReaderThemeMode::OledBlack:
            return QStringLiteral("OledBlack");
        case ReaderThemeMode::Sepia:
            return QStringLiteral("Sepia");
        case ReaderThemeMode::Night:
            return QStringLiteral("Night");
        case ReaderThemeMode::ModernLight:
            return QStringLiteral("ModernLight");
    }
    return QStringLiteral("ModernLight");
}

ReaderThemeMode ReadingProfile::themeModeFromString(const QString& str) {
    if (str.compare(QStringLiteral("Parchment"), Qt::CaseInsensitive) == 0) {
        return ReaderThemeMode::Parchment;
    }
    if (str.compare(QStringLiteral("OledBlack"), Qt::CaseInsensitive) == 0 ||
        str.compare(QStringLiteral("PureBlack"), Qt::CaseInsensitive) == 0) {
        return ReaderThemeMode::OledBlack;
    }
    if (str.compare(QStringLiteral("Sepia"), Qt::CaseInsensitive) == 0) {
        return ReaderThemeMode::Sepia;
    }
    if (str.compare(QStringLiteral("Night"), Qt::CaseInsensitive) == 0 ||
        str.compare(QStringLiteral("NightCharcoal"), Qt::CaseInsensitive) == 0) {
        return ReaderThemeMode::Night;
    }
    return ReaderThemeMode::ModernLight;
}

QJsonObject ReadingProfile::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("themeMode")] = themeModeToString(themeMode);
    obj[QStringLiteral("fontFamily")] = fontFamily;
    obj[QStringLiteral("fontSize")] = fontSize;
    obj[QStringLiteral("lineHeight")] = lineHeight;
    obj[QStringLiteral("letterSpacing")] = letterSpacing;
    obj[QStringLiteral("paragraphSpacing")] = paragraphSpacing;
    obj[QStringLiteral("pageMarginHorizontal")] = pageMarginHorizontal;
    obj[QStringLiteral("pageMarginVertical")] = pageMarginVertical;
    obj[QStringLiteral("brightnessDimPercent")] = brightnessDimPercent;
    obj[QStringLiteral("blueLightFilterPercent")] = blueLightFilterPercent;
    return obj;
}

ReadingProfile ReadingProfile::fromJson(const QJsonObject& obj) {
    ReadingProfile p;
    if (obj.contains(QStringLiteral("name"))) {
        p.name = obj[QStringLiteral("name")].toString(QStringLiteral("Custom"));
    }
    if (obj.contains(QStringLiteral("themeMode"))) {
        p.themeMode = themeModeFromString(obj[QStringLiteral("themeMode")].toString());
    }
    if (obj.contains(QStringLiteral("fontFamily"))) {
        p.fontFamily = obj[QStringLiteral("fontFamily")].toString(QStringLiteral("Roboto"));
    }
    if (obj.contains(QStringLiteral("fontSize"))) {
        p.fontSize = obj[QStringLiteral("fontSize")].toInt(16);
    }
    if (obj.contains(QStringLiteral("lineHeight"))) {
        p.lineHeight = obj[QStringLiteral("lineHeight")].toDouble(1.5);
    }
    if (obj.contains(QStringLiteral("letterSpacing"))) {
        p.letterSpacing = obj[QStringLiteral("letterSpacing")].toDouble(0.0);
    }
    if (obj.contains(QStringLiteral("paragraphSpacing"))) {
        p.paragraphSpacing = obj[QStringLiteral("paragraphSpacing")].toDouble(12.0);
    }
    if (obj.contains(QStringLiteral("pageMarginHorizontal"))) {
        p.pageMarginHorizontal = obj[QStringLiteral("pageMarginHorizontal")].toInt(24);
    }
    if (obj.contains(QStringLiteral("pageMarginVertical"))) {
        p.pageMarginVertical = obj[QStringLiteral("pageMarginVertical")].toInt(24);
    }
    if (obj.contains(QStringLiteral("brightnessDimPercent"))) {
        p.brightnessDimPercent = obj[QStringLiteral("brightnessDimPercent")].toInt(0);
    }
    if (obj.contains(QStringLiteral("blueLightFilterPercent"))) {
        p.blueLightFilterPercent = obj[QStringLiteral("blueLightFilterPercent")].toInt(0);
    }

    p.brightnessDimPercent = std::clamp(p.brightnessDimPercent, 0, 80);
    p.blueLightFilterPercent = std::clamp(p.blueLightFilterPercent, 0, 80);
    p.fontSize = std::clamp(p.fontSize, 10, 72);
    p.lineHeight = std::clamp(p.lineHeight, 1.0, 3.0);
    p.letterSpacing = std::clamp(p.letterSpacing, -2.0, 10.0);
    p.paragraphSpacing = std::clamp(p.paragraphSpacing, 0.0, 50.0);
    p.pageMarginHorizontal = std::clamp(p.pageMarginHorizontal, 8, 120);
    p.pageMarginVertical = std::clamp(p.pageMarginVertical, 8, 120);

    return p;
}

ReadingProfile ReadingProfile::defaultDayProfile() {
    ReadingProfile p;
    p.name = QStringLiteral("Day Mode");
    p.themeMode = ReaderThemeMode::ModernLight;
    p.fontFamily = QStringLiteral("Roboto");
    p.fontSize = 16;
    p.lineHeight = 1.5;
    p.letterSpacing = 0.0;
    p.paragraphSpacing = 12.0;
    p.pageMarginHorizontal = 24;
    p.pageMarginVertical = 24;
    p.brightnessDimPercent = 0;
    p.blueLightFilterPercent = 0;
    return p;
}

ReadingProfile ReadingProfile::defaultNightProfile() {
    ReadingProfile p;
    p.name = QStringLiteral("Night Mode");
    p.themeMode = ReaderThemeMode::Night;
    p.fontFamily = QStringLiteral("Roboto");
    p.fontSize = 16;
    p.lineHeight = 1.5;
    p.letterSpacing = 0.0;
    p.paragraphSpacing = 12.0;
    p.pageMarginHorizontal = 28;
    p.pageMarginVertical = 28;
    p.brightnessDimPercent = 25;
    p.blueLightFilterPercent = 40;
    return p;
}

ReadingProfile ReadingProfile::defaultOledProfile() {
    ReadingProfile p;
    p.name = QStringLiteral("OLED Pure Black");
    p.themeMode = ReaderThemeMode::OledBlack;
    p.fontFamily = QStringLiteral("Roboto");
    p.fontSize = 16;
    p.lineHeight = 1.5;
    p.letterSpacing = 0.0;
    p.paragraphSpacing = 12.0;
    p.pageMarginHorizontal = 28;
    p.pageMarginVertical = 28;
    p.brightnessDimPercent = 20;
    p.blueLightFilterPercent = 30;
    return p;
}

ReadingProfile ReadingProfile::defaultSepiaProfile() {
    ReadingProfile p;
    p.name = QStringLiteral("Paper Sepia");
    p.themeMode = ReaderThemeMode::Sepia;
    p.fontFamily = QStringLiteral("Georgia");
    p.fontSize = 16;
    p.lineHeight = 1.6;
    p.letterSpacing = 0.2;
    p.paragraphSpacing = 14.0;
    p.pageMarginHorizontal = 30;
    p.pageMarginVertical = 26;
    p.brightnessDimPercent = 10;
    p.blueLightFilterPercent = 20;
    return p;
}

ReadingProfile ReadingProfile::defaultParchmentProfile() {
    ReadingProfile p;
    p.name = QStringLiteral("Parchment");
    p.themeMode = ReaderThemeMode::Parchment;
    p.fontFamily = QStringLiteral("Georgia");
    p.fontSize = 16;
    p.lineHeight = 1.5;
    p.letterSpacing = 0.0;
    p.paragraphSpacing = 12.0;
    p.pageMarginHorizontal = 24;
    p.pageMarginVertical = 24;
    p.brightnessDimPercent = 5;
    p.blueLightFilterPercent = 15;
    return p;
}

// ============================================================================
// ReadingProfileManager Implementation
// ============================================================================

ReadingProfileManager& ReadingProfileManager::instance() {
    static ReadingProfileManager s_instance;
    return s_instance;
}

ReadingProfileManager::ReadingProfileManager(QObject* parent)
    : QObject(parent),
      m_scheduleTimer(new QTimer(this)) {
    connect(m_scheduleTimer, &QTimer::timeout, this, &ReadingProfileManager::onScheduleTimerTick);
    loadSettings();
    m_scheduleTimer->start(15000); // Check schedule every 15 seconds
}

void ReadingProfileManager::initializeDefaultProfiles() {
    m_profiles.clear();
    m_profiles.append(ReadingProfile::defaultDayProfile());
    m_profiles.append(ReadingProfile::defaultNightProfile());
    m_profiles.append(ReadingProfile::defaultOledProfile());
    m_profiles.append(ReadingProfile::defaultSepiaProfile());
    m_profiles.append(ReadingProfile::defaultParchmentProfile());
}

ReadingProfile ReadingProfileManager::activeProfile() const {
    return m_activeProfile;
}

void ReadingProfileManager::setActiveProfile(const QString& name) {
    for (const auto& p : m_profiles) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) {
            if (m_activeProfile.name != p.name || m_activeProfile != p) {
                m_activeProfile = p;
                m_activeProfileName = p.name;
                saveSettings();
                emit activeProfileChanged(m_activeProfile);
            }
            return;
        }
    }
}

void ReadingProfileManager::setActiveProfileByObject(const ReadingProfile& profile) {
    saveProfile(profile);
    setActiveProfile(profile.name);
}

QList<ReadingProfile> ReadingProfileManager::allProfiles() const {
    return m_profiles;
}

std::optional<ReadingProfile> ReadingProfileManager::profile(const QString& name) const {
    for (const auto& p : m_profiles) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) {
            return p;
        }
    }
    return std::nullopt;
}

bool ReadingProfileManager::hasProfile(const QString& name) const {
    for (const auto& p : m_profiles) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void ReadingProfileManager::saveProfile(const ReadingProfile& profile) {
    if (profile.name.trimmed().isEmpty()) {
        return;
    }

    ReadingProfile p = profile;
    p.name = p.name.trimmed();
    p.brightnessDimPercent = std::clamp(p.brightnessDimPercent, 0, 80);
    p.blueLightFilterPercent = std::clamp(p.blueLightFilterPercent, 0, 80);
    p.fontSize = std::clamp(p.fontSize, 10, 72);
    p.lineHeight = std::clamp(p.lineHeight, 1.0, 3.0);
    p.letterSpacing = std::clamp(p.letterSpacing, -2.0, 10.0);
    p.paragraphSpacing = std::clamp(p.paragraphSpacing, 0.0, 50.0);
    p.pageMarginHorizontal = std::clamp(p.pageMarginHorizontal, 8, 120);
    p.pageMarginVertical = std::clamp(p.pageMarginVertical, 8, 120);

    bool updated = false;
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].name.compare(p.name, Qt::CaseInsensitive) == 0) {
            m_profiles[i] = p;
            updated = true;
            break;
        }
    }

    if (!updated) {
        m_profiles.append(p);
    }

    if (m_activeProfile.name.compare(p.name, Qt::CaseInsensitive) == 0) {
        m_activeProfile = p;
        m_activeProfileName = p.name;
        emit activeProfileChanged(m_activeProfile);
    }

    saveSettings();
    emit profileSaved(p);
    emit profilesChanged();
}

void ReadingProfileManager::deleteProfile(const QString& name) {
    if (name.compare(QStringLiteral("Day Mode"), Qt::CaseInsensitive) == 0 ||
        name.compare(QStringLiteral("Night Mode"), Qt::CaseInsensitive) == 0) {
        return;
    }

    int targetIndex = -1;
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].name.compare(name, Qt::CaseInsensitive) == 0) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex < 0 || m_profiles.size() <= 1) {
        return;
    }

    const QString deletedName = m_profiles[targetIndex].name;
    m_profiles.removeAt(targetIndex);

    if (m_activeProfile.name.compare(deletedName, Qt::CaseInsensitive) == 0) {
        if (!m_profiles.isEmpty()) {
            setActiveProfile(m_profiles.first().name);
        }
    }

    saveSettings();
    emit profileDeleted(deletedName);
    emit profilesChanged();
}

void ReadingProfileManager::resetToDefaults() {
    m_autoScheduleEnabled = false;
    m_dayStartTime = QTime(7, 0);
    m_nightStartTime = QTime(20, 0);
    m_dayProfileName = QStringLiteral("Day Mode");
    m_nightProfileName = QStringLiteral("Night Mode");
    m_lastSchedulePhase = SchedulePhase::Unknown;

    initializeDefaultProfiles();
    m_activeProfile = m_profiles.first();
    m_activeProfileName = m_activeProfile.name;

    saveSettings();
    emit autoScheduleChanged(m_autoScheduleEnabled);
    emit scheduleTimesChanged(m_dayStartTime, m_nightStartTime);
    emit activeProfileChanged(m_activeProfile);
    emit profilesChanged();
}

void ReadingProfileManager::setAutoScheduleEnabled(bool enabled) {
    if (m_autoScheduleEnabled == enabled) {
        return;
    }
    m_autoScheduleEnabled = enabled;
    m_lastSchedulePhase = SchedulePhase::Unknown;
    saveSettings();
    emit autoScheduleChanged(m_autoScheduleEnabled);

    if (m_autoScheduleEnabled) {
        triggerScheduleCheck(true);
    }
}

void ReadingProfileManager::setDayStartTime(const QTime& time) {
    if (!time.isValid() || m_dayStartTime == time) {
        return;
    }
    m_dayStartTime = time;
    saveSettings();
    emit scheduleTimesChanged(m_dayStartTime, m_nightStartTime);

    if (m_autoScheduleEnabled) {
        checkSchedule();
    }
}

void ReadingProfileManager::setNightStartTime(const QTime& time) {
    if (!time.isValid() || m_nightStartTime == time) {
        return;
    }
    m_nightStartTime = time;
    saveSettings();
    emit scheduleTimesChanged(m_dayStartTime, m_nightStartTime);

    if (m_autoScheduleEnabled) {
        checkSchedule();
    }
}

void ReadingProfileManager::setDayProfileName(const QString& name) {
    if (name.isEmpty() || m_dayProfileName == name) {
        return;
    }
    m_dayProfileName = name;
    saveSettings();

    if (m_autoScheduleEnabled && !isNightTime(currentEffectiveTime())) {
        checkSchedule();
    }
}

void ReadingProfileManager::setNightProfileName(const QString& name) {
    if (name.isEmpty() || m_nightProfileName == name) {
        return;
    }
    m_nightProfileName = name;
    saveSettings();

    if (m_autoScheduleEnabled && isNightTime(currentEffectiveTime())) {
        checkSchedule();
    }
}

bool ReadingProfileManager::isNightTime(const QTime& time) const {
    if (m_dayStartTime < m_nightStartTime) {
        // Standard schedule: e.g. Day from 07:00 to 20:00, Night from 20:00 to 07:00
        return time < m_dayStartTime || time >= m_nightStartTime;
    } else if (m_dayStartTime > m_nightStartTime) {
        // Inverted schedule: e.g. Night from 20:00 to 07:00 where night starts before day
        return time >= m_nightStartTime && time < m_dayStartTime;
    }
    return false;
}

QTime ReadingProfileManager::currentEffectiveTime() const {
    if (m_simulatedTime.has_value()) {
        return *m_simulatedTime;
    }
    return QTime::currentTime();
}

void ReadingProfileManager::setSimulatedTime(const QTime& time) {
    m_simulatedTime = time;
    if (m_autoScheduleEnabled) {
        checkSchedule();
    }
}

void ReadingProfileManager::clearSimulatedTime() {
    m_simulatedTime.reset();
    if (m_autoScheduleEnabled) {
        checkSchedule();
    }
}

void ReadingProfileManager::onScheduleTimerTick() {
    if (!m_autoScheduleEnabled) {
        return;
    }
    checkSchedule();
}

void ReadingProfileManager::checkSchedule() {
    if (!m_autoScheduleEnabled) {
        return;
    }

    const QTime now = currentEffectiveTime();
    const bool isNight = isNightTime(now);
    const SchedulePhase currentPhase = isNight ? SchedulePhase::Night : SchedulePhase::Day;

    if (currentPhase != m_lastSchedulePhase) {
        m_lastSchedulePhase = currentPhase;
        const QString targetName = isNight ? m_nightProfileName : m_dayProfileName;
        if (m_activeProfile.name != targetName && hasProfile(targetName)) {
            setActiveProfile(targetName);
        }
    }
}

void ReadingProfileManager::triggerScheduleCheck(bool forceSwitch) {
    if (!m_autoScheduleEnabled) {
        return;
    }

    const QTime now = currentEffectiveTime();
    const bool isNight = isNightTime(now);
    const SchedulePhase currentPhase = isNight ? SchedulePhase::Night : SchedulePhase::Day;

    if (forceSwitch) {
        m_lastSchedulePhase = currentPhase;
        const QString targetName = isNight ? m_nightProfileName : m_dayProfileName;
        if (hasProfile(targetName)) {
            setActiveProfile(targetName);
        }
    } else {
        checkSchedule();
    }
}

void ReadingProfileManager::saveSettings() {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    settings.beginGroup(QStringLiteral("ReadingProfiles"));
    settings.setValue(QStringLiteral("activeProfile"), m_activeProfileName);
    settings.setValue(QStringLiteral("autoScheduleEnabled"), m_autoScheduleEnabled);
    settings.setValue(QStringLiteral("dayStartTime"), m_dayStartTime.toString(QStringLiteral("hh:mm")));
    settings.setValue(QStringLiteral("nightStartTime"), m_nightStartTime.toString(QStringLiteral("hh:mm")));
    settings.setValue(QStringLiteral("dayProfileName"), m_dayProfileName);
    settings.setValue(QStringLiteral("nightProfileName"), m_nightProfileName);

    QJsonArray arr;
    for (const auto& p : m_profiles) {
        arr.append(p.toJson());
    }
    settings.setValue(QStringLiteral("profilesJson"), QJsonDocument(arr).toJson(QJsonDocument::Compact));
    settings.endGroup();
}

void ReadingProfileManager::loadSettings() {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    settings.beginGroup(QStringLiteral("ReadingProfiles"));

    m_autoScheduleEnabled = settings.value(QStringLiteral("autoScheduleEnabled"), false).toBool();

    const QString dayTimeStr = settings.value(QStringLiteral("dayStartTime"), QStringLiteral("07:00")).toString();
    const QTime parsedDayTime = QTime::fromString(dayTimeStr, QStringLiteral("hh:mm"));
    if (parsedDayTime.isValid()) {
        m_dayStartTime = parsedDayTime;
    }

    const QString nightTimeStr = settings.value(QStringLiteral("nightStartTime"), QStringLiteral("20:00")).toString();
    const QTime parsedNightTime = QTime::fromString(nightTimeStr, QStringLiteral("hh:mm"));
    if (parsedNightTime.isValid()) {
        m_nightStartTime = parsedNightTime;
    }

    m_dayProfileName = settings.value(QStringLiteral("dayProfileName"), QStringLiteral("Day Mode")).toString();
    m_nightProfileName = settings.value(QStringLiteral("nightProfileName"), QStringLiteral("Night Mode")).toString();

    const QByteArray profilesData = settings.value(QStringLiteral("profilesJson")).toByteArray();
    m_profiles.clear();

    if (!profilesData.isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(profilesData, &err);
        if (err.error == QJsonParseError::NoError && doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const auto& val : arr) {
                if (val.isObject()) {
                    m_profiles.append(ReadingProfile::fromJson(val.toObject()));
                }
            }
        }
    }

    if (m_profiles.isEmpty()) {
        initializeDefaultProfiles();
    }

    const QString savedActiveName = settings.value(QStringLiteral("activeProfile"), QStringLiteral("Day Mode")).toString();
    settings.endGroup();

    bool found = false;
    for (const auto& p : m_profiles) {
        if (p.name.compare(savedActiveName, Qt::CaseInsensitive) == 0) {
            m_activeProfile = p;
            m_activeProfileName = p.name;
            found = true;
            break;
        }
    }

    if (!found && !m_profiles.isEmpty()) {
        m_activeProfile = m_profiles.first();
        m_activeProfileName = m_activeProfile.name;
    }

    if (m_autoScheduleEnabled) {
        checkSchedule();
    }
}

} // namespace LunaRead
