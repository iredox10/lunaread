#include "SystemTrayManager.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMessageBox>
#include "../db/DatabaseManager.h"

namespace LunaRead {

SystemTrayManager::SystemTrayManager(QWidget* parentWindow, QObject* parent)
    : QObject(parent), m_mainWindow(parentWindow) {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(createMoonPlusBookIcon());
    m_trayIcon->setToolTip(tr("LunaRead - Desktop Reader"));

    m_contextMenu = new QMenu();

    m_showHideAction = m_contextMenu->addAction(tr("Show LunaRead"), this, &SystemTrayManager::toggleMainWindow);
    m_contextMenu->addSeparator();

    m_recentBooksMenu = m_contextMenu->addMenu(tr("Recent Books"));
    updateRecentBooksMenu();

    m_statisticsAction = m_contextMenu->addAction(tr("Reading Statistics..."), this, &SystemTrayManager::showReadingStatisticsDialog);
    m_settingsAction = m_contextMenu->addAction(tr("Preferences..."), this, &SystemTrayManager::showSettingsDialog);
    m_contextMenu->addSeparator();

    m_quitAction = m_contextMenu->addAction(tr("Quit"), this, &SystemTrayManager::quitApplication);

    m_trayIcon->setContextMenu(m_contextMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayManager::onTrayIconActivated);
    connect(m_contextMenu, &QMenu::aboutToShow, this, &SystemTrayManager::onMenuAboutToShow);
}

void SystemTrayManager::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        toggleMainWindow();
    }
}

void SystemTrayManager::onMenuAboutToShow() {
    updateRecentBooksMenu();
}

SystemTrayManager::~SystemTrayManager() {
    delete m_contextMenu;
}

QIcon SystemTrayManager::createMoonPlusBookIcon() {
    constexpr int size = 64;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Deep blue night sky background disc
    QPainterPath disc;
    disc.addEllipse(2, 2, size - 4, size - 4);
    QLinearGradient discGrad(0, 0, size, size);
    discGrad.setColorAt(0.0, QColor(30, 58, 138));
    discGrad.setColorAt(1.0, QColor(15, 23, 42));
    p.fillPath(disc, discGrad);

    // Golden Crescent Moon
    QPainterPath moonOuter;
    moonOuter.addEllipse(12, 10, 40, 40);
    QPainterPath moonCut;
    moonCut.addEllipse(22, 8, 36, 36);
    QPainterPath crescent = moonOuter.subtracted(moonCut);
    p.fillPath(crescent, QColor(251, 191, 36));

    // Open book pages at the bottom
    QPainterPath bookLeft;
    bookLeft.moveTo(18, 44);
    bookLeft.cubicTo(24, 42, 28, 42, 32, 45);
    bookLeft.lineTo(32, 54);
    bookLeft.cubicTo(28, 51, 24, 51, 18, 53);
    bookLeft.closeSubpath();
    p.fillPath(bookLeft, Qt::white);

    QPainterPath bookRight;
    bookRight.moveTo(32, 45);
    bookRight.cubicTo(36, 42, 40, 42, 46, 44);
    bookRight.lineTo(46, 53);
    bookRight.cubicTo(40, 51, 36, 51, 32, 54);
    bookRight.closeSubpath();
    p.fillPath(bookRight, QColor(241, 245, 249));

    return QIcon(pix);
}

void SystemTrayManager::show() {
    m_trayIcon->show();
}

void SystemTrayManager::hide() {
    m_trayIcon->hide();
}

bool SystemTrayManager::isVisible() const {
    return m_trayIcon->isVisible();
}

void SystemTrayManager::setMainWindow(QWidget* window) {
    m_mainWindow = window;
}

void SystemTrayManager::attachSyncService(WebDavSyncService* service) {
    if (!service) return;
    connect(service, &WebDavSyncService::remoteProgressReceived, this, &SystemTrayManager::onRemoteProgressReceived);
    connect(service, &WebDavSyncService::syncCompleted, this, &SystemTrayManager::onSyncCompleted);
}

void SystemTrayManager::toggleMainWindow() {
    if (!m_mainWindow) return;
    if (m_mainWindow->isVisible() && !m_mainWindow->isMinimized()) {
        m_mainWindow->hide();
        m_showHideAction->setText(tr("Show LunaRead"));
        emit hideMainWindowRequested();
    } else {
        m_mainWindow->showNormal();
        m_mainWindow->activateWindow();
        m_mainWindow->raise();
        m_showHideAction->setText(tr("Hide LunaRead"));
        emit showMainWindowRequested();
    }
}

void SystemTrayManager::updateRecentBooksMenu() {
    m_recentBooksMenu->clear();
    auto books = DatabaseManager::instance().getBooks();
    if (books.isEmpty()) {
        QAction* noBooks = m_recentBooksMenu->addAction(tr("No recent books"));
        noBooks->setEnabled(false);
        return;
    }

    int count = std::min(static_cast<int>(books.size()), 5);
    for (int i = 0; i < count; ++i) {
        const auto& b = books[i];
        QString bookId = b.id;
        QString label = QStringLiteral("%1 (%2%)").arg(b.title).arg(static_cast<int>(b.progressPercentage));
        m_recentBooksMenu->addAction(label, this, [this, bookId]() {
            emit openBookRequested(bookId);
            if (m_mainWindow) {
                m_mainWindow->showNormal();
                m_mainWindow->activateWindow();
            }
        });
    }
}

void SystemTrayManager::showReadingStatisticsDialog() {
    emit showStatisticsRequested();
    if (m_mainWindow) {
        m_mainWindow->showNormal();
        m_mainWindow->activateWindow();
    }
}

void SystemTrayManager::showSettingsDialog() {
    emit showSettingsRequested();
}

void SystemTrayManager::showNotification(const QString& title, const QString& message,
                                         QSystemTrayIcon::MessageIcon icon, int timeoutMs) {
    m_trayIcon->showMessage(title, message, icon, timeoutMs);
}

void SystemTrayManager::showSyncNotification(const SyncPayload& payload) {
    QString msg = QStringLiteral("Progress updated: %1% on %2").arg(static_cast<int>(payload.percentage)).arg(payload.deviceName);
    showNotification(tr("LunaRead Sync"), msg, QSystemTrayIcon::Information);
    emit syncNotificationShown(msg);
}

void SystemTrayManager::showSyncNotification(int percentage, const QString& deviceName) {
    QString msg = QStringLiteral("Progress updated: %1% on %2").arg(percentage).arg(deviceName);
    showNotification(tr("LunaRead Sync"), msg, QSystemTrayIcon::Information);
    emit syncNotificationShown(msg);
}

void SystemTrayManager::showSyncNotification(const QString& message, const QString& title) {
    showNotification(title, message, QSystemTrayIcon::Information);
    emit syncNotificationShown(message);
}

void SystemTrayManager::onRemoteProgressReceived(const SyncPayload& payload) {
    showSyncNotification(payload);
    updateRecentBooksMenu();
}

void SystemTrayManager::onSyncCompleted(bool success, const QString& message) {
    if (success) {
        showNotification(tr("Sync Successful"), message, QSystemTrayIcon::Information, 3000);
    } else {
        showNotification(tr("Sync Failed"), message, QSystemTrayIcon::Warning, 5000);
    }
}

void SystemTrayManager::quitApplication() {
    emit quitRequested();
    QApplication::quit();
}

} // namespace LunaRead
