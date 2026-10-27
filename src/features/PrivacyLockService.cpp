#include "PrivacyLockService.h"
#include "../utils/HashUtils.h"

#include <QSettings>
#include <QRandomGenerator>
#include <QByteArray>

namespace LunaRead {

namespace {
    const QString s_settingsGroup = QStringLiteral("LunaRead");
    const QString s_keyPinHash = QStringLiteral("Privacy/PinHash");
    const QString s_keyPinSalt = QStringLiteral("Privacy/PinSalt");
    const QString s_keyRequireStartup = QStringLiteral("Privacy/RequireOnStartup");
    const QString s_keyPrivateBooks = QStringLiteral("Privacy/PrivateBooks");
}

PrivacyLockService& PrivacyLockService::instance() {
    static PrivacyLockService s_instance;
    return s_instance;
}

PrivacyLockService::PrivacyLockService(QObject* parent)
    : QObject(parent)
{
    loadSettings();
}

void PrivacyLockService::loadSettings() {
    QSettings settings(s_settingsGroup, s_settingsGroup);
    m_pinHash = settings.value(s_keyPinHash, QString()).toString();
    m_salt = settings.value(s_keyPinSalt, QString()).toString();
    m_requireOnStartup = settings.value(s_keyRequireStartup, false).toBool();

    const QStringList bookList = settings.value(s_keyPrivateBooks, QStringList()).toStringList();
    m_privateBooks = QSet<QString>(bookList.begin(), bookList.end());

    if (isPinSet() && m_requireOnStartup) {
        m_locked = true;
    } else {
        m_locked = false;
    }
}

void PrivacyLockService::reloadSettings() {
    loadSettings();
}

bool PrivacyLockService::isPinSet() const noexcept {
    return !m_pinHash.isEmpty() && !m_salt.isEmpty();
}

bool PrivacyLockService::isLocked() const noexcept {
    return isPinSet() && m_locked;
}

bool PrivacyLockService::verifyPin(const QString& pin) const {
    if (!isPinSet() || pin.isEmpty()) {
        return false;
    }

    const QString computedHash = hashWithSalt(pin, m_salt);
    return constantTimeCompare(computedHash, m_pinHash);
}

bool PrivacyLockService::setPin(const QString& oldPin, const QString& newPin) {
    if (newPin.isEmpty()) {
        return false;
    }

    // If a PIN is currently set, oldPin must be verified
    if (isPinSet()) {
        if (!verifyPin(oldPin)) {
            return false;
        }
    }

    m_salt = generateSalt();
    m_pinHash = hashWithSalt(newPin, m_salt);

    QSettings settings(s_settingsGroup, s_settingsGroup);
    settings.setValue(s_keyPinHash, m_pinHash);
    settings.setValue(s_keyPinSalt, m_salt);

    const bool wasLocked = m_locked;
    m_locked = false;

    if (wasLocked) {
        emit lockStateChanged(false);
    }

    return true;
}

void PrivacyLockService::removePin(const QString& pin) {
    if (!isPinSet()) {
        return;
    }

    if (!verifyPin(pin)) {
        return;
    }

    m_pinHash.clear();
    m_salt.clear();
    m_requireOnStartup = false;

    QSettings settings(s_settingsGroup, s_settingsGroup);
    settings.remove(s_keyPinHash);
    settings.remove(s_keyPinSalt);
    settings.remove(s_keyRequireStartup);

    const bool wasLocked = m_locked;
    m_locked = false;

    if (wasLocked) {
        emit lockStateChanged(false);
    }
}

void PrivacyLockService::lock() {
    if (isPinSet() && !m_locked) {
        m_locked = true;
        emit lockStateChanged(true);
    }
}

bool PrivacyLockService::unlock(const QString& pin) {
    if (!isPinSet()) {
        if (m_locked) {
            m_locked = false;
            emit lockStateChanged(false);
        }
        return true;
    }

    if (verifyPin(pin)) {
        if (m_locked) {
            m_locked = false;
            emit lockStateChanged(false);
        }
        return true;
    }

    return false;
}

bool PrivacyLockService::requirePinOnStartup() const noexcept {
    return isPinSet() && m_requireOnStartup;
}

void PrivacyLockService::setRequirePinOnStartup(bool req) {
    if (m_requireOnStartup != req) {
        m_requireOnStartup = req;
        QSettings settings(s_settingsGroup, s_settingsGroup);
        settings.setValue(s_keyRequireStartup, req);
    }
}

bool PrivacyLockService::isBookPrivate(const QString& bookId) const {
    if (bookId.isEmpty()) {
        return false;
    }
    return m_privateBooks.contains(bookId);
}

void PrivacyLockService::setBookPrivate(const QString& bookId, bool isPrivate) {
    if (bookId.isEmpty()) {
        return;
    }

    bool changed = false;
    if (isPrivate) {
        if (!m_privateBooks.contains(bookId)) {
            m_privateBooks.insert(bookId);
            changed = true;
        }
    } else {
        if (m_privateBooks.remove(bookId)) {
            changed = true;
        }
    }

    if (changed) {
        QSettings settings(s_settingsGroup, s_settingsGroup);
        settings.setValue(s_keyPrivateBooks, QStringList(m_privateBooks.begin(), m_privateBooks.end()));
        emit bookPrivacyChanged(bookId, isPrivate);
    }
}

QList<QString> PrivacyLockService::privateBookIds() const {
    return m_privateBooks.values();
}

void PrivacyLockService::reset() {
    m_pinHash.clear();
    m_salt.clear();
    m_requireOnStartup = false;
    m_privateBooks.clear();

    QSettings settings(s_settingsGroup, s_settingsGroup);
    settings.remove(QStringLiteral("Privacy"));

    const bool wasLocked = m_locked;
    m_locked = false;

    if (wasLocked) {
        emit lockStateChanged(false);
    }
}

QString PrivacyLockService::generateSalt() {
    constexpr int saltByteCount = 16;
    QByteArray buffer(saltByteCount, Qt::Uninitialized);
    auto* rng = QRandomGenerator::global();
    rng->fillRange(reinterpret_cast<quint32*>(buffer.data()), saltByteCount / static_cast<int>(sizeof(quint32)));
    return QString::fromLatin1(buffer.toHex());
}

QString PrivacyLockService::hashWithSalt(const QString& pin, const QString& salt) {
    const QByteArray combined = salt.toUtf8() + ":" + pin.toUtf8();
    return HashUtils::calculateBytesSha256(combined);
}

bool PrivacyLockService::constantTimeCompare(const QString& a, const QString& b) noexcept {
    if (a.length() != b.length()) {
        return false;
    }

    int diff = 0;
    const qsizetype len = a.length();
    for (qsizetype i = 0; i < len; ++i) {
        diff |= (static_cast<int>(a[i].unicode()) ^ static_cast<int>(b[i].unicode()));
    }
    return diff == 0;
}

} // namespace LunaRead
