#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <QStringList>

namespace LunaRead {

/**
 * @brief Manages PIN protection for the application and private books.
 *
 * Implements privacy protection corresponding to Moon+ Reader's Private Bookshelf
 * and App Lock features.
 *
 * Security features:
 * - Stores salted SHA-256 hash of PIN in QSettings; never stores plain text PIN.
 * - Uses cryptographically secure random salt (128-bit hex).
 * - Constant-time comparison to mitigate timing attacks.
 * - Tracks startup lock and per-book private status.
 */
class PrivacyLockService : public QObject {
    Q_OBJECT

public:
    static PrivacyLockService& instance();

    explicit PrivacyLockService(QObject* parent = nullptr);
    ~PrivacyLockService() override = default;

    // PIN status & authentication
    [[nodiscard]] bool isPinSet() const noexcept;
    [[nodiscard]] bool isLocked() const noexcept;
    [[nodiscard]] bool verifyPin(const QString& pin) const;
    bool setPin(const QString& oldPin, const QString& newPin);
    void removePin(const QString& pin);
    void lock();
    bool unlock(const QString& pin);

    // Startup lock configuration
    [[nodiscard]] bool requirePinOnStartup() const noexcept;
    void setRequirePinOnStartup(bool req);

    // Private bookshelf / books
    [[nodiscard]] bool isBookPrivate(const QString& bookId) const;
    void setBookPrivate(const QString& bookId, bool isPrivate);
    [[nodiscard]] QList<QString> privateBookIds() const;

    // Reset settings (useful for tests or full data wipe)
    void reset();
    void reloadSettings();

signals:
    void lockStateChanged(bool locked);
    void bookPrivacyChanged(const QString& bookId, bool isPrivate);

private:
    void loadSettings();
    static QString generateSalt();
    static QString hashWithSalt(const QString& pin, const QString& salt);
    static bool constantTimeCompare(const QString& a, const QString& b) noexcept;

    bool m_locked = false;
    QString m_pinHash;
    QString m_salt;
    bool m_requireOnStartup = false;
    QSet<QString> m_privateBooks;
};

} // namespace LunaRead
