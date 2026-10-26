#include "EyeStrainTimerService.h"
#include <algorithm>

namespace LunaRead {

EyeStrainTimerService::EyeStrainTimerService(QObject* parent)
    : QObject(parent),
      m_timer(new QTimer(this)) {
    m_targetIntervalMs = static_cast<qint64>(m_intervalMinutes) * 60 * 1000;
    connect(m_timer, &QTimer::timeout, this, &EyeStrainTimerService::onTick);
    loadSettings();
}

void EyeStrainTimerService::setReadingActive(bool active) {
    if (m_isReadingActive == active) {
        return;
    }

    m_isReadingActive = active;

    if (m_isReadingActive) {
        if (m_enabled) {
            m_elapsedTimer.start();
            if (!m_timer->isActive()) {
                m_timer->start(1000);
            }
        }
    } else {
        if (m_elapsedTimer.isValid()) {
            const qint64 elapsedMs = m_elapsedTimer.restart();
            if (elapsedMs > 0) {
                m_accumulatedMs += elapsedMs;
                m_intervalMs += elapsedMs;
            }
            m_elapsedTimer.invalidate();
        }
        if (m_timer->isActive()) {
            m_timer->stop();
        }
        emit sessionTimeUpdated(sessionMinutesRead());
    }

    emit readingActiveChanged(m_isReadingActive);
}

void EyeStrainTimerService::setEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;

    if (!m_enabled) {
        if (m_elapsedTimer.isValid()) {
            const qint64 elapsedMs = m_elapsedTimer.restart();
            if (elapsedMs > 0) {
                m_accumulatedMs += elapsedMs;
                m_intervalMs += elapsedMs;
            }
            m_elapsedTimer.invalidate();
        }
        if (m_timer->isActive()) {
            m_timer->stop();
        }
    } else if (m_isReadingActive) {
        m_elapsedTimer.start();
        if (!m_timer->isActive()) {
            m_timer->start(1000);
        }
    }

    saveSettings();
    emit enabledChanged(m_enabled);
}

void EyeStrainTimerService::setIntervalMinutes(int minutes) {
    const int validMinutes = std::max(1, minutes);
    if (m_intervalMinutes == validMinutes) {
        return;
    }

    m_intervalMinutes = validMinutes;
    if (!m_isSnoozed) {
        m_targetIntervalMs = static_cast<qint64>(m_intervalMinutes) * 60 * 1000;
    }

    saveSettings();
    emit intervalChanged(m_intervalMinutes);
    emit tick(remainingSecondsUntilRest());
}

void EyeStrainTimerService::setSnoozeMinutes(int minutes) {
    const int validMinutes = std::max(1, minutes);
    if (m_snoozeMinutes == validMinutes) {
        return;
    }

    m_snoozeMinutes = validMinutes;
    if (m_isSnoozed) {
        m_targetIntervalMs = static_cast<qint64>(m_snoozeMinutes) * 60 * 1000;
    }

    saveSettings();
    emit snoozeMinutesChanged(m_snoozeMinutes);
}

void EyeStrainTimerService::snooze(int minutes) {
    const int validMinutes = minutes > 0 ? minutes : m_snoozeMinutes;
    m_isSnoozed = true;
    m_targetIntervalMs = static_cast<qint64>(validMinutes) * 60 * 1000;
    m_intervalMs = 0;

    if (m_isReadingActive && m_enabled) {
        m_elapsedTimer.restart();
    }

    emit snoozed(validMinutes);
    emit tick(remainingSecondsUntilRest());
}

void EyeStrainTimerService::dismissRest() {
    m_isSnoozed = false;
    m_targetIntervalMs = static_cast<qint64>(m_intervalMinutes) * 60 * 1000;
    m_intervalMs = 0;

    if (m_isReadingActive && m_enabled) {
        m_elapsedTimer.restart();
    }

    emit tick(remainingSecondsUntilRest());
}

void EyeStrainTimerService::resetSession() {
    m_accumulatedMs = 0;
    m_intervalMs = 0;
    m_isSnoozed = false;
    m_targetIntervalMs = static_cast<qint64>(m_intervalMinutes) * 60 * 1000;

    if (m_isReadingActive && m_enabled) {
        m_elapsedTimer.restart();
    }

    emit sessionReset();
    emit sessionTimeUpdated(0);
    emit tick(remainingSecondsUntilRest());
}

int EyeStrainTimerService::sessionSecondsRead() const noexcept {
    qint64 extraMs = 0;
    if (m_isReadingActive && m_enabled && m_elapsedTimer.isValid()) {
        extraMs = m_elapsedTimer.elapsed();
    }
    return static_cast<int>((m_accumulatedMs + extraMs) / 1000);
}

int EyeStrainTimerService::sessionMinutesRead() const noexcept {
    return sessionSecondsRead() / 60;
}

int EyeStrainTimerService::currentIntervalSeconds() const noexcept {
    qint64 extraMs = 0;
    if (m_isReadingActive && m_enabled && m_elapsedTimer.isValid()) {
        extraMs = m_elapsedTimer.elapsed();
    }
    return static_cast<int>((m_intervalMs + extraMs) / 1000);
}

int EyeStrainTimerService::remainingSecondsUntilRest() const noexcept {
    const qint64 curIntervalMs = static_cast<qint64>(currentIntervalSeconds()) * 1000;
    const qint64 remainingMs = m_targetIntervalMs - curIntervalMs;
    if (remainingMs <= 0) {
        return 0;
    }
    return static_cast<int>((remainingMs + 999) / 1000);
}

void EyeStrainTimerService::loadSettings() {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    m_enabled = settings.value(QStringLiteral("EyeStrain/Enabled"), true).toBool();

    const int interval = settings.value(QStringLiteral("EyeStrain/IntervalMinutes"), DefaultIntervalMinutes).toInt();
    if (interval > 0) {
        m_intervalMinutes = interval;
    }

    const int snoozeVal = settings.value(QStringLiteral("EyeStrain/SnoozeMinutes"), DefaultSnoozeMinutes).toInt();
    if (snoozeVal > 0) {
        m_snoozeMinutes = snoozeVal;
    }

    if (!m_isSnoozed) {
        m_targetIntervalMs = static_cast<qint64>(m_intervalMinutes) * 60 * 1000;
    }
}

void EyeStrainTimerService::saveSettings() {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    settings.setValue(QStringLiteral("EyeStrain/Enabled"), m_enabled);
    settings.setValue(QStringLiteral("EyeStrain/IntervalMinutes"), m_intervalMinutes);
    settings.setValue(QStringLiteral("EyeStrain/SnoozeMinutes"), m_snoozeMinutes);
}

void EyeStrainTimerService::advanceTimeForTesting(int seconds) {
    if (seconds <= 0) {
        return;
    }

    const qint64 addMs = static_cast<qint64>(seconds) * 1000;
    m_accumulatedMs += addMs;
    m_intervalMs += addMs;

    emit tick(remainingSecondsUntilRest());
    emit sessionTimeUpdated(sessionMinutesRead());

    checkAndEmitReminder();
}

void EyeStrainTimerService::onTick() {
    if (!m_isReadingActive || !m_enabled) {
        if (m_timer->isActive()) {
            m_timer->stop();
        }
        return;
    }

    if (m_elapsedTimer.isValid()) {
        const qint64 elapsedMs = m_elapsedTimer.restart();
        if (elapsedMs > 0) {
            m_accumulatedMs += elapsedMs;
            m_intervalMs += elapsedMs;
        }
    } else {
        m_elapsedTimer.start();
    }

    emit tick(remainingSecondsUntilRest());
    emit sessionTimeUpdated(sessionMinutesRead());

    checkAndEmitReminder();
}

void EyeStrainTimerService::checkAndEmitReminder() {
    if (m_intervalMs >= m_targetIntervalMs) {
        const int sessionMinutes = sessionMinutesRead();
        m_intervalMs = 0;

        if (m_isSnoozed) {
            m_isSnoozed = false;
            m_targetIntervalMs = static_cast<qint64>(m_intervalMinutes) * 60 * 1000;
        }

        emit eyeRestRequired(sessionMinutes);
    }
}

} // namespace LunaRead
