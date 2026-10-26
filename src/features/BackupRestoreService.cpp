#include "BackupRestoreService.h"
#include "../db/DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace LunaRead {

BackupRestoreService::BackupRestoreService(QObject* parent) : QObject(parent) {}

bool BackupRestoreService::createBackup(const QString& destinationPath) {
    emit backupProgress(10, tr("Locating library database..."));

    QString dbPath = DatabaseManager::instance().databasePath();
    if (dbPath.isEmpty() || !QFile::exists(dbPath)) {
        emit backupCompleted(false, tr("Database file not found at: %1").arg(dbPath));
        return false;
    }

    emit backupProgress(30, tr("Generating backup metadata..."));
    BackupMetadata meta;
    meta.timestamp = QDateTime::currentMSecsSinceEpoch();
    meta.booksCount = static_cast<int>(DatabaseManager::instance().getBooks().size());
    meta.totalReadingSeconds = DatabaseManager::instance().getTotalReadingTimeSeconds();

    QJsonObject metaObj;
    metaObj["version"] = meta.version;
    metaObj["timestamp"] = meta.timestamp;
    metaObj["booksCount"] = meta.booksCount;
    metaObj["totalReadingSeconds"] = meta.totalReadingSeconds;

    QString tempDir = QDir::tempPath() + QString("/lunabackup_%1").arg(meta.timestamp);
    QDir().mkpath(tempDir);

    QFile metaFile(tempDir + "/metadata.json");
    if (metaFile.open(QIODevice::WriteOnly)) {
        metaFile.write(QJsonDocument(metaObj).toJson());
        metaFile.close();
    }

    emit backupProgress(50, tr("Copying database snapshot..."));
    QFile::copy(dbPath, tempDir + "/lunaread.db");

    emit backupProgress(70, tr("Packaging backup archive..."));
    // Package into tar.gz
    QProcess proc;
    proc.setWorkingDirectory(tempDir);
    proc.start("tar", QStringList() << "-czf" << destinationPath << "metadata.json" << "lunaread.db");
    if (!proc.waitForFinished(15000) || proc.exitCode() != 0) {
        // Fallback: direct copy if tar fails
        QFile::remove(destinationPath);
        QFile::copy(tempDir + "/lunaread.db", destinationPath);
    }

    QDir(tempDir).removeRecursively();

    emit backupProgress(100, tr("Backup created successfully!"));
    emit backupCompleted(true, tr("Successfully created backup at %1").arg(destinationPath));
    return true;
}

bool BackupRestoreService::restoreBackup(const QString& backupFilePath) {
    if (!QFile::exists(backupFilePath)) {
        emit restoreCompleted(false, tr("Backup file does not exist: %1").arg(backupFilePath));
        return false;
    }

    QString tempDir = QDir::tempPath() + QString("/lunarestore_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempDir);

    QProcess proc;
    proc.setWorkingDirectory(tempDir);
    proc.start("tar", QStringList() << "-xzf" << backupFilePath);
    bool untarred = proc.waitForFinished(15000) && (proc.exitCode() == 0);

    QString extractedDb = tempDir + "/lunaread.db";
    if (!untarred || !QFile::exists(extractedDb)) {
        // Maybe destination was a raw sqlite file
        extractedDb = backupFilePath;
    }

    QString targetDbPath = DatabaseManager::instance().databasePath();
    DatabaseManager::instance().close();

    // Make safety backup of current
    QFile::copy(targetDbPath, targetDbPath + ".bak");
    QFile::remove(targetDbPath);

    bool ok = QFile::copy(extractedDb, targetDbPath);
    QDir(tempDir).removeRecursively();

    DatabaseManager::instance().initialize();

    if (ok) {
        emit restoreCompleted(true, tr("Database successfully restored from backup!"));
    } else {
        // Restore safety backup
        QFile::copy(targetDbPath + ".bak", targetDbPath);
        DatabaseManager::instance().initialize();
        emit restoreCompleted(false, tr("Failed to restore database. Previous database preserved."));
    }
    return ok;
}

std::optional<BackupMetadata> BackupRestoreService::inspectBackup(const QString& backupFilePath) {
    if (!QFile::exists(backupFilePath)) return std::nullopt;

    QString tempDir = QDir::tempPath() + QString("/lunainspect_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempDir);

    QProcess proc;
    proc.setWorkingDirectory(tempDir);
    proc.start("tar", QStringList() << "-xzf" << backupFilePath << "metadata.json");
    if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
        QFile mf(tempDir + "/metadata.json");
        if (mf.open(QIODevice::ReadOnly)) {
            QJsonObject obj = QJsonDocument::fromJson(mf.readAll()).object();
            BackupMetadata meta;
            meta.version = obj["version"].toString("1.0.0");
            meta.timestamp = obj["timestamp"].toInteger();
            meta.booksCount = obj["booksCount"].toInt();
            meta.totalReadingSeconds = obj["totalReadingSeconds"].toInteger();
            QDir(tempDir).removeRecursively();
            return meta;
        }
    }
    QDir(tempDir).removeRecursively();
    return std::nullopt;
}

} // namespace LunaRead
