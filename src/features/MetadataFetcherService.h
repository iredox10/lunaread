#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QPointer>
#include <optional>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Service for fetching book metadata and high-resolution cover art from OpenLibrary API.
 *
 * Conforms to C++20 and Qt 6.11 standards.
 * Queries: https://openlibrary.org/search.json?title=...&author=...
 * Parses:
 * - Title
 * - Author(s)
 * - First publish year
 * - Publisher
 * - ISBN
 * - Description / summary
 * - Cover ID and high-resolution cover image URL (https://covers.openlibrary.org/b/id/{cover_i}-L.jpg)
 */
class MetadataFetcherService : public QObject {
    Q_OBJECT

public:
    explicit MetadataFetcherService(QObject* parent = nullptr);
    ~MetadataFetcherService() override;

    /**
     * @brief Asynchronously search for book metadata on OpenLibrary by title and optional author.
     * @param title Book title to search.
     * @param author Optional author name to refine search.
     */
    void searchMetadata(const QString& title, const QString& author = "");

    /**
     * @brief Asynchronously download book cover image to local target path.
     * @param coverUrl URL of cover image.
     * @param targetPath Local file path to save image. If empty, saves to app data covers dir.
     */
    void downloadCover(const QString& coverUrl, const QString& targetPath = "");

    /**
     * @brief Cancel any pending search or download requests.
     */
    void cancelPendingRequests();

    /**
     * @brief Parse raw OpenLibrary JSON response into metadata map and cover URL.
     * @param jsonData Raw JSON payload from OpenLibrary API.
     * @param outCoverUrl Optional pointer to receive extracted cover URL.
     * @return Extracted metadata map if valid and contains docs; std::nullopt otherwise.
     */
    static std::optional<QVariantMap> parseOpenLibraryResponse(const QByteArray& jsonData, QString* outCoverUrl = nullptr);

signals:
    void metadataFound(const QVariantMap& metadata, const QString& coverUrl);
    void coverDownloaded(const QString& localPath);
    void fetchError(const QString& errorMessage);

private slots:
    void onSearchFinished();
    void onCoverDownloadFinished(const QString& targetPath);

private:
    QNetworkAccessManager* m_networkManager = nullptr;
    QPointer<QNetworkReply> m_searchReply;
    QPointer<QNetworkReply> m_coverReply;
    QString m_pendingSearchTitle;
};

} // namespace LunaRead
