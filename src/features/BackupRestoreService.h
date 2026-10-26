#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>

namespace LunaRead {

struct BackupMetadata {
    QString version = "1.0.0";
    qint64 timestamp = 0;
    int booksCount = 0;
    int annotationsCount = 0;
    qint64 totalReadingSeconds = 0;
};

class BackupRestoreService : public QObject {
    Q_OBJECT

public:
    explicit BackupRestoreService(QObject* parent = nullptr);
    ~BackupRestoreService() override = default;

    bool createBackup(const QString& destinationPath);
    bool restoreBackup(const QString& backupFilePath);

    std::optional<BackupMetadata> inspectBackup(const QString& backupFilePath);

signals:
    void backupProgress(int percentage, const QString& status);
    void backupCompleted(bool success, const QString& message);
    void restoreCompleted(bool success, const QString& message);
};

} // namespace LunaRead
