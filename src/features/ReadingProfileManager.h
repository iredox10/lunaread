#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <optional>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

/**
 * @brief Structure representing a personalized reading profile.
 *
 * Encapsulates typography preferences, page margins, color theme palettes,
 * and eye-care night filter settings (brightness dimming and blue light filter).
 */
struct ReadingProfile {
    QString name = QStringLiteral("Day Mode");
    ReaderThemeMode themeMode = ReaderThemeMode::ModernLight;
    QString fontFamily = QStringLiteral("Roboto");
    int fontSize = 16;
    qreal lineHeight = 1.5;
    qreal letterSpacing = 0.0;
    qreal paragraphSpacing = 12.0;
    int pageMarginHorizontal = 24;
    int pageMarginVertical = 24;
    int brightnessDimPercent = 0;   // 0 to 80%
    int blueLightFilterPercent = 0; // 0 to 80%

    bool operator==(const ReadingProfile& other) const = default;
    bool operator!=(const ReadingProfile& other) const = default;

    [[nodiscard]] QJsonObject toJson() const;
    static ReadingProfile fromJson(const QJsonObject& obj);

    static QString themeModeToString(ReaderThemeMode mode);
    static ReaderThemeMode themeModeFromString(const QString& str);

    static ReadingProfile defaultDayProfile();
    static ReadingProfile defaultNightProfile();
    static ReadingProfile defaultOledProfile();
    static ReadingProfile defaultSepiaProfile();
    static ReadingProfile defaultParchmentProfile();
};

/**
 * @brief Singleton manager handling reading profiles and automatic Day/Night switching.
 *
 * Implements Moon+ Reader's dual reading profiles (Day & Night) with automatic
 * sunset / time-of-day scheduling, custom profiles persistence, and active profile broadcast.
 */
class ReadingProfileManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(ReadingProfile activeProfile READ activeProfile WRITE setActiveProfileByObject NOTIFY activeProfileChanged)
    Q_PROPERTY(bool autoScheduleEnabled READ isAutoScheduleEnabled WRITE setAutoScheduleEnabled NOTIFY autoScheduleChanged)
    Q_PROPERTY(QTime dayStartTime READ dayStartTime WRITE setDayStartTime NOTIFY scheduleTimesChanged)
    Q_PROPERTY(QTime nightStartTime READ nightStartTime WRITE setNightStartTime NOTIFY scheduleTimesChanged)

public:
    static ReadingProfileManager& instance();

    // Profiles access & management
    [[nodiscard]] ReadingProfile activeProfile() const;
    void setActiveProfile(const QString& name);
    void setActiveProfileByObject(const ReadingProfile& profile);
    [[nodiscard]] QList<ReadingProfile> allProfiles() const;
    [[nodiscard]] std::optional<ReadingProfile> profile(const QString& name) const;
    [[nodiscard]] bool hasProfile(const QString& name) const;

    void saveProfile(const ReadingProfile& profile);
    void deleteProfile(const QString& name);
    void resetToDefaults();

    // Time-based automatic Day/Night schedule
    [[nodiscard]] bool isAutoScheduleEnabled() const noexcept { return m_autoScheduleEnabled; }
    void setAutoScheduleEnabled(bool enabled);

    [[nodiscard]] QTime dayStartTime() const noexcept { return m_dayStartTime; }
    void setDayStartTime(const QTime& time);

    [[nodiscard]] QTime nightStartTime() const noexcept { return m_nightStartTime; }
    void setNightStartTime(const QTime& time);

    [[nodiscard]] QString dayProfileName() const { return m_dayProfileName; }
    void setDayProfileName(const QString& name);

    [[nodiscard]] QString nightProfileName() const { return m_nightProfileName; }
    void setNightProfileName(const QString& name);

    [[nodiscard]] bool isNightTime(const QTime& time) const;
    void checkSchedule();
    void triggerScheduleCheck(bool forceSwitch = false);

    // Persistence
    void loadSettings();
    void saveSettings();

    // Deterministic time simulation for testing
    void setSimulatedTime(const QTime& time);
    void clearSimulatedTime();

signals:
    void activeProfileChanged(const ReadingProfile& profile);
    void profileSaved(const ReadingProfile& profile);
    void profileDeleted(const QString& name);
    void profilesChanged();
    void autoScheduleChanged(bool enabled);
    void scheduleTimesChanged(const QTime& dayTime, const QTime& nightTime);

private slots:
    void onScheduleTimerTick();

private:
    explicit ReadingProfileManager(QObject* parent = nullptr);
    ~ReadingProfileManager() override = default;

    ReadingProfileManager(const ReadingProfileManager&) = delete;
    ReadingProfileManager& operator=(const ReadingProfileManager&) = delete;

    void initializeDefaultProfiles();
    [[nodiscard]] QTime currentEffectiveTime() const;

    enum class SchedulePhase {
        Unknown,
        Day,
        Night
    };

    QList<ReadingProfile> m_profiles;
    ReadingProfile m_activeProfile;
    QString m_activeProfileName = QStringLiteral("Day Mode");

    bool m_autoScheduleEnabled = false;
    QTime m_dayStartTime{7, 0};     // 07:00 AM
    QTime m_nightStartTime{20, 0};  // 08:00 PM (20:00)
    QString m_dayProfileName = QStringLiteral("Day Mode");
    QString m_nightProfileName = QStringLiteral("Night Mode");

    SchedulePhase m_lastSchedulePhase = SchedulePhase::Unknown;
    QTimer* m_scheduleTimer = nullptr;
    std::optional<QTime> m_simulatedTime;
};

} // namespace LunaRead
