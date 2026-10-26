#include "BackupRestoreDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

namespace LunaRead {

BackupRestoreDialog::BackupRestoreDialog(QWidget* parent)
    : QDialog(parent)
    , m_service(new BackupRestoreService(this))
{
    setWindowTitle(tr("Library Backup & Restore"));
    resize(480, 260);

    connect(m_service, &BackupRestoreService::backupProgress, this, &BackupRestoreDialog::onProgress);
    connect(m_service, &BackupRestoreService::backupCompleted, this, &BackupRestoreDialog::onBackupDone);
    connect(m_service, &BackupRestoreService::restoreCompleted, this, &BackupRestoreDialog::onRestoreDone);

    setupUi();
}

void BackupRestoreDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto* headerLabel = new QLabel(tr("<h3>🗄️ LunaRead Library Backup</h3>"), this);
    layout->addWidget(headerLabel);

    auto* infoLabel = new QLabel(tr("Safeguard your reading database, bookmarks, annotations, and reading statistics into a portable backup archive."), this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #4B5563; font-size: 11pt;");
    layout->addWidget(infoLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #2563EB; font-weight: 500;");
    layout->addWidget(m_statusLabel);

    layout->addStretch(1);

    auto* btnRow = new QHBoxLayout();
    m_createBtn = new QPushButton(tr("📦 Create Backup Archive..."), this);
    m_createBtn->setStyleSheet("background-color: #3B82F6; color: white; border-radius: 6px; padding: 8px 16px; font-weight: 600;");
    connect(m_createBtn, &QPushButton::clicked, this, &BackupRestoreDialog::onCreateBackupClicked);
    btnRow->addWidget(m_createBtn);

    m_restoreBtn = new QPushButton(tr("🔄 Restore Backup..."), this);
    m_restoreBtn->setStyleSheet("border: 1px solid #D1D5DB; border-radius: 6px; padding: 8px 16px; font-weight: 500;");
    connect(m_restoreBtn, &QPushButton::clicked, this, &BackupRestoreDialog::onRestoreBackupClicked);
    btnRow->addWidget(m_restoreBtn);

    layout->addLayout(btnRow);
}

void BackupRestoreDialog::onCreateBackupClicked() {
    QString defaultName = QString("lunaread_backup_%1.lunabackup").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString dest = QFileDialog::getSaveFileName(this, tr("Save Library Backup"), defaultName, tr("LunaRead Backup (*.lunabackup *.tar.gz)"));
    if (dest.isEmpty()) return;

    m_progressBar->show();
    m_progressBar->setValue(0);
    m_createBtn->setEnabled(false);
    m_restoreBtn->setEnabled(false);

    m_service->createBackup(dest);
}

void BackupRestoreDialog::onRestoreBackupClicked() {
    QString src = QFileDialog::getOpenFileName(this, tr("Select Backup to Restore"), QString(), tr("LunaRead Backup (*.lunabackup *.tar.gz *.db)"));
    if (src.isEmpty()) return;

    if (QMessageBox::warning(this, tr("Confirm Restore"),
        tr("Restoring a backup will overwrite your current reading progress and annotations.\nDo you wish to proceed?"),
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    m_progressBar->show();
    m_progressBar->setValue(0);
    m_createBtn->setEnabled(false);
    m_restoreBtn->setEnabled(false);

    m_service->restoreBackup(src);
}

void BackupRestoreDialog::onProgress(int pct, const QString& status) {
    m_progressBar->setValue(pct);
    m_statusLabel->setText(status);
}

void BackupRestoreDialog::onBackupDone(bool success, const QString& msg) {
    m_progressBar->hide();
    m_createBtn->setEnabled(true);
    m_restoreBtn->setEnabled(true);
    if (success) {
        QMessageBox::information(this, tr("Backup Completed"), msg);
        m_statusLabel->setText(tr("Backup completed successfully."));
    } else {
        QMessageBox::critical(this, tr("Backup Failed"), msg);
        m_statusLabel->setText(tr("Backup failed."));
    }
}

void BackupRestoreDialog::onRestoreDone(bool success, const QString& msg) {
    m_progressBar->hide();
    m_createBtn->setEnabled(true);
    m_restoreBtn->setEnabled(true);
    if (success) {
        QMessageBox::information(this, tr("Restore Completed"), msg);
        m_statusLabel->setText(tr("Library restored successfully."));
    } else {
        QMessageBox::critical(this, tr("Restore Failed"), msg);
        m_statusLabel->setText(tr("Restore failed."));
    }
}

} // namespace LunaRead
