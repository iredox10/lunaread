#pragma once

#include <QDialog>
#include "../features/BackupRestoreService.h"

class QLabel;
class QPushButton;
class QProgressBar;

namespace LunaRead {

class BackupRestoreDialog : public QDialog {
    Q_OBJECT

public:
    explicit BackupRestoreDialog(QWidget* parent = nullptr);
    ~BackupRestoreDialog() override = default;

private slots:
    void onCreateBackupClicked();
    void onRestoreBackupClicked();
    void onProgress(int pct, const QString& status);
    void onBackupDone(bool success, const QString& msg);
    void onRestoreDone(bool success, const QString& msg);

private:
    void setupUi();

    BackupRestoreService* m_service = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_createBtn = nullptr;
    QPushButton* m_restoreBtn = nullptr;
};

} // namespace LunaRead
