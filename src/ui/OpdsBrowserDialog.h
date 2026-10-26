#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QPixmap>
#include <QPointer>
#include "../opds/OpdsClient.h"
#include "../core/Types.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QScrollArea;
class QVBoxLayout;
class QLabel;
class QProgressBar;
class QSplitter;
class QNetworkAccessManager;
QT_END_NAMESPACE

namespace LunaRead {

struct OpdsDownloadTask {
    OpdsBookEntry entry;
    QString destinationPath;
    QString format;
};

class OpdsBookCardWidget;

/**
 * @brief OPDS Catalog Browser dialog for LunaRead desktop reader.
 *
 * Allows exploring remote OPDS 1.2 / Atom feeds, navigating categories,
 * searching for publications, and downloading free e-books directly into
 * the LunaRead SQLite library.
 */
class OpdsBrowserDialog : public QDialog {
    Q_OBJECT

public:
    explicit OpdsBrowserDialog(QWidget* parent = nullptr);
    ~OpdsBrowserDialog() override;

    /**
     * @brief Loads and parses an OPDS catalog feed from the specified URL.
     */
    void loadCatalog(const QString& catalogUrl);

signals:
    /**
     * @brief Emitted when a book has been downloaded, parsed, and inserted into DatabaseManager.
     * @param bookId The unique identifier of the newly inserted book.
     */
    void bookImported(const QString& bookId);

private slots:
    void onCatalogSelectionChanged(int index);
    void onGoRefreshClicked();
    void onBackClicked();
    void onForwardClicked();
    void onSearchTriggered();
    void onSearchTextChanged(const QString& query);
    void onNavTreeItemActivated(QTreeWidgetItem* item, int column);
    void onFeedLoaded(const OpdsFeed& feed);
    void onDownloadRequested(const OpdsBookEntry& entry, const QString& formatMime);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(const QString& localFilePath);
    void onClientError(const QString& errorMessage);

private:
    void setupUi();
    void setupStyles();
    void populateNavigationTree(const OpdsFeed& feed);
    void populateBookList(const QVector<OpdsBookEntry>& books);
    void navigateTo(const QString& targetUrl, bool addToHistory = true);
    void requestThumbnail(const QString& imageUrl, QLabel* targetLabel);
    QString sanitizeFilename(const QString& rawTitle, const QString& extension) const;
    QString determineExtension(const QString& fileUrl, const QString& mimeType) const;
    bool isBookInLibrary(const QString& bookTitle, const QString& bookAuthor) const;
    void updatePaginationControls();

    // Top Navigation & Address Bar
    QComboBox* m_catalogCombo = nullptr;
    QLineEdit* m_urlEdit = nullptr;
    QPushButton* m_goBtn = nullptr;
    QPushButton* m_backBtn = nullptr;
    QPushButton* m_forwardBtn = nullptr;

    // Search Bar
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;

    // Splitter & Navigation Tree
    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_navTree = nullptr;

    // Books Content Area
    QLabel* m_feedTitleLabel = nullptr;
    QLabel* m_feedSubtitleLabel = nullptr;
    QLabel* m_feedCountLabel = nullptr;
    QScrollArea* m_bookScrollArea = nullptr;
    QWidget* m_bookContainer = nullptr;
    QVBoxLayout* m_bookLayout = nullptr;

    // Pagination
    QPushButton* m_prevPageBtn = nullptr;
    QLabel* m_pageStatusLabel = nullptr;
    QPushButton* m_nextPageBtn = nullptr;

    // Bottom Bar
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // Engine & State
    OpdsClient* m_client = nullptr;
    QNetworkAccessManager* m_thumbNet = nullptr;
    OpdsFeed m_currentFeed;
    QStringList m_history;
    int m_historyIndex = -1;
    QMap<QString, QPixmap> m_thumbnailCache;
    QMap<QString, OpdsDownloadTask> m_pendingDownloads;
    QVector<OpdsBookEntry> m_currentBooks;
};

} // namespace LunaRead
