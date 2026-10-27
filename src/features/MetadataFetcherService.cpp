#include "MetadataFetcherService.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <QImage>
#include <QTimer>
#include <QDebug>

namespace LunaRead {

MetadataFetcherService::MetadataFetcherService(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)) {
}

MetadataFetcherService::~MetadataFetcherService() {
    cancelPendingRequests();
}

void MetadataFetcherService::cancelPendingRequests() {
    if (m_searchReply) {
        m_searchReply->disconnect(this);
        m_searchReply->abort();
        m_searchReply->deleteLater();
        m_searchReply = nullptr;
    }
    if (m_coverReply) {
        m_coverReply->disconnect(this);
        m_coverReply->abort();
        m_coverReply->deleteLater();
        m_coverReply = nullptr;
    }
}

void MetadataFetcherService::searchMetadata(const QString& title, const QString& author) {
    const QString cleanTitle = title.trimmed();
    if (cleanTitle.isEmpty()) {
        emit fetchError(tr("Search title cannot be empty."));
        return;
    }

    if (m_searchReply) {
        m_searchReply->disconnect(this);
        m_searchReply->abort();
        m_searchReply->deleteLater();
        m_searchReply = nullptr;
    }

    m_pendingSearchTitle = cleanTitle;

    QUrl url(QStringLiteral("https://openlibrary.org/search.json"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("title"), cleanTitle);
    const QString cleanAuthor = author.trimmed();
    if (!cleanAuthor.isEmpty()) {
        query.addQueryItem(QStringLiteral("author"), cleanAuthor);
    }
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("5"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LunaRead/1.0 (Qt6 desktop reader; +https://github.com/lunaread)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_searchReply = m_networkManager->get(request);

    // Timeout safety: 15 seconds
    QPointer<QNetworkReply> replyPtr = m_searchReply;
    QTimer::singleShot(15000, this, [this, replyPtr]() {
        if (replyPtr && replyPtr->isRunning()) {
            replyPtr->abort();
            emit fetchError(tr("Request timed out while connecting to OpenLibrary."));
        }
    });

    connect(m_searchReply, &QNetworkReply::finished, this, &MetadataFetcherService::onSearchFinished);
}

void MetadataFetcherService::onSearchFinished() {
    if (!m_searchReply) {
        return;
    }

    QPointer<QNetworkReply> reply = m_searchReply;
    m_searchReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit fetchError(tr("Network error: %1").arg(reply->errorString()));
        }
        return;
    }

    const QByteArray data = reply->readAll();
    QString coverUrl;
    auto metaOpt = parseOpenLibraryResponse(data, &coverUrl);
    if (!metaOpt.has_value()) {
        emit fetchError(tr("No metadata found for \"%1\" on OpenLibrary.").arg(m_pendingSearchTitle));
        return;
    }

    emit metadataFound(*metaOpt, coverUrl);
}

void MetadataFetcherService::downloadCover(const QString& coverUrl, const QString& targetPath) {
    const QString cleanUrl = coverUrl.trimmed();
    if (cleanUrl.isEmpty()) {
        emit fetchError(tr("Cover URL is empty."));
        return;
    }

    if (m_coverReply) {
        m_coverReply->disconnect(this);
        m_coverReply->abort();
        m_coverReply->deleteLater();
        m_coverReply = nullptr;
    }

    QString resolvedPath = targetPath.trimmed();
    if (resolvedPath.isEmpty()) {
        const QString coversDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/covers");
        QDir().mkpath(coversDir);
        resolvedPath = coversDir + QStringLiteral("/") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".jpg");
    }

    QUrl url(cleanUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LunaRead/1.0 (Qt6 desktop reader; +https://github.com/lunaread)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_coverReply = m_networkManager->get(request);

    // Timeout safety: 20 seconds
    QPointer<QNetworkReply> replyPtr = m_coverReply;
    QTimer::singleShot(20000, this, [this, replyPtr]() {
        if (replyPtr && replyPtr->isRunning()) {
            replyPtr->abort();
            emit fetchError(tr("Request timed out while downloading cover image."));
        }
    });

    connect(m_coverReply, &QNetworkReply::finished, this, [this, resolvedPath]() {
        onCoverDownloadFinished(resolvedPath);
    });
}

void MetadataFetcherService::onCoverDownloadFinished(const QString& targetPath) {
    if (!m_coverReply) {
        return;
    }

    QPointer<QNetworkReply> reply = m_coverReply;
    m_coverReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit fetchError(tr("Failed to download cover: %1").arg(reply->errorString()));
        }
        return;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode >= 300) {
        emit fetchError(tr("Server returned HTTP error %1 when downloading cover.").arg(statusCode));
        return;
    }

    const QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        emit fetchError(tr("Downloaded cover data is empty."));
        return;
    }

    QImage img;
    if (!img.loadFromData(data)) {
        emit fetchError(tr("Downloaded file is not a valid image format."));
        return;
    }

    // OpenLibrary returns 1x1 transparent image when no cover exists for that ID
    if (img.width() <= 1 && img.height() <= 1) {
        emit fetchError(tr("No cover image available on OpenLibrary for this book."));
        return;
    }

    QFileInfo fi(targetPath);
    QDir().mkpath(fi.dir().absolutePath());

    QFile outFile(targetPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        emit fetchError(tr("Failed to save cover image to disk: %1").arg(outFile.errorString()));
        return;
    }

    if (outFile.write(data) != data.size()) {
        emit fetchError(tr("Failed to write complete cover image to disk."));
        return;
    }
    outFile.close();

    emit coverDownloaded(targetPath);
}

std::optional<QVariantMap> MetadataFetcherService::parseOpenLibraryResponse(const QByteArray& jsonData, QString* outCoverUrl) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        return std::nullopt;
    }

    const QJsonObject root = doc.object();
    const QJsonArray docs = root.value(QStringLiteral("docs")).toArray();
    if (docs.isEmpty()) {
        return std::nullopt;
    }

    const QJsonObject bookDoc = docs.first().toObject();

    // 1. Title
    const QString title = bookDoc.value(QStringLiteral("title")).toString();

    // 2. Author(s)
    QStringList authors;
    if (bookDoc.contains(QStringLiteral("author_name"))) {
        const QJsonValue authorVal = bookDoc.value(QStringLiteral("author_name"));
        if (authorVal.isArray()) {
            const QJsonArray arr = authorVal.toArray();
            for (const auto& a : arr) {
                const QString name = a.toString().trimmed();
                if (!name.isEmpty()) {
                    authors.append(name);
                }
            }
        } else if (authorVal.isString()) {
            const QString name = authorVal.toString().trimmed();
            if (!name.isEmpty()) {
                authors.append(name);
            }
        }
    }
    const QString authorStr = authors.join(QStringLiteral(", "));

    // 3. First publish year
    int publishYear = 0;
    if (bookDoc.contains(QStringLiteral("first_publish_year"))) {
        publishYear = bookDoc.value(QStringLiteral("first_publish_year")).toInt();
    } else if (bookDoc.contains(QStringLiteral("publish_year"))) {
        const QJsonArray yearArr = bookDoc.value(QStringLiteral("publish_year")).toArray();
        for (const auto& y : yearArr) {
            const int yr = y.toInt();
            if (yr > 0 && (publishYear == 0 || yr < publishYear)) {
                publishYear = yr;
            }
        }
    }

    // 4. Publisher
    QString publisher;
    if (bookDoc.contains(QStringLiteral("publisher"))) {
        const QJsonValue pubVal = bookDoc.value(QStringLiteral("publisher"));
        if (pubVal.isArray()) {
            const QJsonArray arr = pubVal.toArray();
            for (const auto& p : arr) {
                const QString pStr = p.toString().trimmed();
                if (!pStr.isEmpty()) {
                    publisher = pStr;
                    break;
                }
            }
        } else if (pubVal.isString()) {
            publisher = pubVal.toString().trimmed();
        }
    }

    // 5. ISBN
    QString isbn;
    if (bookDoc.contains(QStringLiteral("isbn"))) {
        const QJsonValue isbnVal = bookDoc.value(QStringLiteral("isbn"));
        if (isbnVal.isArray()) {
            const QJsonArray arr = isbnVal.toArray();
            QString fallbackIsbn;
            for (const auto& i : arr) {
                const QString iStr = i.toString().trimmed();
                if (iStr.startsWith(QStringLiteral("978")) || iStr.startsWith(QStringLiteral("979")) || iStr.length() == 13) {
                    isbn = iStr;
                    break;
                } else if (fallbackIsbn.isEmpty() && !iStr.isEmpty()) {
                    fallbackIsbn = iStr;
                }
            }
            if (isbn.isEmpty()) {
                isbn = fallbackIsbn;
            }
        } else if (isbnVal.isString()) {
            isbn = isbnVal.toString().trimmed();
        }
    }

    // 6. Description / Summary
    QString description;
    if (bookDoc.contains(QStringLiteral("description"))) {
        const QJsonValue dVal = bookDoc.value(QStringLiteral("description"));
        if (dVal.isString()) {
            description = dVal.toString().trimmed();
        } else if (dVal.isObject()) {
            description = dVal.toObject().value(QStringLiteral("value")).toString().trimmed();
        }
    }
    if (description.isEmpty() && bookDoc.contains(QStringLiteral("first_sentence"))) {
        const QJsonValue fsVal = bookDoc.value(QStringLiteral("first_sentence"));
        if (fsVal.isArray()) {
            const QJsonArray arr = fsVal.toArray();
            for (const auto& s : arr) {
                if (s.isString() && !s.toString().trimmed().isEmpty()) {
                    description = s.toString().trimmed();
                    break;
                } else if (s.isObject()) {
                    description = s.toObject().value(QStringLiteral("value")).toString().trimmed();
                    if (!description.isEmpty()) break;
                }
            }
        } else if (fsVal.isObject()) {
            description = fsVal.toObject().value(QStringLiteral("value")).toString().trimmed();
        } else if (fsVal.isString()) {
            description = fsVal.toString().trimmed();
        }
    }
    if (description.isEmpty() && bookDoc.contains(QStringLiteral("subtitle"))) {
        description = bookDoc.value(QStringLiteral("subtitle")).toString().trimmed();
    }

    // 7. Cover ID & Cover URL
    qint64 coverId = 0;
    if (bookDoc.contains(QStringLiteral("cover_i"))) {
        coverId = bookDoc.value(QStringLiteral("cover_i")).toInteger();
    }

    QString coverUrl;
    if (coverId > 0) {
        coverUrl = QString(QStringLiteral("https://covers.openlibrary.org/b/id/%1-L.jpg")).arg(coverId);
    } else if (!isbn.isEmpty()) {
        coverUrl = QString(QStringLiteral("https://covers.openlibrary.org/b/isbn/%1-L.jpg")).arg(isbn);
    } else if (bookDoc.contains(QStringLiteral("cover_edition_key"))) {
        coverUrl = QString(QStringLiteral("https://covers.openlibrary.org/b/olid/%1-L.jpg")).arg(bookDoc.value(QStringLiteral("cover_edition_key")).toString());
    }

    if (outCoverUrl) {
        *outCoverUrl = coverUrl;
    }

    QVariantMap metadata;
    metadata[QStringLiteral("title")] = title;
    metadata[QStringLiteral("author")] = authorStr;
    metadata[QStringLiteral("authors")] = authors;
    metadata[QStringLiteral("year")] = publishYear;
    metadata[QStringLiteral("first_publish_year")] = publishYear;
    metadata[QStringLiteral("firstPublishYear")] = publishYear;
    metadata[QStringLiteral("publisher")] = publisher;
    metadata[QStringLiteral("isbn")] = isbn;
    metadata[QStringLiteral("description")] = description;
    metadata[QStringLiteral("summary")] = description;
    metadata[QStringLiteral("coverId")] = coverId > 0 ? QString::number(coverId) : QString();
    metadata[QStringLiteral("cover_i")] = coverId;
    metadata[QStringLiteral("coverUrl")] = coverUrl;

    return metadata;
}

} // namespace LunaRead
