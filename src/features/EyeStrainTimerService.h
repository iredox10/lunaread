#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>

namespace LunaRead {

/**
 * @brief Service tracking active reading duration and triggering eye rest reminders.
 *
 * Implements Moon+ Reader's iconic Eye Rest / Health Reminder ("Look 20 feet away for 20 seconds
 * after 45 minutes of reading").
 *
 * Features:
 * - Tracks active reading duration (only counts when viewport is active / reading)
 * - Configurable interval (default: 45 minutes, configurable & persisted in QSettings)
 * - Emits eyeRestRequired(int sessionMinutesRead)
 * - Supports snooze (e.g. 15 minutes), mute/disable, and session reset
 */
class EyeStrainTimerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int intervalMinutes READ intervalMinutes WRITE setIntervalMinutes NOTIFY intervalChanged)
    Q_PROPERTY(int snoozeMinutes READ snoozeMinutes WRITE setSnoozeMinutes NOTIFY snoozeMinutesChanged)
    Q_PROPERTY(bool readingActive READ isReadingActive WRITE setReadingActive NOTIFY readingActiveChanged)
    Q_PROPERTY(int sessionMinutesRead READ sessionMinutesRead NOTIFY sessionTimeUpdated)

public:
    static constexpr int DefaultIntervalMinutes = 45;
    static constexpr int DefaultSnoozeMinutes = 15;
    static constexpr int DefaultRestDurationSeconds = 20;

    explicit EyeStrainTimerService(QObject* parent = nullptr);
    ~EyeStrainTimerService() override = default;

    // Active reading state
    [[nodiscard]] bool isReadingActive() const noexcept { return m_isReadingActive; }
    void setReadingActive(bool active);
    void start() { setReadingActive(true); }
    void pause() { setReadingActive(false); }
    void resume() { setReadingActive(true); }

    // Enabled state
    [[nodiscard]] bool isEnabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);

    // Interval & Snooze settings
    [[nodiscard]] int intervalMinutes() const noexcept { return m_intervalMinutes; }
    void setIntervalMinutes(int minutes);

    [[nodiscard]] int snoozeMinutes() const noexcept { return m_snoozeMinutes; }
    void setSnoozeMinutes(int minutes);

    // Snooze, Dismiss, and Reset
    void snooze(int minutes = DefaultSnoozeMinutes);
    void dismissRest();
    void resetSession();

    // Time queries
    [[nodiscard]] bool isSnoozed() const noexcept { return m_isSnoozed; }
    [[nodiscard]] int sessionSecondsRead() const noexcept;
    [[nodiscard]] int sessionMinutesRead() const noexcept;
    [[nodiscard]] int currentIntervalSeconds() const noexcept;
    [[nodiscard]] int remainingSecondsUntilRest() const noexcept;

    // QSettings persistence
    void loadSettings();
    void saveSettings();

    // Deterministic testing helper
    void advanceTimeForTesting(int seconds);

signals:
    void eyeRestRequired(int sessionMinutesRead);
    void readingActiveChanged(bool active);
    void enabledChanged(bool enabled);
    void intervalChanged(int intervalMinutes);
    void snoozeMinutesChanged(int snoozeMinutes);
    void sessionTimeUpdated(int sessionMinutesRead);
    void tick(int remainingSeconds);
    void snoozed(int minutes);
    void sessionReset();

private slots:
    void onTick();

private:
    void checkAndEmitReminder();

    bool m_enabled = true;
    bool m_isReadingActive = false;
    bool m_isSnoozed = false;
    int m_intervalMinutes = DefaultIntervalMinutes;
    int m_snoozeMinutes = DefaultSnoozeMinutes;

    qint64 m_accumulatedMs = 0;
    qint64 m_intervalMs = 0;
    qint64 m_targetIntervalMs = 0;

    QTimer* m_timer = nullptr;
    QElapsedTimer m_elapsedTimer;
};

} // namespace LunaRead
