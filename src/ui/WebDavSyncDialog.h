#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QList>
#include <memory>
#include "../sync/WebDavSyncService.h"
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QCheckBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QGroupBox;
class QTimer;
class QAction;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief WebDAV Cloud Sync configuration and status dialog.
 *
 * Implements Moon+ Reader compatible cloud synchronization:
 * - WebDAV server URL, Username, and masked Password configuration
 * - Remote directory prefix selection (.moonreader or /LunaRead/)
 * - Automatic background synchronization timer (every 5 minutes)
 * - "Test Connection" connectivity validation
 * - "Sync Now" bidirectional synchronization of reading progress for library books
 * - Real-time activity log showing progress upload, conflict resolution, and errors
 * - Persistent configuration storage via QSettings
 */
class WebDavSyncDialog : public QDialog {
    Q_OBJECT

public:
    explicit WebDavSyncDialog(QWidget* parent = nullptr);
    explicit WebDavSyncDialog(WebDavSyncService* service, QWidget* parent = nullptr);
    explicit WebDavSyncDialog(WebDavSyncService& service, QWidget* parent = nullptr);
    ~WebDavSyncDialog() override;

    // --- Configuration accessors ---
    [[nodiscard]] QString serverUrl() const;
    void setServerUrl(const QString& url);

    [[nodiscard]] QString username() const;
    void setUsername(const QString& user);

    [[nodiscard]] QString password() const;
    void setPassword(const QString& pass);

    [[nodiscard]] QString remotePrefix() const;
    void setRemotePrefix(const QString& prefix);

    [[nodiscard]] bool isAutoSyncEnabled() const;
    void setAutoSyncEnabled(bool enabled);

    [[nodiscard]] QString statusText() const;
    void setStatusText(const QString& text);

    [[nodiscard]] int logCount() const;
    [[nodiscard]] QStringList logMessages() const;

    // --- Service integration ---
    void setSyncService(WebDavSyncService* service);
    [[nodiscard]] WebDavSyncService* syncService() const { return m_syncService; }

    // --- Visual styling ---
    void applyTheme(const ReaderThemeTokens& tokens);

    // --- Persistence (QSettings) ---
    void loadSettings();
    void saveSettings();

public slots:
    // --- Actions ---
    void testConnection();
    void syncNow();
    void clearLog();
    void togglePasswordVisibility();

signals:
    void syncStarted();
    void syncFinished(bool success, int syncedCount);
    void connectionTested(bool success, const QString& message);
    void logMessageAdded(const QString& message);

private slots:
    void onTestConnectionFinished(bool success, const QString& message);
    void onRemoteProgressReceived(const LunaRead::SyncPayload& payload);
    void onSyncConflictResolved(LunaRead::ConflictResolution resolution, const LunaRead::SyncPayload& winningPayload);
    void onSyncCompleted(bool success, const QString& message);
    void onAutoSyncTimerTimeout();
    void onAutoSyncToggled(bool checked);

private:
    void setupUi();
    void connectServiceSignals();
    void disconnectServiceSignals();
    void addLogEntry(const QString& message, const QString& level = QStringLiteral("INFO"));
    void processNextBookInQueue();
    void applySettingsToService();
    void updateStyles();

    // UI Controls
    QLineEdit* m_serverUrlEdit = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QLineEdit* m_remotePrefixEdit = nullptr;
    QAction* m_togglePasswordAction = nullptr;
    QCheckBox* m_autoSyncCheckBox = nullptr;

    QPushButton* m_testConnectionBtn = nullptr;
    QPushButton* m_syncNowBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QPushButton* m_clearLogBtn = nullptr;

    QLabel* m_headerTitleLabel = nullptr;
    QLabel* m_headerSubtitleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_lastSyncLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QListWidget* m_logList = nullptr;

    QGroupBox* m_credentialsGroup = nullptr;
    QGroupBox* m_activityGroup = nullptr;

    // Background Timer
    QTimer* m_autoSyncTimer = nullptr;

    // Service & Synchronization Queue
    WebDavSyncService* m_syncService = nullptr;
    bool m_ownsService = false;

    QList<BookItem> m_pendingBooks;
    int m_totalBooksToSync = 0;
    int m_syncedCount = 0;
    int m_errorCount = 0;
    bool m_isSyncing = false;
    bool m_passwordMasked = true;

    ReaderThemeTokens m_tokens = ReaderThemeTokens::modernLight();
};

} // namespace LunaRead
