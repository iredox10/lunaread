#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QUrl>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace LunaRead {

/**
 * @brief Represents an Atom/OPDS <link> element with relationship, MIME type, target URI, and optional metadata.
 */
struct OpdsLink {
    QString rel;      ///< Link relationship (e.g., "http://opds-spec.org/acquisition", "subsection", "next")
    QString href;     ///< Target URL (resolved to absolute URL if base URL was available)
    QString type;     ///< MIME type of target resource (e.g., "application/epub+zip", "image/jpeg")
    QString title;    ///< Human-readable link title or label
    qint64 length = 0;///< Resource byte length if provided (0 if unspecified)

    /**
     * @brief Determines whether this link is a book acquisition download link.
     */
    [[nodiscard]] bool isAcquisition() const {
        if (rel.isEmpty()) return false;
        return rel.contains(QStringLiteral("acquisition"), Qt::CaseInsensitive);
    }

    /**
     * @brief Determines whether this link points to a thumbnail cover image.
     */
    [[nodiscard]] bool isThumbnail() const {
        return rel.contains(QStringLiteral("thumbnail"), Qt::CaseInsensitive);
    }

    /**
     * @brief Determines whether this link points to a cover image (full size or generic image).
     */
    [[nodiscard]] bool isImage() const {
        return rel.contains(QStringLiteral("image"), Qt::CaseInsensitive) ||
               rel.contains(QStringLiteral("cover"), Qt::CaseInsensitive);
    }

    /**
     * @brief Determines whether this link is a navigation subsection.
     */
    [[nodiscard]] bool isSubsection() const {
        return rel.compare(QStringLiteral("subsection"), Qt::CaseInsensitive) == 0;
    }

    /**
     * @brief Determines whether this link is a pagination next page link.
     */
    [[nodiscard]] bool isNext() const {
        return rel.compare(QStringLiteral("next"), Qt::CaseInsensitive) == 0;
    }

    /**
     * @brief Determines whether this link is a pagination previous page link.
     */
    [[nodiscard]] bool isPrev() const {
        return rel.compare(QStringLiteral("prev"), Qt::CaseInsensitive) == 0 ||
               rel.compare(QStringLiteral("previous"), Qt::CaseInsensitive) == 0;
    }

    /**
     * @brief Determines whether this link is a pagination first page link.
     */
    [[nodiscard]] bool isFirst() const {
        return rel.compare(QStringLiteral("first"), Qt::CaseInsensitive) == 0;
    }

    /**
     * @brief Determines whether this link is a pagination last page link.
     */
    [[nodiscard]] bool isLast() const {
        return rel.compare(QStringLiteral("last"), Qt::CaseInsensitive) == 0;
    }

    /**
     * @brief Determines whether this link points to an OpenSearch description document.
     */
    [[nodiscard]] bool isSearch() const {
        return rel.compare(QStringLiteral("search"), Qt::CaseInsensitive) == 0 ||
               type.compare(QStringLiteral("application/opensearchdescription+xml"), Qt::CaseInsensitive) == 0;
    }

    /**
     * @brief Determines whether this link represents catalog navigation.
     */
    [[nodiscard]] bool isNavigation() const {
        return isSubsection() ||
               type.contains(QStringLiteral("kind=navigation"), Qt::CaseInsensitive) ||
               type.contains(QStringLiteral("opds-catalog"), Qt::CaseInsensitive) ||
               rel.contains(QStringLiteral("sort"), Qt::CaseInsensitive);
    }
};

/**
 * @brief Author metadata extracted from <author> or <dc:creator>.
 */
struct OpdsAuthor {
    QString name;
    QString uri;
    QString email;
};

/**
 * @brief Category metadata extracted from <category term="..." scheme="..." label="...">.
 */
struct OpdsCategory {
    QString term;
    QString label;
    QString scheme;
};

/**
 * @brief Represents an OPDS catalog entry (typically a book or publication).
 */
struct OpdsBookEntry {
    QString id;           ///< Unique identifier URN/URI
    QString title;        ///< Book or entry title
    QString author;       ///< Primary author or joined author names (backwards-compatible)
    QString summary;      ///< Short plain-text or HTML synopsis
    QString downloadUrl;  ///< Primary preferred acquisition URL (EPUB prioritized)

    // OPDS 1.2 Extended Metadata
    QVector<OpdsAuthor> authors;
    QString content;
    QString contentType;
    QDateTime updated;
    QDateTime published;
    QString language;
    QString publisher;
    QString rights;
    QString identifier;
    QVector<OpdsCategory> categories;

    // Links & Resources
    QVector<OpdsLink> links;
    QVector<OpdsLink> acquisitionLinks;
    QString coverUrl;
    QString thumbnailUrl;
    bool isNavigation = false;

    /**
     * @brief Selects the best acquisition download URL.
     * @param preferredMime MIME type to prefer (e.g., "application/epub+zip").
     * @return Best matching download URL, or first acquisition link, or empty string.
     */
    [[nodiscard]] QString bestDownloadUrl(const QString& preferredMime = QStringLiteral("application/epub+zip")) const;

    /**
     * @brief Retrieves download URL for a specific MIME type if available.
     */
    [[nodiscard]] QString acquisitionUrlForType(const QString& mimeType) const;

    /**
     * @brief Helper to get all category labels as a string list.
     */
    [[nodiscard]] QStringList categoryLabels() const;
};

/**
 * @brief Represents a navigation entry pointing to a subcatalog or search facet.
 */
struct OpdsNavigationLink {
    QString id;
    QString title;
    QString href;
    QString type;
    QString rel;
    QString summary;
    QString iconUrl;
    int count = 0;
};

/**
 * @brief Represents a parsed OPDS feed including metadata, links, book entries, and navigation subsections.
 */
struct OpdsFeed {
    QString id;
    QString title;
    QString subtitle;
    QString iconUrl;
    QDateTime updated;
    OpdsAuthor author;

    // Feed navigation / pagination URLs
    QString selfUrl;
    QString nextUrl;
    QString prevUrl;
    QString firstUrl;
    QString lastUrl;
    QString searchUrl;
    QString startUrl;
    QString upUrl;

    QVector<OpdsLink> links;
    QVector<OpdsBookEntry> entries;
    QVector<OpdsNavigationLink> navigationLinks;

    [[nodiscard]] bool hasNext() const { return !nextUrl.isEmpty(); }
    [[nodiscard]] bool hasPrev() const { return !prevUrl.isEmpty(); }
    [[nodiscard]] bool isNavigationFeed() const { return !navigationLinks.isEmpty() && entries.isEmpty(); }
    [[nodiscard]] bool isAcquisitionFeed() const { return !entries.isEmpty(); }
    [[nodiscard]] qsizetype totalEntries() const { return entries.size(); }
};

/**
 * @class OpdsClient
 * @brief Production-grade OPDS 1.2 catalog client and Atom XML feed parser for LunaRead-Qt.
 *
 * Supports:
 * - Parsing Atom XML 1.0 feeds (<feed>, <entry>, <title>, <author>, <summary>, <content>, <category>, <link>).
 * - Dublin Core metadata (<dc:language>, <dc:publisher>, <dc:issued>, <dc:creator>, <dc:identifier>).
 * - Extraction of acquisition links (EPUB, PDF, MOBI, FB2, CBZ) with mime types and byte lengths.
 * - Thumbnail and cover image URL extraction with automatic fallbacks.
 * - Navigation feeds (subsections, facets, root/start catalog links).
 * - Multi-page pagination links (rel="next", rel="prev", rel="first", rel="last").
 * - Relative URL resolution using feed base URLs or xml:base attributes.
 * - HTTP basic authentication and redirect traversal.
 * - Book downloading to local storage with progress reporting.
 */
class OpdsClient : public QObject {
    Q_OBJECT
public:
    explicit OpdsClient(QObject* parent = nullptr);
    ~OpdsClient() override = default;

    /**
     * @brief Parses an OPDS/Atom XML feed byte array.
     * @param xmlData Raw XML feed bytes.
     * @param baseUrl Base URL for resolving relative links.
     * @return Parsed OpdsFeed structure.
     */
    static OpdsFeed parseFeedXml(const QByteArray& xmlData, const QUrl& baseUrl = QUrl());

    /**
     * @brief Parses an OPDS/Atom XML feed string.
     * @param xmlString Raw XML feed string.
     * @param baseUrl Base URL for resolving relative links.
     * @return Parsed OpdsFeed structure.
     */
    static OpdsFeed parseFeedXml(const QString& xmlString, const QUrl& baseUrl = QUrl());

    /**
     * @brief Fetches an OPDS catalog from the specified URL.
     * @param url Catalog URL.
     * @param user Optional HTTP Basic username.
     * @param pass Optional HTTP Basic password.
     */
    void fetchCatalog(const QString& url, const QString& user = QString(), const QString& pass = QString());
    void fetchCatalog(const QUrl& url, const QString& user = QString(), const QString& pass = QString());

    /**
     * @brief Fetches the next page of an OPDS feed if rel="next" is present.
     */
    void fetchNextPage(const OpdsFeed& feed, const QString& user = QString(), const QString& pass = QString());

    /**
     * @brief Downloads a book publication file from an acquisition URL.
     * @param downloadUrl Acquisition URL.
     * @param destinationFilePath Absolute path where downloaded file should be saved.
     * @param user Optional HTTP Basic username.
     * @param pass Optional HTTP Basic password.
     */
    void downloadBook(const QString& downloadUrl, const QString& destinationFilePath,
                      const QString& user = QString(), const QString& pass = QString());

    /**
     * @brief Cancels all pending network requests.
     */
    void cancelAllRequests();

signals:
    /// Emitted when catalog entries are parsed (backwards-compatible with original API).
    void catalogLoaded(const QVector<OpdsBookEntry>& books);

    /// Emitted with the full OpdsFeed structure including metadata, navigation links, and pagination.
    void feedLoaded(const OpdsFeed& feed);

    /// Emitted during book file download with progress bytes.
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /// Emitted when a book download completes successfully.
    void downloadFinished(const QString& localFilePath);

    /// Emitted when an error occurs during network fetch or parsing.
    void errorOccurred(const QString& message);

private:
    static OpdsAuthor parseAuthorElement(QXmlStreamReader& xml);
    static OpdsLink parseLinkElement(QXmlStreamReader& xml, const QUrl& baseUrl);
    static OpdsBookEntry parseEntryElement(QXmlStreamReader& xml, const QUrl& baseUrl);
    static void categorizeFeedLink(OpdsFeed& feed, const OpdsLink& link);
    static QString resolveUrl(const QString& rawUrl, const QUrl& baseUrl);

    QNetworkAccessManager* m_net = nullptr;
    QVector<QNetworkReply*> m_activeReplies;
};

} // namespace LunaRead

Q_DECLARE_METATYPE(LunaRead::OpdsBookEntry)
Q_DECLARE_METATYPE(LunaRead::OpdsFeed)
