#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <optional>
#include <functional>

namespace LunaRead {

/**
 * @brief Represents the reading progress payload stored in Moon+ Reader sync files (.po).
 *
 * Stored at: .moonreader/<MD5_HASH>.po on WebDAV/Cloud storage.
 * Contains reading progress, chapter and page pointers, character offset, UTC timestamp,
 * and originating device identification.
 */
struct SyncPayload {
    QString bookHash;           ///< MD5 hash of the book file (hex string, e.g. 32 chars)
    QString title;              ///< Optional human-readable book title
    int chapterIndex = 0;       ///< 0-based chapter index
    int pageIndex = 0;          ///< 0-based page index within current chapter
    int characterOffset = 0;    ///< 0-based character offset within chapter / page
    double progressPercentage = 0.0; ///< Reading progress (0.0 - 100.0)
    double percentage = 0.0;    ///< Alias kept in sync with progressPercentage for backward compatibility
    qint64 timestamp = 0;       ///< UTC timestamp in milliseconds since Unix epoch
    QString deviceName;         ///< Identifier of the device that wrote the progress

    /// Synchronizes progressPercentage and percentage
    void setProgress(double p) {
        progressPercentage = p;
        percentage = p;
    }

    /// Normalizes internal state ensuring percentage and progressPercentage match
    void normalize() {
        if (progressPercentage == 0.0 && percentage != 0.0) {
            progressPercentage = percentage;
        } else {
            percentage = progressPercentage;
        }
    }

    /// Checks if payload contains basic valid identifiers
    [[nodiscard]] bool isValid() const {
        return !bookHash.isEmpty() && timestamp > 0;
    }

    bool operator==(const SyncPayload& other) const = default;
};

/**
 * @brief Serialization format for Moon+ Reader sync file (.po).
 */
enum class PayloadFormat {
    Json,
    KeyValue
};

/**
 * @brief Conflict resolution result for Last-Write-Wins (LWW) evaluation.
 */
enum class ConflictResolution {
    UseLocal,   ///< Local timestamp is newer; keep local and push to remote
    UseRemote,  ///< Remote timestamp is newer; overwrite local with remote
    InSync      ///< Timestamps and progress are identical; no sync action needed
};

/**
 * @brief Service responsible for WebDAV-based reading progress synchronization
 * compatible with Moon+ Reader (.moonreader/<MD5>.po).
 *
 * Implements:
 * - Moon+ Reader sync file path formatting: `.moonreader/<MD5_HASH>.po`
 * - Payload serialization & deserialization (both JSON and Key-Value formats)
 * - Last-Write-Wins (LWW) UTC timestamp conflict resolution
 * - HTTP PUT / GET WebDAV operations with Basic Auth
 * - Automatic MKCOL directory creation on 404/409 conflicts
 */
class WebDavSyncService : public QObject {
    Q_OBJECT
public:
    explicit WebDavSyncService(QObject* parent = nullptr);
    explicit WebDavSyncService(QNetworkAccessManager* customNam, QObject* parent = nullptr);
    ~WebDavSyncService() override = default;

    // --- Configuration ---
    void setCredentials(const QString& baseUrl, const QString& username, const QString& password);
    void setNetworkAccessManager(QNetworkAccessManager* nam);
    [[nodiscard]] const QString& baseUrl() const { return m_baseUrl; }
    [[nodiscard]] const QString& authHeader() const { return m_authHeader; }

    // --- Moon+ Reader File Path & Formatting ---
    void setRemotePrefix(const QString& prefix);
    [[nodiscard]] const QString& remotePrefix() const { return m_remotePrefix; }
    [[nodiscard]] QString bookSyncPath(const QString& bookHash) const;
    /// Generates canonical Moon+ Reader sync path: `.moonreader/<MD5_HASH>.po`
    [[nodiscard]] static QString syncFilePath(const QString& bookHash, const QString& prefix = QStringLiteral(".moonreader"));

    // --- Payload Serialization & Deserialization ---
    /// Serializes payload to JSON formatted byte array
    [[nodiscard]] static QByteArray serializeJson(const SyncPayload& payload);

    /// Serializes payload to Key-Value formatted byte array (key=value lines)
    [[nodiscard]] static QByteArray serializeKeyValue(const SyncPayload& payload);

    /// Serializes payload using specified format
    [[nodiscard]] static QByteArray serialize(const SyncPayload& payload, PayloadFormat format = PayloadFormat::Json);

    /// Deserializes payload from byte array, auto-detecting JSON vs Key-Value
    [[nodiscard]] static std::optional<SyncPayload> deserialize(const QByteArray& data);

    /// Deserializes specifically from JSON
    [[nodiscard]] static std::optional<SyncPayload> deserializeJson(const QByteArray& data);

    /// Deserializes specifically from Key-Value lines
    [[nodiscard]] static std::optional<SyncPayload> deserializeKeyValue(const QByteArray& data);

    // --- Conflict Resolution (Last-Write-Wins LWW) ---
    /**
     * @brief Resolves conflict between local and remote progress using Last-Write-Wins (LWW)
     * based on UTC timestamp.
     *
     * @param local Local reading progress
     * @param remote Remote reading progress from WebDAV
     * @return ConflictResolution indicating which payload wins
     */
    [[nodiscard]] static ConflictResolution resolveConflict(const SyncPayload& local, const SyncPayload& remote);

    /**
     * @brief Returns the winning payload according to Last-Write-Wins (LWW).
     */
    [[nodiscard]] static SyncPayload resolveLww(const SyncPayload& local, const SyncPayload& remote);

    /**
     * @brief Helper returning true if remote payload should overwrite local state.
     */
    [[nodiscard]] static bool shouldApplyRemote(const SyncPayload& local, const SyncPayload& remote);

    // --- Network WebDAV Operations ---
    /**
     * @brief Uploads local progress payload to WebDAV server (.moonreader/<MD5>.po).
     */
    void uploadProgress(const SyncPayload& payload, PayloadFormat format = PayloadFormat::Json);

    /**
     * @brief Downloads progress payload from WebDAV server for the given book hash.
     */
    void downloadProgress(const QString& bookHash);

    /**
     * @brief Performs full two-way synchronization for a book:
     * downloads remote progress, executes LWW conflict resolution,
     * and either applies remote progress or pushes local progress.
     */
    void syncBook(const SyncPayload& localPayload, PayloadFormat format = PayloadFormat::Json);

    // --- Connectivity & Testing ---
    /**
     * @brief Tests connectivity and authentication to the WebDAV server.
     */
    void testConnection(const std::function<void(bool success, const QString& message)>& callback = nullptr);
    void testConnection(const QString& baseUrl, const QString& username, const QString& password,
                        const std::function<void(bool success, const QString& message)>& callback = nullptr);

signals:
    void syncCompleted(bool success, const QString& message);
    void remoteProgressReceived(const LunaRead::SyncPayload& payload);
    void syncConflictResolved(LunaRead::ConflictResolution resolution, const LunaRead::SyncPayload& winningPayload);
    void connectionTested(bool success, const QString& message);

private:
    void ensureRemoteDirectory(const std::function<void(bool)>& callback);

    QNetworkAccessManager* m_net = nullptr;
    bool m_ownsNam = true;
    QString m_baseUrl;
    QString m_authHeader;
    QString m_remotePrefix{QStringLiteral(".moonreader")};
};

} // namespace LunaRead

Q_DECLARE_METATYPE(LunaRead::SyncPayload)
Q_DECLARE_METATYPE(LunaRead::ConflictResolution)
Q_DECLARE_METATYPE(LunaRead::PayloadFormat)
