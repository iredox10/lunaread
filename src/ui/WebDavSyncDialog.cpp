#include "WebDavSyncDialog.h"
#include "../db/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QGroupBox>
#include <QTimer>
#include <QSettings>
#include <QDateTime>
#include <QAction>
#include <QIcon>
#include <QStyle>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QCursor>

namespace LunaRead {

WebDavSyncDialog::WebDavSyncDialog(QWidget* parent)
    : QDialog(parent)
    , m_autoSyncTimer(new QTimer(this))
    , m_syncService(new WebDavSyncService(this))
    , m_ownsService(false) {
    setupUi();
    connectServiceSignals();
    loadSettings();
}

WebDavSyncDialog::WebDavSyncDialog(WebDavSyncService* service, QWidget* parent)
    : QDialog(parent)
    , m_autoSyncTimer(new QTimer(this))
    , m_syncService(service ? service : new WebDavSyncService(this))
    , m_ownsService(false) {
    setupUi();
    connectServiceSignals();
    loadSettings();
}

WebDavSyncDialog::WebDavSyncDialog(WebDavSyncService& service, QWidget* parent)
    : WebDavSyncDialog(&service, parent) {
}

WebDavSyncDialog::~WebDavSyncDialog() {
    if (m_autoSyncTimer && m_autoSyncTimer->isActive()) {
        m_autoSyncTimer->stop();
    }
    disconnectServiceSignals();
    if (m_ownsService && m_syncService) {
        delete m_syncService;
        m_syncService = nullptr;
    }
}

void WebDavSyncDialog::setupUi() {
    setWindowTitle(tr("WebDAV Cloud Sync - LunaRead"));
    resize(620, 640);
    setMinimumSize(540, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(14);

    // Header banner
    auto* headerLayout = new QVBoxLayout();
    headerLayout->setSpacing(4);

    m_headerTitleLabel = new QLabel(tr("WebDAV Cloud Sync"), this);
    QFont titleFont = m_headerTitleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_headerTitleLabel->setFont(titleFont);

    m_headerSubtitleLabel = new QLabel(
        tr("Synchronize reading positions, chapters, and progress across your devices (Moon+ Reader compatible)."), this);
    m_headerSubtitleLabel->setWordWrap(true);
    m_headerSubtitleLabel->setStyleSheet(QStringLiteral("color: #64748B; font-size: 10pt;"));

    headerLayout->addWidget(m_headerTitleLabel);
    headerLayout->addWidget(m_headerSubtitleLabel);
    mainLayout->addLayout(headerLayout);

    // Group 1: Server Settings
    m_credentialsGroup = new QGroupBox(tr("Server Configuration"), this);
    auto* formLayout = new QFormLayout(m_credentialsGroup);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setContentsMargins(14, 16, 14, 14);
    formLayout->setSpacing(10);

    m_serverUrlEdit = new QLineEdit(this);
    m_serverUrlEdit->setPlaceholderText(QStringLiteral("https://dav.example.com/remote.php/webdav/"));
    m_serverUrlEdit->setClearButtonEnabled(true);
    formLayout->addRow(tr("Server URL:"), m_serverUrlEdit);

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(tr("WebDAV username"));
    m_usernameEdit->setClearButtonEnabled(true);
    formLayout->addRow(tr("Username:"), m_usernameEdit);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("WebDAV password or app token"));
    m_togglePasswordAction = m_passwordEdit->addAction(
        style()->standardIcon(QStyle::SP_FileDialogContentsView),
        QLineEdit::TrailingPosition);
    m_togglePasswordAction->setToolTip(tr("Toggle password visibility"));
    connect(m_togglePasswordAction, &QAction::triggered, this, &WebDavSyncDialog::togglePasswordVisibility);
    formLayout->addRow(tr("Password:"), m_passwordEdit);

    m_remotePrefixEdit = new QLineEdit(this);
    m_remotePrefixEdit->setText(QStringLiteral(".moonreader"));
    m_remotePrefixEdit->setPlaceholderText(QStringLiteral(".moonreader or /LunaRead/"));
    m_remotePrefixEdit->setToolTip(tr("Remote directory prefix for sync files (.po). Defaults to .moonreader."));
    formLayout->addRow(tr("Remote Directory:"), m_remotePrefixEdit);

    m_autoSyncCheckBox = new QCheckBox(tr("Enable automatic background sync every 5 minutes"), this);
    connect(m_autoSyncCheckBox, &QCheckBox::toggled, this, &WebDavSyncDialog::onAutoSyncToggled);
    formLayout->addRow(QString(), m_autoSyncCheckBox);

    mainLayout->addWidget(m_credentialsGroup);

    // Action Buttons Row (Test Connection, Sync Now)
    auto* actionButtonLayout = new QHBoxLayout();
    actionButtonLayout->setSpacing(10);

    m_testConnectionBtn = new QPushButton(tr("Test Connection"), this);
    m_testConnectionBtn->setCursor(Qt::PointingHandCursor);
    m_testConnectionBtn->setMinimumHeight(34);
    connect(m_testConnectionBtn, &QPushButton::clicked, this, &WebDavSyncDialog::testConnection);

    m_syncNowBtn = new QPushButton(tr("Sync Now"), this);
    m_syncNowBtn->setCursor(Qt::PointingHandCursor);
    m_syncNowBtn->setMinimumHeight(34);
    QFont syncBtnFont = m_syncNowBtn->font();
    syncBtnFont.setBold(true);
    m_syncNowBtn->setFont(syncBtnFont);
    m_syncNowBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2563EB; color: white; border-radius: 6px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #1D4ED8; }"
        "QPushButton:pressed { background-color: #1E40AF; }"
        "QPushButton:disabled { background-color: #93C5FD; color: #F1F5F9; }"
    ));
    connect(m_syncNowBtn, &QPushButton::clicked, this, &WebDavSyncDialog::syncNow);

    actionButtonLayout->addWidget(m_testConnectionBtn);
    actionButtonLayout->addWidget(m_syncNowBtn);
    actionButtonLayout->addStretch(1);
    mainLayout->addLayout(actionButtonLayout);

    // Group 2: Status & Activity Log
    m_activityGroup = new QGroupBox(tr("Status & Activity Log"), this);
    auto* activityLayout = new QVBoxLayout(m_activityGroup);
    activityLayout->setContentsMargins(14, 14, 14, 14);
    activityLayout->setSpacing(8);

    auto* statusRow = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Status: Ready"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #475569; font-weight: 500;"));
    m_lastSyncLabel = new QLabel(tr("Last sync: Never"), this);
    m_lastSyncLabel->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 9pt;"));

    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(m_lastSyncLabel, 0, Qt::AlignRight);
    activityLayout->addLayout(statusRow);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(14);
    activityLayout->addWidget(m_progressBar);

    m_logList = new QListWidget(this);
    m_logList->setAlternatingRowColors(true);
    m_logList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_logList->setContextMenuPolicy(Qt::CustomContextMenu);
    QFont monoFont = m_logList->font();
    monoFont.setPointSize(9);
    m_logList->setFont(monoFont);
    connect(m_logList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        auto* copyAction = menu.addAction(tr("Copy Selected"));
        auto* clearAction = menu.addAction(tr("Clear Log"));
        QAction* selected = menu.exec(m_logList->viewport()->mapToGlobal(pos));
        if (selected == copyAction) {
            QStringList selectedTexts;
            for (auto* item : m_logList->selectedItems()) {
                selectedTexts << item->text();
            }
            if (!selectedTexts.isEmpty()) {
                QApplication::clipboard()->setText(selectedTexts.join(QLatin1Char('\n')));
            }
        } else if (selected == clearAction) {
            clearLog();
        }
    });
    activityLayout->addWidget(m_logList, 1);

    auto* logToolsLayout = new QHBoxLayout();
    m_clearLogBtn = new QPushButton(tr("Clear Log"), this);
    m_clearLogBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearLogBtn, &QPushButton::clicked, this, &WebDavSyncDialog::clearLog);
    logToolsLayout->addStretch(1);
    logToolsLayout->addWidget(m_clearLogBtn);
    activityLayout->addLayout(logToolsLayout);

    mainLayout->addWidget(m_activityGroup, 1);

    // Dialog bottom buttons (Save Settings, Close)
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    m_saveBtn = new QPushButton(tr("Save Settings"), this);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        m_statusLabel->setText(tr("Settings saved successfully"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #16A34A; font-weight: 500;"));
        addLogEntry(tr("Configuration saved."), QStringLiteral("SUCCESS"));
    });

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    bottomLayout->addStretch(1);
    bottomLayout->addWidget(m_saveBtn);
    bottomLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(bottomLayout);

    // Timer setup
    connect(m_autoSyncTimer, &QTimer::timeout, this, &WebDavSyncDialog::onAutoSyncTimerTimeout);

    updateStyles();
}

void WebDavSyncDialog::connectServiceSignals() {
    if (!m_syncService) return;
    connect(m_syncService, &WebDavSyncService::remoteProgressReceived,
            this, &WebDavSyncDialog::onRemoteProgressReceived);
    connect(m_syncService, &WebDavSyncService::syncConflictResolved,
            this, &WebDavSyncDialog::onSyncConflictResolved);
    connect(m_syncService, &WebDavSyncService::syncCompleted,
            this, &WebDavSyncDialog::onSyncCompleted);
    connect(m_syncService, &WebDavSyncService::connectionTested,
            this, &WebDavSyncDialog::onTestConnectionFinished);
}

void WebDavSyncDialog::disconnectServiceSignals() {
    if (!m_syncService) return;
    disconnect(m_syncService, &WebDavSyncService::remoteProgressReceived,
               this, &WebDavSyncDialog::onRemoteProgressReceived);
    disconnect(m_syncService, &WebDavSyncService::syncConflictResolved,
               this, &WebDavSyncDialog::onSyncConflictResolved);
    disconnect(m_syncService, &WebDavSyncService::syncCompleted,
               this, &WebDavSyncDialog::onSyncCompleted);
    disconnect(m_syncService, &WebDavSyncService::connectionTested,
               this, &WebDavSyncDialog::onTestConnectionFinished);
}

void WebDavSyncDialog::setSyncService(WebDavSyncService* service) {
    if (m_syncService == service) return;
    disconnectServiceSignals();
    if (m_ownsService && m_syncService) {
        delete m_syncService;
    }
    m_syncService = service;
    m_ownsService = false;
    connectServiceSignals();
    applySettingsToService();
}

QString WebDavSyncDialog::serverUrl() const {
    return m_serverUrlEdit ? m_serverUrlEdit->text().trimmed() : QString();
}

void WebDavSyncDialog::setServerUrl(const QString& url) {
    if (m_serverUrlEdit) {
        m_serverUrlEdit->setText(url.trimmed());
    }
}

QString WebDavSyncDialog::username() const {
    return m_usernameEdit ? m_usernameEdit->text().trimmed() : QString();
}

void WebDavSyncDialog::setUsername(const QString& user) {
    if (m_usernameEdit) {
        m_usernameEdit->setText(user.trimmed());
    }
}

QString WebDavSyncDialog::password() const {
    return m_passwordEdit ? m_passwordEdit->text() : QString();
}

void WebDavSyncDialog::setPassword(const QString& pass) {
    if (m_passwordEdit) {
        m_passwordEdit->setText(pass);
    }
}

QString WebDavSyncDialog::remotePrefix() const {
    QString p = m_remotePrefixEdit ? m_remotePrefixEdit->text().trimmed() : QString();
    return p.isEmpty() ? QStringLiteral(".moonreader") : p;
}

void WebDavSyncDialog::setRemotePrefix(const QString& prefix) {
    if (m_remotePrefixEdit) {
        m_remotePrefixEdit->setText(prefix.trimmed());
    }
}

bool WebDavSyncDialog::isAutoSyncEnabled() const {
    return m_autoSyncCheckBox && m_autoSyncCheckBox->isChecked();
}

void WebDavSyncDialog::setAutoSyncEnabled(bool enabled) {
    if (m_autoSyncCheckBox) {
        m_autoSyncCheckBox->setChecked(enabled);
    }
}

QString WebDavSyncDialog::statusText() const {
    return m_statusLabel ? m_statusLabel->text() : QString();
}

void WebDavSyncDialog::setStatusText(const QString& text) {
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}

int WebDavSyncDialog::logCount() const {
    return m_logList ? m_logList->count() : 0;
}

QStringList WebDavSyncDialog::logMessages() const {
    QStringList list;
    if (!m_logList) return list;
    for (int i = 0; i < m_logList->count(); ++i) {
        list << m_logList->item(i)->text();
    }
    return list;
}

void WebDavSyncDialog::clearLog() {
    if (m_logList) {
        m_logList->clear();
    }
}

void WebDavSyncDialog::togglePasswordVisibility() {
    m_passwordMasked = !m_passwordMasked;
    if (m_passwordMasked) {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
    } else {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
    }
}

void WebDavSyncDialog::applySettingsToService() {
    if (m_syncService) {
        m_syncService->setCredentials(serverUrl(), username(), password());
        m_syncService->setRemotePrefix(remotePrefix());
    }
}

void WebDavSyncDialog::loadSettings() {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    settings.beginGroup(QStringLiteral("WebDav"));

    QString url = settings.value(QStringLiteral("serverUrl"), QString()).toString();
    QString user = settings.value(QStringLiteral("username"), QString()).toString();
    QByteArray encodedPass = settings.value(QStringLiteral("password"), QByteArray()).toByteArray();
    QString pass;
    if (!encodedPass.isEmpty()) {
        pass = QString::fromUtf8(QByteArray::fromBase64(encodedPass));
    }
    QString prefix = settings.value(QStringLiteral("remotePrefix"), QStringLiteral(".moonreader")).toString();
    if (prefix.trimmed().isEmpty()) {
        prefix = QStringLiteral(".moonreader");
    }
    bool autoSync = settings.value(QStringLiteral("autoSync"), false).toBool();
    qint64 lastSync = settings.value(QStringLiteral("lastSyncTime"), 0).toLongLong();

    settings.endGroup();

    if (m_serverUrlEdit) m_serverUrlEdit->setText(url);
    if (m_usernameEdit) m_usernameEdit->setText(user);
    if (m_passwordEdit) m_passwordEdit->setText(pass);
    if (m_remotePrefixEdit) m_remotePrefixEdit->setText(prefix);
    if (m_autoSyncCheckBox) m_autoSyncCheckBox->setChecked(autoSync);

    if (lastSync > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(lastSync);
        if (m_lastSyncLabel) {
            m_lastSyncLabel->setText(tr("Last sync: %1").arg(dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        }
    } else if (m_lastSyncLabel) {
        m_lastSyncLabel->setText(tr("Last sync: Never"));
    }

    applySettingsToService();

    if (autoSync && !m_autoSyncTimer->isActive()) {
        m_autoSyncTimer->start(5 * 60 * 1000); // 5 minutes
    }
}

void WebDavSyncDialog::saveSettings() {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    settings.beginGroup(QStringLiteral("WebDav"));

    settings.setValue(QStringLiteral("serverUrl"), serverUrl());
    settings.setValue(QStringLiteral("username"), username());
    settings.setValue(QStringLiteral("password"), password().toUtf8().toBase64());
    settings.setValue(QStringLiteral("remotePrefix"), remotePrefix());
    settings.setValue(QStringLiteral("autoSync"), isAutoSyncEnabled());

    settings.endGroup();

    applySettingsToService();
}

void WebDavSyncDialog::testConnection() {
    applySettingsToService();

    if (serverUrl().isEmpty()) {
        m_statusLabel->setText(tr("Server URL is empty"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #DC2626; font-weight: 500;"));
        addLogEntry(tr("Connection test aborted: Server URL is empty."), QStringLiteral("ERROR"));
        return;
    }

    m_testConnectionBtn->setEnabled(false);
    m_syncNowBtn->setEnabled(false);
    m_statusLabel->setText(tr("Testing connection to WebDAV server..."));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #2563EB; font-weight: 500;"));
    addLogEntry(tr("Testing connection to %1...").arg(serverUrl()), QStringLiteral("INFO"));

    m_syncService->testConnection([this](bool success, const QString& message) {
        onTestConnectionFinished(success, message);
    });
}

void WebDavSyncDialog::onTestConnectionFinished(bool success, const QString& message) {
    m_testConnectionBtn->setEnabled(true);
    m_syncNowBtn->setEnabled(true);

    if (success) {
        m_statusLabel->setText(tr("Connection successful"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #16A34A; font-weight: bold;"));
        addLogEntry(tr("Connection test succeeded: %1").arg(message), QStringLiteral("SUCCESS"));
    } else {
        m_statusLabel->setText(tr("Connection failed"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #DC2626; font-weight: bold;"));
        addLogEntry(tr("Connection test failed: %1").arg(message), QStringLiteral("ERROR"));
    }

    emit connectionTested(success, message);
}

void WebDavSyncDialog::syncNow() {
    if (m_isSyncing) return;

    if (serverUrl().isEmpty()) {
        m_statusLabel->setText(tr("Server URL is empty"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #DC2626; font-weight: 500;"));
        addLogEntry(tr("Sync aborted: Please configure WebDAV server URL first."), QStringLiteral("ERROR"));
        return;
    }

    saveSettings();

    auto books = DatabaseManager::instance().getAllBooks();
    if (books.isEmpty()) {
        m_statusLabel->setText(tr("No books in library to sync"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #64748B; font-weight: 500;"));
        addLogEntry(tr("Library contains 0 books. Import books to start syncing reading progress."), QStringLiteral("INFO"));
        emit syncFinished(true, 0);
        return;
    }

    m_isSyncing = true;
    m_pendingBooks = books;
    m_totalBooksToSync = static_cast<int>(books.size());
    m_syncedCount = 0;
    m_errorCount = 0;

    m_testConnectionBtn->setEnabled(false);
    m_syncNowBtn->setEnabled(false);

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, m_totalBooksToSync);
    m_progressBar->setValue(0);

    m_statusLabel->setText(tr("Starting sync (0/%1 books)...").arg(m_totalBooksToSync));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #2563EB; font-weight: bold;"));
    addLogEntry(tr("Starting bidirectional sync for %1 book(s)...").arg(m_totalBooksToSync), QStringLiteral("INFO"));

    emit syncStarted();
    processNextBookInQueue();
}

void WebDavSyncDialog::processNextBookInQueue() {
    if (!m_isSyncing) return;

    if (m_pendingBooks.isEmpty()) {
        // Complete sync process
        m_isSyncing = false;
        m_progressBar->setValue(m_totalBooksToSync);
        m_progressBar->setVisible(false);
        m_testConnectionBtn->setEnabled(true);
        m_syncNowBtn->setEnabled(true);

        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
        settings.setValue(QStringLiteral("WebDav/lastSyncTime"), nowMs);

        QDateTime now = QDateTime::fromMSecsSinceEpoch(nowMs);
        m_lastSyncLabel->setText(tr("Last sync: %1").arg(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

        if (m_errorCount == 0) {
            m_statusLabel->setText(tr("Sync completed successfully (%1 books)").arg(m_syncedCount));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #16A34A; font-weight: bold;"));
            addLogEntry(tr("All %1 book(s) synchronized successfully.").arg(m_syncedCount), QStringLiteral("SUCCESS"));
            emit syncFinished(true, m_syncedCount);
        } else {
            m_statusLabel->setText(tr("Sync finished with %1 error(s)").arg(m_errorCount));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #D97706; font-weight: bold;"));
            addLogEntry(tr("Sync completed with %1 error(s) out of %2 book(s).")
                            .arg(m_errorCount).arg(m_totalBooksToSync), QStringLiteral("WARNING"));
            emit syncFinished(false, m_syncedCount);
        }
        return;
    }

    BookItem book = m_pendingBooks.takeFirst();
    int currentNum = m_syncedCount + m_errorCount + 1;
    m_progressBar->setValue(m_syncedCount + m_errorCount);
    m_statusLabel->setText(tr("Syncing [%1/%2]: %3").arg(currentNum).arg(m_totalBooksToSync).arg(book.title));

    SyncPayload localPayload;
    localPayload.bookHash = book.fileHash.isEmpty() ? book.id : book.fileHash;
    localPayload.title = book.title;
    localPayload.chapterIndex = book.currentChapterIndex;
    localPayload.pageIndex = book.currentPageIndex;
    localPayload.characterOffset = 0;
    localPayload.progressPercentage = book.progressPercentage;
    localPayload.percentage = book.progressPercentage;
    localPayload.timestamp = book.lastReadTime > 0 ? book.lastReadTime : QDateTime::currentMSecsSinceEpoch();
    localPayload.deviceName = QStringLiteral("LunaRead Desktop");

    m_syncService->syncBook(localPayload);
}

void WebDavSyncDialog::onSyncCompleted(bool success, const QString& message) {
    if (!m_isSyncing) return;

    if (success) {
        m_syncedCount++;
    } else {
        m_errorCount++;
        addLogEntry(tr("Sync item error: %1").arg(message), QStringLiteral("ERROR"));
    }

    // Process next item asynchronously allowing UI events to process
    QTimer::singleShot(25, this, &WebDavSyncDialog::processNextBookInQueue);
}

void WebDavSyncDialog::onRemoteProgressReceived(const SyncPayload& payload) {
    auto optBook = DatabaseManager::instance().getBookByHash(payload.bookHash);
    if (!optBook.has_value()) {
        optBook = DatabaseManager::instance().getBookById(payload.bookHash);
    }

    QString bookName = payload.title.isEmpty() ? payload.bookHash : payload.title;
    if (optBook.has_value()) {
        DatabaseManager::instance().updateBookProgress(
            optBook->id, payload.chapterIndex, payload.pageIndex, payload.percentage);
        bookName = optBook->title;
    }

    addLogEntry(tr("Applied remote progress for \"%1\": Ch %2, Pg %3 (%4%) [Device: %5]")
                    .arg(bookName)
                    .arg(payload.chapterIndex + 1)
                    .arg(payload.pageIndex + 1)
                    .arg(payload.percentage, 0, 'f', 1)
                    .arg(payload.deviceName.isEmpty() ? tr("Unknown") : payload.deviceName),
                QStringLiteral("SYNC"));
}

void WebDavSyncDialog::onSyncConflictResolved(ConflictResolution resolution, const SyncPayload& winningPayload) {
    QString bookName = winningPayload.title.isEmpty() ? winningPayload.bookHash : winningPayload.title;

    switch (resolution) {
    case ConflictResolution::UseRemote:
        addLogEntry(tr("Conflict resolved: Remote timestamp is newer for \"%1\" (applied %2%)")
                        .arg(bookName)
                        .arg(winningPayload.percentage, 0, 'f', 1),
                    QStringLiteral("CONFLICT"));
        break;
    case ConflictResolution::UseLocal:
        addLogEntry(tr("Conflict resolved: Local progress is newer for \"%1\" (uploading %2%)")
                        .arg(bookName)
                        .arg(winningPayload.percentage, 0, 'f', 1),
                    QStringLiteral("CONFLICT"));
        break;
    case ConflictResolution::InSync:
        addLogEntry(tr("\"%1\" is already in sync (%2%)")
                        .arg(bookName)
                        .arg(winningPayload.percentage, 0, 'f', 1),
                    QStringLiteral("INFO"));
        break;
    }
}

void WebDavSyncDialog::onAutoSyncTimerTimeout() {
    if (!m_isSyncing && !serverUrl().isEmpty()) {
        addLogEntry(tr("Running scheduled 5-minute background sync..."), QStringLiteral("INFO"));
        syncNow();
    }
}

void WebDavSyncDialog::onAutoSyncToggled(bool checked) {
    if (checked) {
        if (!m_autoSyncTimer->isActive()) {
            m_autoSyncTimer->start(5 * 60 * 1000); // 5 minutes
        }
        addLogEntry(tr("Automatic sync enabled (every 5 minutes)."), QStringLiteral("INFO"));
    } else {
        m_autoSyncTimer->stop();
        addLogEntry(tr("Automatic sync disabled."), QStringLiteral("INFO"));
    }
    saveSettings();
}

void WebDavSyncDialog::addLogEntry(const QString& message, const QString& level) {
    if (!m_logList) return;

    QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    QString formatted = QStringLiteral("[%1] %2").arg(timeStr, message);

    auto* item = new QListWidgetItem(formatted, m_logList);
    if (level == QStringLiteral("ERROR")) {
        item->setForeground(QColor(220, 38, 38)); // Red #DC2626
    } else if (level == QStringLiteral("SUCCESS")) {
        item->setForeground(QColor(22, 163, 74)); // Green #16A34A
    } else if (level == QStringLiteral("WARNING") || level == QStringLiteral("CONFLICT")) {
        item->setForeground(QColor(217, 119, 6)); // Amber #D97706
    } else if (level == QStringLiteral("SYNC")) {
        item->setForeground(QColor(37, 99, 235)); // Blue #2563EB
    } else {
        item->setForeground(QColor(100, 116, 139)); // Slate #64748B
    }

    m_logList->addItem(item);
    m_logList->scrollToBottom();

    emit logMessageAdded(formatted);
}

void WebDavSyncDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    updateStyles();
}

void WebDavSyncDialog::updateStyles() {
    // Subtle styling matching LunaRead theme palette
    QString cardStyle = QStringLiteral(
        "QGroupBox { font-weight: bold; border: 1px solid #E2E8F0; border-radius: 8px; margin-top: 10px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
    );
    if (m_credentialsGroup) m_credentialsGroup->setStyleSheet(cardStyle);
    if (m_activityGroup) m_activityGroup->setStyleSheet(cardStyle);

    QString editStyle = QStringLiteral(
        "QLineEdit { border: 1px solid #CBD5E1; border-radius: 6px; padding: 4px 8px; min-height: 24px; }"
        "QLineEdit:focus { border: 1.5px solid #2563EB; }"
    );
    if (m_serverUrlEdit) m_serverUrlEdit->setStyleSheet(editStyle);
    if (m_usernameEdit) m_usernameEdit->setStyleSheet(editStyle);
    if (m_passwordEdit) m_passwordEdit->setStyleSheet(editStyle);
    if (m_remotePrefixEdit) m_remotePrefixEdit->setStyleSheet(editStyle);

    QString listStyle = QStringLiteral(
        "QListWidget { border: 1px solid #E2E8F0; border-radius: 6px; background-color: #F8FAFC; }"
        "QListWidget::item { padding: 3px 6px; border-bottom: 1px solid #F1F5F9; }"
        "QListWidget::item:selected { background-color: #E2E8F0; color: #0F172A; }"
    );
    if (m_logList) m_logList->setStyleSheet(listStyle);
}

} // namespace LunaRead
