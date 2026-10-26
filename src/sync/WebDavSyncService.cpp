#include "WebDavSyncService.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QRegularExpression>
#include <QStringConverter>
#include <cmath>

namespace LunaRead {

WebDavSyncService::WebDavSyncService(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_ownsNam(true) {
}

WebDavSyncService::WebDavSyncService(QNetworkAccessManager* customNam, QObject* parent)
    : QObject(parent)
    , m_net(customNam ? customNam : new QNetworkAccessManager(this))
    , m_ownsNam(customNam == nullptr) {
}

void WebDavSyncService::setCredentials(const QString& baseUrl, const QString& username, const QString& password) {
    m_baseUrl = baseUrl.trimmed();
    if (!m_baseUrl.isEmpty() && !m_baseUrl.endsWith('/')) {
        m_baseUrl += '/';
    }
    if (!username.isEmpty() || !password.isEmpty()) {
        QString token = QString("%1:%2").arg(username, password);
        m_authHeader = "Basic " + token.toUtf8().toBase64();
    } else {
        m_authHeader.clear();
    }
}

void WebDavSyncService::setNetworkAccessManager(QNetworkAccessManager* nam) {
    if (!nam) return;
    if (m_ownsNam && m_net) {
        m_net->deleteLater();
    }
    m_net = nam;
    m_ownsNam = false;
}

void WebDavSyncService::setRemotePrefix(const QString& prefix) {
    QString p = prefix.trimmed();
    while (p.startsWith('/')) p.remove(0, 1);
    while (p.endsWith('/')) p.chop(1);
    m_remotePrefix = p.isEmpty() ? QStringLiteral(".moonreader") : p;
}

QString WebDavSyncService::bookSyncPath(const QString& bookHash) const {
    return syncFilePath(bookHash, m_remotePrefix);
}

QString WebDavSyncService::syncFilePath(const QString& bookHash, const QString& prefix) {
    QString p = prefix.trimmed();
    while (p.startsWith('/')) p.remove(0, 1);
    while (p.endsWith('/')) p.chop(1);
    if (p.isEmpty()) {
        return QStringLiteral("%1.po").arg(bookHash);
    }
    return QStringLiteral("%1/%2.po").arg(p, bookHash);
}

QByteArray WebDavSyncService::serializeJson(const SyncPayload& payload) {
    SyncPayload copy = payload;
    copy.normalize();

    QJsonObject json;
    // Canonical camelCase fields
    json[QStringLiteral("bookHash")] = copy.bookHash;
    json[QStringLiteral("title")] = copy.title;
    json[QStringLiteral("chapterIndex")] = copy.chapterIndex;
    json[QStringLiteral("pageIndex")] = copy.pageIndex;
    json[QStringLiteral("characterOffset")] = copy.characterOffset;
    json[QStringLiteral("progressPercentage")] = copy.progressPercentage;
    json[QStringLiteral("percentage")] = copy.percentage;
    json[QStringLiteral("timestamp")] = copy.timestamp;
    json[QStringLiteral("deviceName")] = copy.deviceName;

    // Snake_case aliases for maximum cross-client compatibility
    json[QStringLiteral("book_hash")] = copy.bookHash;
    json[QStringLiteral("chapter_index")] = copy.chapterIndex;
    json[QStringLiteral("page_index")] = copy.pageIndex;
    json[QStringLiteral("character_offset")] = copy.characterOffset;
    json[QStringLiteral("progress_percentage")] = copy.progressPercentage;
    json[QStringLiteral("device_name")] = copy.deviceName;

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QByteArray WebDavSyncService::serializeKeyValue(const SyncPayload& payload) {
    SyncPayload copy = payload;
    copy.normalize();

    QStringList lines;
    lines << QStringLiteral("bookHash=%1").arg(copy.bookHash);
    lines << QStringLiteral("title=%1").arg(copy.title);
    lines << QStringLiteral("chapterIndex=%1").arg(copy.chapterIndex);
    lines << QStringLiteral("pageIndex=%1").arg(copy.pageIndex);
    lines << QStringLiteral("characterOffset=%1").arg(copy.characterOffset);
    lines << QStringLiteral("progressPercentage=%1").arg(copy.progressPercentage, 0, 'f', 4);
    lines << QStringLiteral("timestamp=%1").arg(copy.timestamp);
    lines << QStringLiteral("deviceName=%1").arg(copy.deviceName);

    return lines.join(QLatin1Char('\n')).toUtf8();
}

QByteArray WebDavSyncService::serialize(const SyncPayload& payload, PayloadFormat format) {
    if (format == PayloadFormat::KeyValue) {
        return serializeKeyValue(payload);
    }
    return serializeJson(payload);
}

std::optional<SyncPayload> WebDavSyncService::deserializeJson(const QByteArray& data) {
    if (data.trimmed().isEmpty()) {
        return std::nullopt;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }

    QJsonObject obj = doc.object();
    SyncPayload payload;

    // Book hash
    if (obj.contains(QStringLiteral("bookHash"))) {
        payload.bookHash = obj.value(QStringLiteral("bookHash")).toString();
    } else if (obj.contains(QStringLiteral("book_hash"))) {
        payload.bookHash = obj.value(QStringLiteral("book_hash")).toString();
    }

    // Title
    if (obj.contains(QStringLiteral("title"))) {
        payload.title = obj.value(QStringLiteral("title")).toString();
    } else if (obj.contains(QStringLiteral("book_title"))) {
        payload.title = obj.value(QStringLiteral("book_title")).toString();
    }

    // Chapter index
    if (obj.contains(QStringLiteral("chapterIndex"))) {
        payload.chapterIndex = obj.value(QStringLiteral("chapterIndex")).toInt();
    } else if (obj.contains(QStringLiteral("chapter_index"))) {
        payload.chapterIndex = obj.value(QStringLiteral("chapter_index")).toInt();
    }

    // Page index
    if (obj.contains(QStringLiteral("pageIndex"))) {
        payload.pageIndex = obj.value(QStringLiteral("pageIndex")).toInt();
    } else if (obj.contains(QStringLiteral("page_index"))) {
        payload.pageIndex = obj.value(QStringLiteral("page_index")).toInt();
    }

    // Character offset
    if (obj.contains(QStringLiteral("characterOffset"))) {
        payload.characterOffset = obj.value(QStringLiteral("characterOffset")).toInt();
    } else if (obj.contains(QStringLiteral("character_offset"))) {
        payload.characterOffset = obj.value(QStringLiteral("character_offset")).toInt();
    }

    // Progress percentage
    if (obj.contains(QStringLiteral("progressPercentage"))) {
        payload.progressPercentage = obj.value(QStringLiteral("progressPercentage")).toDouble();
    } else if (obj.contains(QStringLiteral("progress_percentage"))) {
        payload.progressPercentage = obj.value(QStringLiteral("progress_percentage")).toDouble();
    } else if (obj.contains(QStringLiteral("percentage"))) {
        payload.progressPercentage = obj.value(QStringLiteral("percentage")).toDouble();
    }
    payload.percentage = payload.progressPercentage;

    // Timestamp
    if (obj.contains(QStringLiteral("timestamp"))) {
        QJsonValue tsVal = obj.value(QStringLiteral("timestamp"));
        if (tsVal.isDouble()) {
            payload.timestamp = static_cast<qint64>(tsVal.toDouble());
        } else if (tsVal.isString()) {
            bool ok = false;
            qint64 val = tsVal.toString().toLongLong(&ok);
            if (ok) {
                payload.timestamp = val;
            } else {
                QDateTime dt = QDateTime::fromString(tsVal.toString(), Qt::ISODate);
                if (dt.isValid()) {
                    payload.timestamp = dt.toMSecsSinceEpoch();
                }
            }
        }
    } else if (obj.contains(QStringLiteral("utc_timestamp"))) {
        payload.timestamp = static_cast<qint64>(obj.value(QStringLiteral("utc_timestamp")).toDouble());
    }

    // Device name
    if (obj.contains(QStringLiteral("deviceName"))) {
        payload.deviceName = obj.value(QStringLiteral("deviceName")).toString();
    } else if (obj.contains(QStringLiteral("device_name"))) {
        payload.deviceName = obj.value(QStringLiteral("device_name")).toString();
    }

    return payload;
}

std::optional<SyncPayload> WebDavSyncService::deserializeKeyValue(const QByteArray& data) {
    if (data.trimmed().isEmpty()) {
        return std::nullopt;
    }

    QString text = QString::fromUtf8(data);
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return std::nullopt;
    }

    SyncPayload payload;
    bool foundAnyField = false;

    for (const QString& line : lines) {
        int delimPos = static_cast<int>(line.indexOf(QLatin1Char('=')));
        if (delimPos < 0) {
            delimPos = static_cast<int>(line.indexOf(QLatin1Char(':')));
        }
        if (delimPos <= 0) continue;

        QString key = line.left(delimPos).trimmed().toLower();
        key.remove(QLatin1Char('_'));
        QString val = line.mid(delimPos + 1).trimmed();

        if (key == QStringLiteral("bookhash")) {
            payload.bookHash = val;
            foundAnyField = true;
        } else if (key == QStringLiteral("title") || key == QStringLiteral("booktitle")) {
            payload.title = val;
            foundAnyField = true;
        } else if (key == QStringLiteral("chapterindex") || key == QStringLiteral("chapter")) {
            payload.chapterIndex = val.toInt();
            foundAnyField = true;
        } else if (key == QStringLiteral("pageindex") || key == QStringLiteral("page")) {
            payload.pageIndex = val.toInt();
            foundAnyField = true;
        } else if (key == QStringLiteral("characteroffset") || key == QStringLiteral("offset") || key == QStringLiteral("charoffset")) {
            payload.characterOffset = val.toInt();
            foundAnyField = true;
        } else if (key == QStringLiteral("progresspercentage") || key == QStringLiteral("percentage") || key == QStringLiteral("progress")) {
            payload.progressPercentage = val.toDouble();
            payload.percentage = payload.progressPercentage;
            foundAnyField = true;
        } else if (key == QStringLiteral("timestamp") || key == QStringLiteral("utctimestamp")) {
            bool ok = false;
            qint64 t = val.toLongLong(&ok);
            if (ok) {
                payload.timestamp = t;
            } else {
                QDateTime dt = QDateTime::fromString(val, Qt::ISODate);
                if (dt.isValid()) {
                    payload.timestamp = dt.toMSecsSinceEpoch();
                }
            }
            foundAnyField = true;
        } else if (key == QStringLiteral("devicename") || key == QStringLiteral("device")) {
            payload.deviceName = val;
            foundAnyField = true;
        }
    }

    if (!foundAnyField) {
        return std::nullopt;
    }

    payload.normalize();
    return payload;
}

std::optional<SyncPayload> WebDavSyncService::deserialize(const QByteArray& data) {
    QByteArray trimmed = data.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    if (trimmed.startsWith('{')) {
        auto res = deserializeJson(trimmed);
        if (res.has_value()) {
            return res;
        }
        return deserializeKeyValue(trimmed);
    } else {
        auto res = deserializeKeyValue(trimmed);
        if (res.has_value()) {
            return res;
        }
        return deserializeJson(trimmed);
    }
}

ConflictResolution WebDavSyncService::resolveConflict(const SyncPayload& local, const SyncPayload& remote) {
    // 1. Primary rule: Last-Write-Wins (LWW) based on UTC timestamp
    if (remote.timestamp > local.timestamp) {
        return ConflictResolution::UseRemote;
    }
    if (local.timestamp > remote.timestamp) {
        return ConflictResolution::UseLocal;
    }

    // 2. Timestamps are identical - check if reading progress is identical
    const double diff = std::abs(remote.progressPercentage - local.progressPercentage);
    const bool sameProgress = (diff < 0.0001) &&
                              (remote.chapterIndex == local.chapterIndex) &&
                              (remote.pageIndex == local.pageIndex) &&
                              (remote.characterOffset == local.characterOffset);
    if (sameProgress) {
        return ConflictResolution::InSync;
    }

    // 3. Deterministic tie-breaking if timestamps match exactly but state diverges:
    // Higher progress wins
    if (remote.progressPercentage > local.progressPercentage) {
        return ConflictResolution::UseRemote;
    }
    if (local.progressPercentage > remote.progressPercentage) {
        return ConflictResolution::UseLocal;
    }

    // Higher character offset wins
    if (remote.characterOffset > local.characterOffset) {
        return ConflictResolution::UseRemote;
    }
    if (local.characterOffset > remote.characterOffset) {
        return ConflictResolution::UseLocal;
    }

    // Default to local to prevent unintentional loss of local reading state
    return ConflictResolution::UseLocal;
}

SyncPayload WebDavSyncService::resolveLww(const SyncPayload& local, const SyncPayload& remote) {
    ConflictResolution res = resolveConflict(local, remote);
    if (res == ConflictResolution::UseRemote) {
        return remote;
    }
    return local;
}

bool WebDavSyncService::shouldApplyRemote(const SyncPayload& local, const SyncPayload& remote) {
    return resolveConflict(local, remote) == ConflictResolution::UseRemote;
}

void WebDavSyncService::ensureRemoteDirectory(const std::function<void(bool)>& callback) {
    if (m_baseUrl.isEmpty()) {
        if (callback) callback(false);
        return;
    }

    QString prefix = m_remotePrefix.trimmed();
    while (prefix.startsWith('/')) prefix.remove(0, 1);
    while (prefix.endsWith('/')) prefix.chop(1);
    if (prefix.isEmpty()) {
        if (callback) callback(true);
        return;
    }

    QUrl dirUrl(m_baseUrl + prefix + QLatin1Char('/'));
    QNetworkRequest request(dirUrl);
    if (!m_authHeader.isEmpty()) {
        request.setRawHeader("Authorization", m_authHeader.toUtf8());
    }

    QNetworkReply* reply = m_net->sendCustomRequest(request, "MKCOL");
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 201 Created, 405 Method Not Allowed (already exists), 200 OK
        bool ok = (status == 201 || status == 405 || status == 200 || reply->error() == QNetworkReply::NoError);
        if (callback) callback(ok);
        reply->deleteLater();
    });
}

void WebDavSyncService::uploadProgress(const SyncPayload& payload, PayloadFormat format) {
    if (m_baseUrl.isEmpty()) {
        emit syncCompleted(false, QStringLiteral("WebDAV Base URL not configured"));
        return;
    }

    QUrl url(m_baseUrl + bookSyncPath(payload.bookHash));
    QNetworkRequest request(url);
    if (!m_authHeader.isEmpty()) {
        request.setRawHeader("Authorization", m_authHeader.toUtf8());
    }

    if (format == PayloadFormat::Json) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    } else {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/plain; charset=utf-8"));
    }

    QByteArray body = serialize(payload, format);
    QNetworkReply* reply = m_net->put(request, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply, payload, format]() {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool ok = (reply->error() == QNetworkReply::NoError);

        // If directory doesn't exist, create it via MKCOL and retry once
        if (!ok && (httpStatus == 404 || httpStatus == 409)) {
            reply->deleteLater();
            ensureRemoteDirectory([this, payload, format](bool dirOk) {
                if (!dirOk) {
                    emit syncCompleted(false, QStringLiteral("Failed to create remote directory on WebDAV server"));
                    return;
                }
                // Retry PUT
                QUrl retryUrl(m_baseUrl + bookSyncPath(payload.bookHash));
                QNetworkRequest retryReq(retryUrl);
                if (!m_authHeader.isEmpty()) {
                    retryReq.setRawHeader("Authorization", m_authHeader.toUtf8());
                }
                retryReq.setHeader(QNetworkRequest::ContentTypeHeader,
                                   format == PayloadFormat::Json ? QStringLiteral("application/json; charset=utf-8")
                                                                 : QStringLiteral("text/plain; charset=utf-8"));
                QByteArray retryBody = serialize(payload, format);
                QNetworkReply* retryReply = m_net->put(retryReq, retryBody);
                connect(retryReply, &QNetworkReply::finished, this, [this, retryReply]() {
                    bool retryOk = (retryReply->error() == QNetworkReply::NoError);
                    emit syncCompleted(retryOk, retryOk ? QStringLiteral("Sync uploaded successfully")
                                                        : retryReply->errorString());
                    retryReply->deleteLater();
                });
            });
            return;
        }

        emit syncCompleted(ok, ok ? QStringLiteral("Sync uploaded successfully") : reply->errorString());
        reply->deleteLater();
    });
}

void WebDavSyncService::downloadProgress(const QString& bookHash) {
    if (m_baseUrl.isEmpty()) {
        emit syncCompleted(false, QStringLiteral("WebDAV Base URL not configured"));
        return;
    }

    QUrl url(m_baseUrl + bookSyncPath(bookHash));
    QNetworkRequest request(url);
    if (!m_authHeader.isEmpty()) {
        request.setRawHeader("Authorization", m_authHeader.toUtf8());
    }

    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray raw = reply->readAll();
            auto payloadOpt = deserialize(raw);
            if (payloadOpt.has_value()) {
                emit remoteProgressReceived(*payloadOpt);
                emit syncCompleted(true, QStringLiteral("Remote progress received"));
            } else {
                emit syncCompleted(false, QStringLiteral("Failed to parse remote Moon+ Reader payload"));
            }
        } else {
            emit syncCompleted(false, reply->errorString());
        }
        reply->deleteLater();
    });
}

void WebDavSyncService::syncBook(const SyncPayload& localPayload, PayloadFormat format) {
    if (m_baseUrl.isEmpty()) {
        emit syncCompleted(false, QStringLiteral("WebDAV Base URL not configured"));
        return;
    }

    QUrl url(m_baseUrl + bookSyncPath(localPayload.bookHash));
    QNetworkRequest request(url);
    if (!m_authHeader.isEmpty()) {
        request.setRawHeader("Authorization", m_authHeader.toUtf8());
    }

    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, localPayload, format]() {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray raw = reply->readAll();
            auto remoteOpt = deserialize(raw);
            if (remoteOpt.has_value()) {
                ConflictResolution res = resolveConflict(localPayload, *remoteOpt);
                if (res == ConflictResolution::UseRemote) {
                    emit remoteProgressReceived(*remoteOpt);
                    emit syncConflictResolved(res, *remoteOpt);
                    emit syncCompleted(true, QStringLiteral("Remote progress applied (newer UTC timestamp)"));
                } else if (res == ConflictResolution::UseLocal) {
                    uploadProgress(localPayload, format);
                    emit syncConflictResolved(res, localPayload);
                } else { // InSync
                    emit syncConflictResolved(res, localPayload);
                    emit syncCompleted(true, QStringLiteral("Progress is already up to date"));
                }
            } else {
                // If remote file is invalid or empty, overwrite with valid local state
                uploadProgress(localPayload, format);
                emit syncConflictResolved(ConflictResolution::UseLocal, localPayload);
            }
        } else if (httpStatus == 404 || reply->error() == QNetworkReply::ContentNotFoundError) {
            // No remote sync record exists yet: push local
            uploadProgress(localPayload, format);
            emit syncConflictResolved(ConflictResolution::UseLocal, localPayload);
        } else {
            emit syncCompleted(false, reply->errorString());
        }
        reply->deleteLater();
    });
}

void WebDavSyncService::testConnection(const std::function<void(bool success, const QString& message)>& callback) {
    if (m_baseUrl.isEmpty()) {
        QString msg = QStringLiteral("WebDAV Base URL is not configured");
        emit connectionTested(false, msg);
        if (callback) callback(false, msg);
        return;
    }

    QUrl url(m_baseUrl);
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        QString msg = QStringLiteral("Invalid WebDAV URL (must start with http:// or https://)");
        emit connectionTested(false, msg);
        if (callback) callback(false, msg);
        return;
    }

    QNetworkRequest request(url);
    if (!m_authHeader.isEmpty()) {
        request.setRawHeader("Authorization", m_authHeader.toUtf8());
    }
    request.setRawHeader("Depth", "0");

    QNetworkReply* reply = m_net->sendCustomRequest(request, "PROPFIND");
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool success = false;
        QString message;

        if (reply->error() == QNetworkReply::NoError || status == 200 || status == 207 || status == 204) {
            success = true;
            message = QStringLiteral("Connection successful (HTTP %1)").arg(status > 0 ? status : 200);
        } else if (status == 401 || status == 403 || reply->error() == QNetworkReply::AuthenticationRequiredError) {
            success = false;
            message = QStringLiteral("Authentication failed (HTTP %1). Please check credentials.").arg(status);
        } else if (status == 405) {
            // PROPFIND method not allowed on endpoint, but server is reachable
            success = true;
            message = QStringLiteral("Server reachable (HTTP 405)");
        } else if (status == 404) {
            success = false;
            message = QStringLiteral("WebDAV directory not found (HTTP 404). Check the server URL path.");
        } else {
            success = false;
            message = QStringLiteral("Connection error: %1 (HTTP %2)").arg(reply->errorString()).arg(status);
        }

        emit connectionTested(success, message);
        if (callback) callback(success, message);
        reply->deleteLater();
    });
}

void WebDavSyncService::testConnection(const QString& baseUrl, const QString& username, const QString& password,
                                       const std::function<void(bool success, const QString& message)>& callback) {
    setCredentials(baseUrl, username, password);
    testConnection(callback);
}

} // namespace LunaRead
