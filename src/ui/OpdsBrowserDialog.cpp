#include "OpdsBrowserDialog.h"
#include "../db/DatabaseManager.h"
#include "../parsers/ParserFactory.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QScrollArea>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QMessageBox>
#include <QInputDialog>
#include <QTextDocumentFragment>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
#include <QDateTime>
#include <QRegularExpression>
#include <QPainter>
#include <QPainterPath>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalBlocker>

namespace LunaRead {

// ============================================================================
// OpdsBookCardWidget - Item display card for a book publication
// ============================================================================

class OpdsBookCardWidget : public QFrame {
    Q_OBJECT
public:
    explicit OpdsBookCardWidget(const OpdsBookEntry& entry, bool inLibrary, QWidget* parent = nullptr)
        : QFrame(parent), m_entry(entry), m_inLibrary(inLibrary) {
        setupUi();
    }

    [[nodiscard]] QLabel* coverLabel() const { return m_coverLabel; }
    [[nodiscard]] const OpdsBookEntry& bookEntry() const { return m_entry; }

    void setInLibrary(bool inLib) {
        m_inLibrary = inLib;
        if (m_downloadBtn) {
            if (m_inLibrary) {
                m_downloadBtn->setText(tr("✓ In Library"));
                m_downloadBtn->setStyleSheet(
                    QStringLiteral("QPushButton { background-color: #ECFDF5; color: #059669; "
                                   "border: 1px solid #A7F3D0; border-radius: 6px; padding: 6px 14px; font-weight: 600; font-size: 9pt; }"
                                   "QPushButton:hover { background-color: #D1FAE5; }"));
                m_downloadBtn->setToolTip(tr("This book is in your library. Click to download again if desired."));
            } else {
                m_downloadBtn->setText(tr("⬇ Download & Add to Library"));
                m_downloadBtn->setStyleSheet(
                    QStringLiteral("QPushButton { background-color: #2563EB; color: #FFFFFF; "
                                   "border: none; border-radius: 6px; padding: 6px 14px; font-weight: 600; font-size: 9pt; }"
                                   "QPushButton:hover { background-color: #1D4ED8; }"
                                   "QPushButton:pressed { background-color: #1E40AF; }"));
                m_downloadBtn->setToolTip(tr("Download and import into LunaRead"));
            }
        }
    }

    void setDownloadingState(bool downloading) {
        if (!m_downloadBtn) return;
        m_downloadBtn->setEnabled(!downloading);
        if (downloading) {
            m_downloadBtn->setText(tr("Downloading..."));
        } else {
            setInLibrary(m_inLibrary);
        }
    }

signals:
    void downloadRequested(const QString& formatMime);

private:
    void setupUi() {
        setObjectName(QStringLiteral("OpdsBookCardWidget"));
        setFrameShape(QFrame::StyledPanel);
        setStyleSheet(QStringLiteral(
            "#OpdsBookCardWidget { background-color: #FFFFFF; border: 1px solid #E2E8F0; "
            "border-radius: 8px; } "
            "#OpdsBookCardWidget:hover { border: 1px solid #94A3B8; }"));

        auto* mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(12, 10, 12, 10);
        mainLayout->setSpacing(16);

        // 1. Cover Image Thumbnail
        m_coverLabel = new QLabel(this);
        m_coverLabel->setFixedSize(80, 115);
        m_coverLabel->setAlignment(Qt::AlignCenter);
        m_coverLabel->setStyleSheet(QStringLiteral("border: 1px solid #CBD5E1; border-radius: 5px; background-color: #F1F5F9;"));
        paintFallbackCover(m_coverLabel, m_entry.title, m_entry.author);
        mainLayout->addWidget(m_coverLabel);

        // 2. Middle Column: Metadata & Summary Snippet
        auto* metaLayout = new QVBoxLayout();
        metaLayout->setSpacing(4);

        auto* titleLabel = new QLabel(m_entry.title, this);
        titleLabel->setWordWrap(true);
        titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        titleLabel->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #0F172A;"));
        metaLayout->addWidget(titleLabel);

        QString authorText = m_entry.author.isEmpty() ? tr("Unknown Author") : m_entry.author;
        auto* authorLabel = new QLabel(authorText, this);
        authorLabel->setStyleSheet(QStringLiteral("font-size: 9.5pt; color: #475569; font-style: italic;"));
        metaLayout->addWidget(authorLabel);

        // Category Tags
        QStringList catLabels = m_entry.categoryLabels();
        if (!catLabels.isEmpty()) {
            QString catString = catLabels.mid(0, 3).join(QStringLiteral("  •  "));
            auto* catLabel = new QLabel(catString, this);
            catLabel->setStyleSheet(QStringLiteral("font-size: 8.5pt; color: #4F46E5; font-weight: 500;"));
            metaLayout->addWidget(catLabel);
        }

        // Summary Snippet (Clean HTML markup to plain text)
        QString rawSummary = m_entry.summary.isEmpty() ? m_entry.content : m_entry.summary;
        QString cleanSnippet = QTextDocumentFragment::fromHtml(rawSummary).toPlainText().trimmed();
        cleanSnippet.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        if (cleanSnippet.length() > 220) {
            cleanSnippet = cleanSnippet.left(220).trimmed() + QStringLiteral("...");
        }
        if (cleanSnippet.isEmpty()) {
            cleanSnippet = tr("No summary provided by catalog.");
        }

        auto* summaryLabel = new QLabel(cleanSnippet, this);
        summaryLabel->setWordWrap(true);
        summaryLabel->setStyleSheet(QStringLiteral("font-size: 9pt; color: #64748B; line-height: 1.3;"));
        metaLayout->addWidget(summaryLabel);
        metaLayout->addStretch(1);

        mainLayout->addLayout(metaLayout, 1);

        // 3. Right Column: Format Selector & Download Action
        auto* actionLayout = new QVBoxLayout();
        actionLayout->setSpacing(8);
        actionLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // Format selector if multiple formats are offered
        if (m_entry.acquisitionLinks.size() > 1) {
            m_formatCombo = new QComboBox(this);
            m_formatCombo->setFixedHeight(28);
            m_formatCombo->setStyleSheet(QStringLiteral(
                "QComboBox { border: 1px solid #CBD5E1; border-radius: 4px; padding: 2px 8px; font-size: 8.5pt; }"));

            int defaultIndex = 0;
            for (qsizetype i = 0; i < m_entry.acquisitionLinks.size(); ++i) {
                const auto& link = m_entry.acquisitionLinks[i];
                QString label = friendlyFormatName(link.type, link.title);
                if (link.length > 0) {
                    double mb = static_cast<double>(link.length) / (1024.0 * 1024.0);
                    label += QStringLiteral(" (%1 MB)").arg(mb, 0, 'f', 1);
                }
                m_formatCombo->addItem(label, link.type);
                if (link.type.contains(QStringLiteral("epub"), Qt::CaseInsensitive)) {
                    defaultIndex = static_cast<int>(i);
                }
            }
            m_formatCombo->setCurrentIndex(defaultIndex);
            actionLayout->addWidget(m_formatCombo);
        } else if (!m_entry.acquisitionLinks.isEmpty()) {
            QString fmt = friendlyFormatName(m_entry.acquisitionLinks.first().type, m_entry.acquisitionLinks.first().title);
            auto* formatBadge = new QLabel(fmt, this);
            formatBadge->setAlignment(Qt::AlignCenter);
            formatBadge->setStyleSheet(QStringLiteral(
                "background-color: #EEF2FF; color: #4338CA; border: 1px solid #C7D2FE; "
                "border-radius: 4px; padding: 3px 8px; font-size: 8pt; font-weight: 600;"));
            actionLayout->addWidget(formatBadge);
        }

        m_downloadBtn = new QPushButton(this);
        m_downloadBtn->setFixedHeight(32);
        setInLibrary(m_inLibrary);

        connect(m_downloadBtn, &QPushButton::clicked, this, [this]() {
            QString selectedMime;
            if (m_formatCombo) {
                selectedMime = m_formatCombo->currentData().toString();
            } else if (!m_entry.acquisitionLinks.isEmpty()) {
                selectedMime = m_entry.acquisitionLinks.first().type;
            }
            setDownloadingState(true);
            emit downloadRequested(selectedMime);
        });

        actionLayout->addWidget(m_downloadBtn);
        mainLayout->addLayout(actionLayout);
    }

    void paintFallbackCover(QLabel* label, const QString& title, const QString& author) {
        QPixmap pm(80, 115);
        pm.fill(Qt::transparent);

        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);

        QRect rect(0, 0, 80, 115);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect), 5.0, 5.0);

        const uint h = qHash(title);
        const int hue = static_cast<int>(h % 360);
        const QColor c1 = QColor::fromHsv(hue, 140, 75);
        const QColor c2 = QColor::fromHsv((hue + 40) % 360, 180, 45);

        QLinearGradient grad(rect.topLeft(), rect.bottomRight());
        grad.setColorAt(0.0, c1);
        grad.setColorAt(1.0, c2);
        p.fillPath(path, grad);

        // Spine highlight
        p.setPen(QColor(255, 255, 255, 45));
        p.drawLine(6, 0, 6, 115);

        // Title
        p.setPen(Qt::white);
        QFont f = label->font();
        f.setBold(true);
        f.setPointSize(8);
        p.setFont(f);
        QFontMetrics fm(f);
        QRect textRect = rect.adjusted(10, 10, -8, -35);
        p.drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, fm.elidedText(title, Qt::ElideRight, 68));

        // Author
        if (!author.isEmpty()) {
            f.setBold(false);
            f.setPointSize(7);
            p.setFont(f);
            p.setPen(QColor(255, 255, 255, 200));
            QRect authorRect = rect.adjusted(10, 85, -8, -6);
            QFontMetrics fma(f);
            p.drawText(authorRect, Qt::AlignBottom | Qt::AlignLeft, fma.elidedText(author, Qt::ElideRight, 64));
        }

        p.end();
        label->setPixmap(pm);
    }

    static QString friendlyFormatName(const QString& mimeType, const QString& explicitTitle) {
        if (!explicitTitle.isEmpty() && explicitTitle.length() < 24) {
            return explicitTitle;
        }
        QString m = mimeType.toLower();
        if (m.contains(QStringLiteral("epub"))) return QStringLiteral("EPUB");
        if (m.contains(QStringLiteral("pdf"))) return QStringLiteral("PDF");
        if (m.contains(QStringLiteral("mobi")) || m.contains(QStringLiteral("kindle")) || m.contains(QStringLiteral("amazon"))) return QStringLiteral("MOBI");
        if (m.contains(QStringLiteral("fb2"))) return QStringLiteral("FB2");
        if (m.contains(QStringLiteral("cbz"))) return QStringLiteral("CBZ");
        if (m.contains(QStringLiteral("plain"))) return QStringLiteral("TXT");
        return QStringLiteral("E-Book");
    }

    OpdsBookEntry m_entry;
    QLabel* m_coverLabel = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QPushButton* m_downloadBtn = nullptr;
    bool m_inLibrary = false;
};

// ============================================================================
// OpdsBrowserDialog Implementation
// ============================================================================

OpdsBrowserDialog::OpdsBrowserDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("OPDS Catalog Browser - LunaRead"));
    resize(1120, 720);
    setMinimumSize(840, 520);

    m_client = new OpdsClient(this);
    m_thumbNet = new QNetworkAccessManager(this);

    connect(m_client, &OpdsClient::feedLoaded, this, &OpdsBrowserDialog::onFeedLoaded);
    connect(m_client, &OpdsClient::downloadProgress, this, &OpdsBrowserDialog::onDownloadProgress);
    connect(m_client, &OpdsClient::downloadFinished, this, &OpdsBrowserDialog::onDownloadFinished);
    connect(m_client, &OpdsClient::errorOccurred, this, &OpdsBrowserDialog::onClientError);

    setupUi();
    setupStyles();

    // Default to Standard Ebooks
    m_catalogCombo->setCurrentIndex(1);
}

OpdsBrowserDialog::~OpdsBrowserDialog() {
    if (m_client) {
        m_client->cancelAllRequests();
    }
}

void OpdsBrowserDialog::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 16, 18, 16);
    rootLayout->setSpacing(12);

    // ------------------------------------------------------------------------
    // Top Bar 1: Catalog Preset Selection & Custom URL Address Bar
    // ------------------------------------------------------------------------
    auto* topRowLayout = new QHBoxLayout();
    topRowLayout->setSpacing(10);

    auto* catLabel = new QLabel(tr("Catalog:"), this);
    catLabel->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 10pt; color: #1E293B;"));
    topRowLayout->addWidget(catLabel);

    m_catalogCombo = new QComboBox(this);
    m_catalogCombo->setFixedHeight(34);
    m_catalogCombo->setMinimumWidth(180);
    m_catalogCombo->addItem(tr("Project Gutenberg"), QStringLiteral("https://m.gutenberg.org/ebooks/?format=opds"));
    m_catalogCombo->addItem(tr("Standard Ebooks"), QStringLiteral("https://standardebooks.org/opds"));
    m_catalogCombo->addItem(tr("Feedbooks"), QStringLiteral("https://catalog.feedbooks.com/catalog/public_domain.atom"));
    m_catalogCombo->insertSeparator(m_catalogCombo->count());
    m_catalogCombo->addItem(tr("+ Add Custom Feed URL"), QStringLiteral("__custom__"));
    connect(m_catalogCombo, &QComboBox::currentIndexChanged, this, &OpdsBrowserDialog::onCatalogSelectionChanged);
    topRowLayout->addWidget(m_catalogCombo);

    // History Navigation Buttons
    m_backBtn = new QPushButton(QStringLiteral("◀"), this);
    m_backBtn->setFixedSize(34, 34);
    m_backBtn->setEnabled(false);
    m_backBtn->setToolTip(tr("Go Back"));
    connect(m_backBtn, &QPushButton::clicked, this, &OpdsBrowserDialog::onBackClicked);
    topRowLayout->addWidget(m_backBtn);

    m_forwardBtn = new QPushButton(QStringLiteral("▶"), this);
    m_forwardBtn->setFixedSize(34, 34);
    m_forwardBtn->setEnabled(false);
    m_forwardBtn->setToolTip(tr("Go Forward"));
    connect(m_forwardBtn, &QPushButton::clicked, this, &OpdsBrowserDialog::onForwardClicked);
    topRowLayout->addWidget(m_forwardBtn);

    // Address Bar
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setFixedHeight(34);
    m_urlEdit->setPlaceholderText(tr("Enter OPDS catalog URL..."));
    m_urlEdit->setClearButtonEnabled(true);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &OpdsBrowserDialog::onGoRefreshClicked);
    topRowLayout->addWidget(m_urlEdit, 1);

    m_goBtn = new QPushButton(tr("Go / Refresh"), this);
    m_goBtn->setFixedHeight(34);
    connect(m_goBtn, &QPushButton::clicked, this, &OpdsBrowserDialog::onGoRefreshClicked);
    topRowLayout->addWidget(m_goBtn);

    rootLayout->addLayout(topRowLayout);

    // ------------------------------------------------------------------------
    // Top Bar 2: Catalog Search Box
    // ------------------------------------------------------------------------
    auto* searchRowLayout = new QHBoxLayout();
    searchRowLayout->setSpacing(10);

    auto* searchLabel = new QLabel(tr("Search:"), this);
    searchLabel->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 10pt; color: #1E293B;"));
    searchRowLayout->addWidget(searchLabel);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setFixedHeight(32);
    m_searchEdit->setPlaceholderText(tr("Search catalog books by title, author, or keyword..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &OpdsBrowserDialog::onSearchTriggered);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &OpdsBrowserDialog::onSearchTextChanged);
    searchRowLayout->addWidget(m_searchEdit, 1);

    m_searchBtn = new QPushButton(tr("Search"), this);
    m_searchBtn->setFixedHeight(32);
    connect(m_searchBtn, &QPushButton::clicked, this, &OpdsBrowserDialog::onSearchTriggered);
    searchRowLayout->addWidget(m_searchBtn);

    rootLayout->addLayout(searchRowLayout);

    // ------------------------------------------------------------------------
    // Center Splitter: Navigation Tree (Left) and Book Publication Cards (Right)
    // ------------------------------------------------------------------------
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // Left Navigation Tree
    auto* navContainer = new QWidget(m_splitter);
    auto* navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(8);

    auto* navTitleLabel = new QLabel(tr("Catalog Navigation"), navContainer);
    navTitleLabel->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 10.5pt; color: #334155; padding-bottom: 2px;"));
    navLayout->addWidget(navTitleLabel);

    m_navTree = new QTreeWidget(navContainer);
    m_navTree->setHeaderHidden(true);
    m_navTree->setAnimated(true);
    connect(m_navTree, &QTreeWidget::itemClicked, this, &OpdsBrowserDialog::onNavTreeItemActivated);
    connect(m_navTree, &QTreeWidget::itemActivated, this, &OpdsBrowserDialog::onNavTreeItemActivated);
    navLayout->addWidget(m_navTree, 1);
    m_splitter->addWidget(navContainer);

    // Right Books Section
    auto* booksContainer = new QWidget(m_splitter);
    auto* booksContainerLayout = new QVBoxLayout(booksContainer);
    booksContainerLayout->setContentsMargins(0, 0, 0, 0);
    booksContainerLayout->setSpacing(8);

    // Feed Header Info
    auto* headerWidget = new QWidget(booksContainer);
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(4, 0, 4, 4);

    auto* titleSubLayout = new QVBoxLayout();
    titleSubLayout->setSpacing(2);

    m_feedTitleLabel = new QLabel(tr("Exploring Catalog"), headerWidget);
    m_feedTitleLabel->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 13pt; color: #0F172A;"));
    titleSubLayout->addWidget(m_feedTitleLabel);

    m_feedSubtitleLabel = new QLabel(headerWidget);
    m_feedSubtitleLabel->setStyleSheet(QStringLiteral("font-size: 9pt; color: #64748B;"));
    m_feedSubtitleLabel->setWordWrap(true);
    titleSubLayout->addWidget(m_feedSubtitleLabel);
    headerLayout->addLayout(titleSubLayout, 1);

    m_feedCountLabel = new QLabel(headerWidget);
    m_feedCountLabel->setStyleSheet(QStringLiteral("font-size: 9pt; color: #475569; font-weight: 600;"));
    headerLayout->addWidget(m_feedCountLabel, 0, Qt::AlignRight | Qt::AlignTop);

    booksContainerLayout->addWidget(headerWidget);

    // Scroll Area for Book Cards
    m_bookScrollArea = new QScrollArea(booksContainer);
    m_bookScrollArea->setWidgetResizable(true);
    m_bookScrollArea->setStyleSheet(QStringLiteral("QScrollArea { border: 1px solid #E2E8F0; border-radius: 8px; background-color: #F8FAFC; }"));

    m_bookContainer = new QWidget(m_bookScrollArea);
    m_bookLayout = new QVBoxLayout(m_bookContainer);
    m_bookLayout->setContentsMargins(12, 12, 12, 12);
    m_bookLayout->setSpacing(10);
    m_bookScrollArea->setWidget(m_bookContainer);

    booksContainerLayout->addWidget(m_bookScrollArea, 1);

    // Pagination Controls
    auto* paginationLayout = new QHBoxLayout();
    paginationLayout->setContentsMargins(4, 4, 4, 0);

    m_prevPageBtn = new QPushButton(tr("◀ Previous Page"), booksContainer);
    m_prevPageBtn->setFixedHeight(30);
    m_prevPageBtn->setEnabled(false);
    connect(m_prevPageBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentFeed.hasPrev()) {
            navigateTo(m_currentFeed.prevUrl);
        }
    });
    paginationLayout->addWidget(m_prevPageBtn);

    m_pageStatusLabel = new QLabel(booksContainer);
    m_pageStatusLabel->setAlignment(Qt::AlignCenter);
    m_pageStatusLabel->setStyleSheet(QStringLiteral("font-size: 9pt; color: #475569;"));
    paginationLayout->addWidget(m_pageStatusLabel, 1);

    m_nextPageBtn = new QPushButton(tr("Next Page ▶"), booksContainer);
    m_nextPageBtn->setFixedHeight(30);
    m_nextPageBtn->setEnabled(false);
    connect(m_nextPageBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentFeed.hasNext()) {
            navigateTo(m_currentFeed.nextUrl);
        }
    });
    paginationLayout->addWidget(m_nextPageBtn);

    booksContainerLayout->addLayout(paginationLayout);

    m_splitter->addWidget(booksContainer);
    m_splitter->setSizes({260, 800});
    rootLayout->addWidget(m_splitter, 1);

    // ------------------------------------------------------------------------
    // Bottom Bar: Progress Bar, Status Info & Close Button
    // ------------------------------------------------------------------------
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setTextVisible(true);
    m_progressBar->setVisible(false);
    bottomLayout->addWidget(m_progressBar, 1);

    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 9pt; color: #475569;"));
    bottomLayout->addWidget(m_statusLabel, 2);

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setFixedSize(90, 32);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_closeBtn);

    rootLayout->addLayout(bottomLayout);
}

void OpdsBrowserDialog::setupStyles() {
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #F8FAFC; }"
        "QLineEdit { border: 1px solid #CBD5E1; border-radius: 6px; padding: 0 10px; background-color: #FFFFFF; font-size: 10pt; }"
        "QLineEdit:focus { border: 1px solid #3B82F6; }"
        "QComboBox { border: 1px solid #CBD5E1; border-radius: 6px; padding: 0 10px; background-color: #FFFFFF; font-size: 10pt; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QPushButton { border: 1px solid #CBD5E1; border-radius: 6px; padding: 0 14px; background-color: #FFFFFF; color: #0F172A; font-size: 9.5pt; font-weight: 500; }"
        "QPushButton:hover { background-color: #F1F5F9; border-color: #94A3B8; }"
        "QPushButton:pressed { background-color: #E2E8F0; }"
        "QTreeWidget { border: 1px solid #E2E8F0; border-radius: 8px; background-color: #FFFFFF; font-size: 9.5pt; padding: 6px; }"
        "QTreeWidget::item { height: 28px; padding-left: 4px; border-radius: 4px; }"
        "QTreeWidget::item:hover { background-color: #F1F5F9; }"
        "QTreeWidget::item:selected { background-color: #E0E7FF; color: #3730A3; font-weight: 600; }"
        "QProgressBar { border: 1px solid #CBD5E1; border-radius: 4px; text-align: center; font-size: 8pt; background-color: #E2E8F0; }"
        "QProgressBar::chunk { background-color: #3B82F6; border-radius: 3px; }"
    ));

    m_goBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2563EB; color: #FFFFFF; border: none; font-weight: 600; }"
        "QPushButton:hover { background-color: #1D4ED8; }"
        "QPushButton:pressed { background-color: #1E40AF; }"
    ));

    m_searchBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0EA5E9; color: #FFFFFF; border: none; font-weight: 600; }"
        "QPushButton:hover { background-color: #0284C7; }"
        "QPushButton:pressed { background-color: #0369A1; }"
    ));
}

void OpdsBrowserDialog::loadCatalog(const QString& catalogUrl) {
    navigateTo(catalogUrl, true);
}

void OpdsBrowserDialog::navigateTo(const QString& targetUrl, bool addToHistory) {
    if (targetUrl.isEmpty()) return;

    if (addToHistory) {
        while (m_history.size() > m_historyIndex + 1) {
            m_history.removeLast();
        }
        m_history.append(targetUrl);
        m_historyIndex = static_cast<int>(m_history.size()) - 1;
    }

    m_backBtn->setEnabled(m_historyIndex > 0);
    m_forwardBtn->setEnabled(m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size());

    m_urlEdit->setText(targetUrl);
    m_statusLabel->setText(tr("Fetching catalog feed: %1").arg(targetUrl));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate loading spinner

    m_client->fetchCatalog(targetUrl);
}

void OpdsBrowserDialog::onCatalogSelectionChanged(int index) {
    if (index < 0) return;

    QString data = m_catalogCombo->itemData(index).toString();
    if (data == QStringLiteral("__custom__")) {
        bool ok = false;
        QString url = QInputDialog::getText(this, tr("Add Custom Catalog"),
                                            tr("Enter OPDS Feed URL:"),
                                            QLineEdit::Normal, QStringLiteral("https://"), &ok);
        if (ok && !url.trimmed().isEmpty()) {
            QString cleanUrl = url.trimmed();
            bool okName = false;
            QString name = QInputDialog::getText(this, tr("Add Custom Catalog"),
                                                 tr("Catalog Name:"),
                                                 QLineEdit::Normal, cleanUrl, &okName);
            QString cleanName = name.trimmed().isEmpty() ? cleanUrl : name.trimmed();

            int insertIdx = std::max(0, m_catalogCombo->count() - 2);
            m_catalogCombo->insertItem(insertIdx, cleanName, cleanUrl);
            m_catalogCombo->setCurrentIndex(insertIdx);
        } else {
            // Revert back to previous valid item
            for (int i = 0; i < m_catalogCombo->count(); ++i) {
                if (m_catalogCombo->itemData(i).toString() == m_urlEdit->text()) {
                    QSignalBlocker blocker(m_catalogCombo);
                    m_catalogCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    } else if (!data.isEmpty()) {
        navigateTo(data, true);
    }
}

void OpdsBrowserDialog::onGoRefreshClicked() {
    QString url = m_urlEdit->text().trimmed();
    if (!url.isEmpty()) {
        navigateTo(url, true);
    }
}

void OpdsBrowserDialog::onBackClicked() {
    if (m_historyIndex > 0) {
        m_historyIndex--;
        navigateTo(m_history[m_historyIndex], false);
    }
}

void OpdsBrowserDialog::onForwardClicked() {
    if (m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size()) {
        m_historyIndex++;
        navigateTo(m_history[m_historyIndex], false);
    }
}

void OpdsBrowserDialog::onSearchTriggered() {
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) {
        populateBookList(m_currentBooks);
        return;
    }

    // 1. If feed provides an OpenSearch template searchUrl
    QString sUrl = m_currentFeed.searchUrl;
    if (!sUrl.isEmpty()) {
        if (sUrl.contains(QLatin1String("{searchTerms}"))) {
            sUrl.replace(QLatin1String("{searchTerms}"), QUrl::toPercentEncoding(query));
        } else if (sUrl.contains(QLatin1String("{query}"))) {
            sUrl.replace(QLatin1String("{query}"), QUrl::toPercentEncoding(query));
        } else {
            sUrl += (sUrl.contains('?') ? QStringLiteral("&query=") : QStringLiteral("?query=")) + QUrl::toPercentEncoding(query);
        }
        navigateTo(sUrl, true);
        return;
    }

    // 2. Client-side local filtering if no remote search template is exposed
    QVector<OpdsBookEntry> filtered;
    for (const auto& b : m_currentBooks) {
        if (b.title.contains(query, Qt::CaseInsensitive) ||
            b.author.contains(query, Qt::CaseInsensitive) ||
            b.summary.contains(query, Qt::CaseInsensitive) ||
            b.categoryLabels().join(QLatin1Char(' ')).contains(query, Qt::CaseInsensitive)) {
            filtered.append(b);
        }
    }
    populateBookList(filtered);
    m_statusLabel->setText(tr("Found %1 matching books").arg(filtered.size()));
}

void OpdsBrowserDialog::onSearchTextChanged(const QString& query) {
    if (query.trimmed().isEmpty()) {
        populateBookList(m_currentBooks);
    }
}

void OpdsBrowserDialog::onNavTreeItemActivated(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    QString targetUrl = item->data(0, Qt::UserRole).toString();
    if (!targetUrl.isEmpty()) {
        navigateTo(targetUrl, true);
    }
}

void OpdsBrowserDialog::onFeedLoaded(const OpdsFeed& feed) {
    m_currentFeed = feed;
    m_progressBar->setVisible(false);

    // Update Header labels
    m_feedTitleLabel->setText(feed.title.isEmpty() ? tr("Catalog") : feed.title);
    m_feedSubtitleLabel->setText(feed.subtitle);
    m_feedSubtitleLabel->setVisible(!feed.subtitle.isEmpty());

    // Update address bar
    if (!feed.selfUrl.isEmpty()) {
        m_urlEdit->setText(feed.selfUrl);
    }

    // Sync catalog combo selection if matches
    for (int i = 0; i < m_catalogCombo->count(); ++i) {
        if (m_catalogCombo->itemData(i).toString() == m_urlEdit->text()) {
            QSignalBlocker blocker(m_catalogCombo);
            m_catalogCombo->setCurrentIndex(i);
            break;
        }
    }

    // Filter books from navigation items
    m_currentBooks.clear();
    for (const auto& entry : feed.entries) {
        if (!entry.isNavigation && !entry.acquisitionLinks.isEmpty()) {
            m_currentBooks.append(entry);
        }
    }

    // If no acquisition links were marked on entries, treat all non-navigation entries as books
    if (m_currentBooks.isEmpty() && !feed.entries.isEmpty()) {
        for (const auto& entry : feed.entries) {
            if (!entry.isNavigation) {
                m_currentBooks.append(entry);
            }
        }
    }

    m_feedCountLabel->setText(tr("%1 books").arg(m_currentBooks.size()));

    populateNavigationTree(feed);
    populateBookList(m_currentBooks);
    updatePaginationControls();

    m_statusLabel->setText(tr("Loaded: %1 (%2 items)").arg(feed.title, QString::number(feed.entries.size())));
}

void OpdsBrowserDialog::populateNavigationTree(const OpdsFeed& feed) {
    m_navTree->clear();

    // Home / Root link
    if (!feed.startUrl.isEmpty() && feed.startUrl != feed.selfUrl) {
        auto* rootItem = new QTreeWidgetItem(m_navTree);
        rootItem->setText(0, tr("🏠 Catalog Home"));
        rootItem->setData(0, Qt::UserRole, feed.startUrl);
    }

    // Parent / Up link
    if (!feed.upUrl.isEmpty()) {
        auto* upItem = new QTreeWidgetItem(m_navTree);
        upItem->setText(0, tr("⬆ Up / Parent"));
        upItem->setData(0, Qt::UserRole, feed.upUrl);
    }

    // Subsections & Categories
    if (!feed.navigationLinks.isEmpty()) {
        auto* catHeader = new QTreeWidgetItem(m_navTree);
        catHeader->setText(0, tr("Categories & Sections"));
        catHeader->setExpanded(true);

        for (const auto& nav : feed.navigationLinks) {
            auto* item = new QTreeWidgetItem(catHeader);
            QString label = nav.title.isEmpty() ? tr("Section") : nav.title;
            if (nav.count > 0) {
                label += QStringLiteral(" (%1)").arg(nav.count);
            }
            item->setText(0, label);
            item->setToolTip(0, nav.summary.isEmpty() ? nav.href : nav.summary);
            item->setData(0, Qt::UserRole, nav.href);
        }
    } else {
        // Fallback: check entries where isNavigation is true
        QVector<OpdsBookEntry> navEntries;
        for (const auto& entry : feed.entries) {
            if (entry.isNavigation) {
                navEntries.append(entry);
            }
        }

        if (!navEntries.isEmpty()) {
            auto* catHeader = new QTreeWidgetItem(m_navTree);
            catHeader->setText(0, tr("Navigation Sections"));
            catHeader->setExpanded(true);

            for (const auto& entry : navEntries) {
                auto* item = new QTreeWidgetItem(catHeader);
                item->setText(0, entry.title.isEmpty() ? tr("Section") : entry.title);
                QString href;
                for (const auto& link : entry.links) {
                    if (link.isNavigation() || !link.href.isEmpty()) {
                        href = link.href;
                        break;
                    }
                }
                item->setData(0, Qt::UserRole, href);
            }
        }
    }
}

void OpdsBrowserDialog::populateBookList(const QVector<OpdsBookEntry>& books) {
    // Clear previous card widgets
    QLayoutItem* item;
    while ((item = m_bookLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    if (books.isEmpty()) {
        auto* emptyLabel = new QLabel(m_bookContainer);
        if (m_currentFeed.isNavigationFeed() || !m_currentFeed.navigationLinks.isEmpty()) {
            emptyLabel->setText(tr("This catalog section contains subcategories.\n"
                                   "Please select a category from the Catalog Navigation panel on the left."));
        } else {
            emptyLabel->setText(tr("No book publications found in this catalog feed."));
        }
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QStringLiteral("color: #64748B; font-size: 11pt; padding: 48px; line-height: 1.6;"));
        m_bookLayout->addWidget(emptyLabel, 0, Qt::AlignCenter);
        return;
    }

    for (const auto& book : books) {
        bool inLib = isBookInLibrary(book.title, book.author);
        auto* card = new OpdsBookCardWidget(book, inLib, m_bookContainer);

        connect(card, &OpdsBookCardWidget::downloadRequested, this, [this, book](const QString& formatMime) {
            onDownloadRequested(book, formatMime);
        });

        // Asynchronously request cover thumbnail if available
        if (!book.thumbnailUrl.isEmpty()) {
            requestThumbnail(book.thumbnailUrl, card->coverLabel());
        } else if (!book.coverUrl.isEmpty()) {
            requestThumbnail(book.coverUrl, card->coverLabel());
        }

        m_bookLayout->addWidget(card);
    }

    m_bookLayout->addStretch(1);
}

void OpdsBrowserDialog::updatePaginationControls() {
    bool hasPrev = m_currentFeed.hasPrev();
    bool hasNext = m_currentFeed.hasNext();

    m_prevPageBtn->setEnabled(hasPrev);
    m_nextPageBtn->setEnabled(hasNext);

    if (hasPrev || hasNext) {
        m_prevPageBtn->setVisible(true);
        m_nextPageBtn->setVisible(true);
        m_pageStatusLabel->setVisible(true);
        m_pageStatusLabel->setText(tr("Multi-page catalog feed"));
    } else {
        m_prevPageBtn->setVisible(false);
        m_nextPageBtn->setVisible(false);
        m_pageStatusLabel->setVisible(false);
    }
}

void OpdsBrowserDialog::requestThumbnail(const QString& imageUrl, QLabel* targetLabel) {
    if (imageUrl.isEmpty() || !targetLabel) return;

    if (m_thumbnailCache.contains(imageUrl)) {
        const QPixmap& pm = m_thumbnailCache[imageUrl];
        targetLabel->setPixmap(pm.scaled(targetLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }

    QPointer<QLabel> weakLabel = targetLabel;
    QNetworkRequest request(QUrl(imageUrl));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LunaRead-Qt/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_thumbNet->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, imageUrl, weakLabel]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pm;
            if (pm.loadFromData(data) && !pm.isNull()) {
                m_thumbnailCache.insert(imageUrl, pm);
                if (weakLabel) {
                    weakLabel->setPixmap(pm.scaled(weakLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            }
        }
    });
}

void OpdsBrowserDialog::onDownloadRequested(const OpdsBookEntry& entry, const QString& formatMime) {
    QString downloadUrl;
    if (!formatMime.isEmpty()) {
        downloadUrl = entry.acquisitionUrlForType(formatMime);
    }
    if (downloadUrl.isEmpty()) {
        downloadUrl = entry.bestDownloadUrl();
    }

    if (downloadUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Download Unavailable"),
                             tr("No direct download link was found for '%1'.").arg(entry.title));
        return;
    }

    QString ext = determineExtension(downloadUrl, formatMime);
    QString filename = sanitizeFilename(entry.title.isEmpty() ? QStringLiteral("ebook") : entry.title, ext);
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/books");
    QDir().mkpath(dir);

    QString destPath = dir + QStringLiteral("/") + filename;

    OpdsDownloadTask task;
    task.entry = entry;
    task.destinationPath = destPath;
    task.format = ext;
    m_pendingDownloads.insert(destPath, task);

    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Downloading '%1'...").arg(entry.title));

    m_client->downloadBook(downloadUrl, destPath);
}

void OpdsBrowserDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(percent);

        double recvMb = static_cast<double>(bytesReceived) / (1024.0 * 1024.0);
        double totalMb = static_cast<double>(bytesTotal) / (1024.0 * 1024.0);
        m_statusLabel->setText(tr("Downloading... %1 MB / %2 MB (%3%)")
                                   .arg(recvMb, 0, 'f', 1)
                                   .arg(totalMb, 0, 'f', 1)
                                   .arg(percent));
    } else {
        m_progressBar->setRange(0, 0);
        double recvMb = static_cast<double>(bytesReceived) / (1024.0 * 1024.0);
        m_statusLabel->setText(tr("Downloading... %1 MB").arg(recvMb, 0, 'f', 1));
    }
}

void OpdsBrowserDialog::onDownloadFinished(const QString& localFilePath) {
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);

    if (!m_pendingDownloads.contains(localFilePath)) {
        m_statusLabel->setText(tr("Download complete: %1").arg(QFileInfo(localFilePath).fileName()));
        return;
    }

    OpdsDownloadTask task = m_pendingDownloads.take(localFilePath);
    const OpdsBookEntry& entry = task.entry;
    QFileInfo fi(localFilePath);

    BookItem item;

    // Parse book with ParserFactory
    auto parsedDoc = ParserFactory::parse(localFilePath);
    if (parsedDoc.has_value()) {
        item.id = parsedDoc->id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : parsedDoc->id;
        item.title = parsedDoc->title.isEmpty() ? (entry.title.isEmpty() ? fi.baseName() : entry.title) : parsedDoc->title;
        item.author = parsedDoc->author.isEmpty() ? (entry.author.isEmpty() ? tr("Unknown Author") : entry.author) : parsedDoc->author;
        item.format = parsedDoc->format.isEmpty() ? fi.suffix().toLower() : parsedDoc->format.toLower();
        item.totalChapters = parsedDoc->totalChapters;

        if (!parsedDoc->coverImageBytes.isEmpty()) {
            QString coverDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/covers");
            QDir().mkpath(coverDir);
            QString coverPath = coverDir + QStringLiteral("/") + item.id + QStringLiteral(".jpg");
            QFile coverFile(coverPath);
            if (coverFile.open(QIODevice::WriteOnly)) {
                coverFile.write(parsedDoc->coverImageBytes);
                coverFile.close();
                item.coverImagePath = coverPath;
            }
        }
    } else {
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.title = entry.title.isEmpty() ? fi.baseName() : entry.title;
        item.author = entry.author.isEmpty() ? tr("Unknown Author") : entry.author;
        item.format = fi.suffix().toLower();
        item.totalChapters = 1;
    }

    // Fallback: If coverImagePath is still empty, and we cached thumbnail, save it
    if (item.coverImagePath.isEmpty() && m_thumbnailCache.contains(entry.thumbnailUrl)) {
        QString coverDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/covers");
        QDir().mkpath(coverDir);
        QString coverPath = coverDir + QStringLiteral("/") + item.id + QStringLiteral(".jpg");
        if (m_thumbnailCache[entry.thumbnailUrl].save(coverPath, "JPEG")) {
            item.coverImagePath = coverPath;
        }
    }

    item.filePath = localFilePath;
    item.fileSize = fi.size();
    item.fileHash = item.id;
    item.dateAdded = QDateTime::currentMSecsSinceEpoch();
    item.lastReadTime = item.dateAdded;

    // Ensure database connection is initialized
    if (!DatabaseManager::instance().isOpen()) {
        DatabaseManager::instance().initialize();
    }

    bool inserted = DatabaseManager::instance().insertBook(item);
    if (inserted) {
        m_statusLabel->setText(tr("✓ Successfully added '%1' to Bookshelf!").arg(item.title));
        emit bookImported(item.id);

        // Update card in book container
        for (int i = 0; i < m_bookLayout->count(); ++i) {
            auto* w = m_bookLayout->itemAt(i)->widget();
            if (auto* card = qobject_cast<OpdsBookCardWidget*>(w)) {
                if (card->bookEntry().title == entry.title) {
                    card->setInLibrary(true);
                }
            }
        }
    } else {
        m_statusLabel->setText(tr("Downloaded '%1', but database insertion failed.").arg(item.title));
    }
}

void OpdsBrowserDialog::onClientError(const QString& errorMessage) {
    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("Error: %1").arg(errorMessage));
}

QString OpdsBrowserDialog::sanitizeFilename(const QString& rawTitle, const QString& extension) const {
    QString safe = rawTitle;
    static const QRegularExpression rx(QStringLiteral("[^a-zA-Z0-9 _-]"));
    safe.replace(rx, QString());
    safe = safe.trimmed();
    if (safe.isEmpty()) {
        safe = QStringLiteral("book_") + QString::number(QDateTime::currentMSecsSinceEpoch());
    }
    if (safe.length() > 60) {
        safe = safe.left(60).trimmed();
    }
    return safe + QStringLiteral(".") + extension;
}

QString OpdsBrowserDialog::determineExtension(const QString& fileUrl, const QString& mimeType) const {
    QString mime = mimeType.toLower();
    if (mime.contains(QStringLiteral("epub"))) return QStringLiteral("epub");
    if (mime.contains(QStringLiteral("pdf"))) return QStringLiteral("pdf");
    if (mime.contains(QStringLiteral("mobi")) || mime.contains(QStringLiteral("amazon")) || mime.contains(QStringLiteral("azw"))) return QStringLiteral("mobi");
    if (mime.contains(QStringLiteral("fb2"))) return QStringLiteral("fb2");
    if (mime.contains(QStringLiteral("cbz"))) return QStringLiteral("cbz");
    if (mime.contains(QStringLiteral("plain"))) return QStringLiteral("txt");

    QUrl qurl(fileUrl);
    QString path = qurl.path();
    QString suffix = QFileInfo(path).suffix().toLower();
    if (!suffix.isEmpty() && suffix.length() <= 5) {
        return suffix;
    }
    return QStringLiteral("epub");
}

bool OpdsBrowserDialog::isBookInLibrary(const QString& bookTitle, const QString& bookAuthor) const {
    if (bookTitle.isEmpty()) return false;

    if (!DatabaseManager::instance().isOpen()) {
        return false;
    }

    const auto books = DatabaseManager::instance().getAllBooks();
    for (const auto& b : books) {
        if (b.title.compare(bookTitle, Qt::CaseInsensitive) == 0) {
            if (bookAuthor.isEmpty() || b.author.contains(bookAuthor, Qt::CaseInsensitive) || bookAuthor.contains(b.author, Qt::CaseInsensitive)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace LunaRead

#include "OpdsBrowserDialog.moc"
