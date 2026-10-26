#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QString>
#include <QList>
#include <QWidget>
#include "../core/Types.h"
#include "../sync/WebDavSyncService.h"

namespace LunaRead {

/**
 * @brief Manages the system tray icon, context menu, and desktop notifications for LunaRead.
 * 
 * Provides an authentic Moon+ Reader desktop tray presence:
 * - System tray icon using QSystemTrayIcon with Moon+ book icon.
 * - Context menu: Show/Hide LunaRead, Open Recent Book, Reading Statistics, Settings, Quit.
 * - Notifications for sync events ("Remote progress updated: 42% on Linux-Desktop").
 */
class SystemTrayManager : public QObject {
    Q_OBJECT

public:
    explicit SystemTrayManager(QWidget* parentWindow = nullptr, QObject* parent = nullptr);
    ~SystemTrayManager() override;

    /// Generates the signature Moon+ Reader desktop icon (crescent moon and open book).
    static QIcon createMoonPlusBookIcon();

    /// Shows the system tray icon.
    void show();

    /// Hides the system tray icon.
    void hide();

    /// Returns true if the tray icon is currently visible.
    bool isVisible() const;

    /// Associates or updates the main application window.
    void setMainWindow(QWidget* window);
    QWidget* mainWindow() const { return m_mainWindow; }

    /// Direct access to the underlying QSystemTrayIcon.
    QSystemTrayIcon* trayIcon() const { return m_trayIcon; }

    /// Context menu and actions access.
    QMenu* contextMenu() const { return m_contextMenu; }
    QAction* showHideAction() const { return m_showHideAction; }
    QMenu* recentBooksMenu() const { return m_recentBooksMenu; }
    QAction* statisticsAction() const { return m_statisticsAction; }
    QAction* settingsAction() const { return m_settingsAction; }
    QAction* quitAction() const { return m_quitAction; }

    /// Connects to a WebDavSyncService to automatically show sync notifications.
    void attachSyncService(WebDavSyncService* service);

public slots:
    /// Toggles the main window between hidden and visible/active.
    void toggleMainWindow();

    /// Rebuilds / updates the recent books submenu in the tray context menu.
    void updateRecentBooksMenu();

    /// Displays the Reading Statistics dialog.
    void showReadingStatisticsDialog();

    /// Displays the Settings dialog.
    void showSettingsDialog();

    /// Displays a generic system tray desktop notification.
    void showNotification(const QString& title, const QString& message,
                          QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                          int timeoutMs = 5000);

    /// Displays a sync event notification with formatted progress.
    void showSyncNotification(const SyncPayload& payload);
    void showSyncNotification(int percentage, const QString& deviceName);
    void showSyncNotification(const QString& message, const QString& title = "LunaRead Sync");

    /// Slot handling remote sync progress updates.
    void onRemoteProgressReceived(const SyncPayload& payload);

    /// Slot handling sync completion.
    void onSyncCompleted(bool success, const QString& message);

    /// Quits the desktop application cleanly.
    void quitApplication();

signals:
    void showMainWindowRequested();
    void hideMainWindowRequested();
    void openBookRequested(const QString& bookId);
    void showStatisticsRequested();
    void showSettingsRequested();
    void quitRequested();
    void syncNotificationShown(const QString& message);

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onMenuAboutToShow();

private:
    void setupTrayIcon();
    void setupContextMenu();

    QWidget* m_mainWindow = nullptr;
    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_contextMenu = nullptr;

    QAction* m_showHideAction = nullptr;
    QMenu* m_recentBooksMenu = nullptr;
    QAction* m_statisticsAction = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_quitAction = nullptr;
};

} // namespace LunaRead
