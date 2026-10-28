#include "BookshelfView.h"
#include "BookDetailsDialog.h"
#include "AnnotationExportDialog.h"
#include "../db/DatabaseManager.h"
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDateTime>

namespace LunaRead {

BookshelfView::BookshelfView(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // Filter, Search, and View Mode Header
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search books by title or author..."));
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setStyleSheet("QLineEdit { border: 1px solid #D4D4D8; border-radius: 6px; padding: 0 12px; font-size: 11pt; }");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &BookshelfView::refreshBooks);
    headerLayout->addWidget(m_searchEdit, 1);

    m_formatFilter = new QComboBox(this);
    m_formatFilter->setFixedHeight(36);
    m_formatFilter->addItems({"All Formats", "EPUB", "PDF", "TXT", "MOBI", "FB2", "DOCX", "CBZ", "Markdown"});
    connect(m_formatFilter, &QComboBox::currentIndexChanged, this, &BookshelfView::refreshBooks);
    headerLayout->addWidget(m_formatFilter);

    // View Mode Toggle Segmented Control: Grid View (🔲) and List View (☰)
    auto* viewModeFrame = new QFrame(this);
    viewModeFrame->setFixedHeight(36);
    viewModeFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #F1F5F9;"
        "  border: 1px solid #E2E8F0;"
        "  border-radius: 8px;"
        "}"
    );
    auto* viewModeLayout = new QHBoxLayout(viewModeFrame);
    viewModeLayout->setContentsMargins(2, 2, 2, 2);
    viewModeLayout->setSpacing(2);

    m_viewModeGroup = new QButtonGroup(this);
    m_viewModeGroup->setExclusive(true);

    m_gridViewBtn = new QPushButton(QString::fromUtf8("🔲 Grid View"), viewModeFrame);
    m_gridViewBtn->setCheckable(true);
    m_gridViewBtn->setChecked(true);
    m_gridViewBtn->setCursor(Qt::PointingHandCursor);
    m_gridViewBtn->setFixedHeight(30);

    m_listViewBtn = new QPushButton(QString::fromUtf8("☰ List View"), viewModeFrame);
    m_listViewBtn->setCheckable(true);
    m_listViewBtn->setChecked(false);
    m_listViewBtn->setCursor(Qt::PointingHandCursor);
    m_listViewBtn->setFixedHeight(30);

    const QString modeBtnStyle =
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "  color: #64748B;"
        "  font-size: 9.5pt;"
        "  font-weight: 600;"
        "  padding: 4px 10px;"
        "}"
        "QPushButton:hover {"
        "  color: #0F172A;"
        "  background: #E2E8F0;"
        "}"
        "QPushButton:checked {"
        "  background-color: #FFFFFF;"
        "  color: #2563EB;"
        "  font-weight: bold;"
        "  border: 1px solid #CBD5E1;"
        "}";

    m_gridViewBtn->setStyleSheet(modeBtnStyle);
    m_listViewBtn->setStyleSheet(modeBtnStyle);

    m_viewModeGroup->addButton(m_gridViewBtn, 0);
    m_viewModeGroup->addButton(m_listViewBtn, 1);

    viewModeLayout->addWidget(m_gridViewBtn);
    viewModeLayout->addWidget(m_listViewBtn);

    connect(m_viewModeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setListView(id == 1);
    });

    headerLayout->addWidget(viewModeFrame);

    mainLayout->addLayout(headerLayout);

    // Continue Reading Hero Banner
    setupContinueBanner();
    mainLayout->addWidget(m_continueBanner);

    // Scroll Area with Grid / List Container
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    m_gridContainer = new QWidget(m_scrollArea);
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(20);
    m_gridLayout->setAlignment(Qt::AlignTop);
    m_scrollArea->setWidget(m_gridContainer);

    mainLayout->addWidget(m_scrollArea, 1);

    refreshBooks();
}

void BookshelfView::setListView(bool listView) {
    if (m_isListView == listView) return;
    m_isListView = listView;
    if (m_listViewBtn) m_listViewBtn->setChecked(listView);
    if (m_gridViewBtn) m_gridViewBtn->setChecked(!listView);
    refreshBooks();
}

void BookshelfView::setupContinueBanner() {
    m_continueBanner = new QFrame(this);
    m_continueBanner->setFixedHeight(88);
    m_continueBanner->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1E293B, stop:1 #334155);"
        "  border-radius: 10px;"
        "}"
    );

    auto* layout = new QHBoxLayout(m_continueBanner);
    layout->setContentsMargins(18, 12, 18, 12);
    layout->setSpacing(16);

    auto* iconLabel = new QLabel(QStringLiteral("📖"), m_continueBanner);
    iconLabel->setStyleSheet("font-size: 24pt; background: transparent;");
    layout->addWidget(iconLabel);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    auto* headerTag = new QLabel(tr("CONTINUE READING"), m_continueBanner);
    headerTag->setStyleSheet("color: #60A5FA; font-size: 8pt; font-weight: bold; letter-spacing: 1px; background: transparent;");
    textLayout->addWidget(headerTag);

    m_bannerTitleLabel = new QLabel(m_continueBanner);
    m_bannerTitleLabel->setStyleSheet("color: #FFFFFF; font-size: 13pt; font-weight: bold; background: transparent;");
    textLayout->addWidget(m_bannerTitleLabel);

    m_bannerAuthorLabel = new QLabel(m_continueBanner);
    m_bannerAuthorLabel->setStyleSheet("color: #94A3B8; font-size: 9pt; background: transparent;");
    textLayout->addWidget(m_bannerAuthorLabel);

    layout->addLayout(textLayout, 1);

    auto* progressLayout = new QVBoxLayout();
    progressLayout->setSpacing(4);
    progressLayout->setAlignment(Qt::AlignCenter);

    m_bannerTimeLabel = new QLabel(m_continueBanner);
    m_bannerTimeLabel->setStyleSheet("color: #CBD5E1; font-size: 9pt; background: transparent;");
    progressLayout->addWidget(m_bannerTimeLabel);

    m_bannerProgress = new QProgressBar(m_continueBanner);
    m_bannerProgress->setFixedSize(160, 8);
    m_bannerProgress->setTextVisible(false);
    m_bannerProgress->setStyleSheet(
        "QProgressBar { background: rgba(255, 255, 255, 40); border-radius: 4px; }"
        "QProgressBar::chunk { background: #3B82F6; border-radius: 4px; }"
    );
    progressLayout->addWidget(m_bannerProgress);

    layout->addLayout(progressLayout);

    m_resumeBtn = new QPushButton(tr("▶ Resume Reading"), m_continueBanner);
    m_resumeBtn->setCursor(Qt::PointingHandCursor);
    m_resumeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563EB; color: #FFFFFF; font-weight: bold; font-size: 10pt;"
        "  border: none; border-radius: 8px; padding: 10px 20px;"
        "}"
        "QPushButton:hover { background-color: #1D4ED8; }"
    );
    connect(m_resumeBtn, &QPushButton::clicked, this, [this]() {
        if (!m_resumeBookId.isEmpty()) {
            emit bookSelected(m_resumeBookId);
        }
    });
    layout->addWidget(m_resumeBtn);

    m_continueBanner->hide();
}

void BookshelfView::updateContinueBanner(const QList<BookItem>& allBooks) {
    if (allBooks.isEmpty()) {
        m_continueBanner->hide();
        return;
    }

    // Find book with latest lastReadTime
    const BookItem* mostRecent = nullptr;
    qint64 latestTime = 0;
    for (const auto& b : allBooks) {
        if (b.lastReadTime > latestTime) {
            latestTime = b.lastReadTime;
            mostRecent = &b;
        }
    }

    if (!mostRecent && !allBooks.isEmpty()) {
        mostRecent = &allBooks.first();
    }

    if (mostRecent) {
        m_resumeBookId = mostRecent->id;
        m_bannerTitleLabel->setText(mostRecent->title);
        m_bannerAuthorLabel->setText(mostRecent->author.isEmpty() ? tr("Unknown Author") : mostRecent->author);
        m_bannerProgress->setValue(static_cast<int>(mostRecent->progressPercentage));
        m_bannerTimeLabel->setText(tr("%1% completed").arg(static_cast<int>(mostRecent->progressPercentage)));
        m_continueBanner->show();
    } else {
        m_continueBanner->hide();
    }
}

void BookshelfView::refreshBooks() {
    QString filter = m_formatFilter->currentText();
    if (filter == "All Formats") filter = "";
    else if (filter == "Markdown") filter = "md";

    QString search = m_searchEdit->text().trimmed();

    QList<BookItem> books = DatabaseManager::instance().getBooks(filter, false, search);
    if (m_isListView) {
        populateList(books);
    } else {
        populateGrid(books);
    }

    auto allBooks = DatabaseManager::instance().getAllBooks();
    updateContinueBanner(allBooks);
}

void BookshelfView::clearItems() {
    QLayoutItem* item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

void BookshelfView::populateGrid(const QList<BookItem>& books) {
    clearItems();
    m_gridLayout->setSpacing(20);
    m_gridLayout->setAlignment(Qt::AlignTop);

    int columns = 5;
    for (int c = 0; c < columns; ++c) {
        m_gridLayout->setColumnStretch(c, 1);
    }
    for (int c = columns; c < 10; ++c) {
        m_gridLayout->setColumnStretch(c, 0);
    }

    if (books.isEmpty()) {
        QLabel* emptyLabel = new QLabel(tr("No books found on shelf.\nClick 'Import Book' to add your first publication."), m_gridContainer);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #71717A; font-size: 12pt; line-height: 1.5;");
        m_gridLayout->addWidget(emptyLabel, 0, 0, 1, columns, Qt::AlignCenter);
        return;
    }

    for (int i = 0; i < books.size(); ++i) {
        int row = i / columns;
        int col = i % columns;
        m_gridLayout->setRowStretch(row, 0);

        auto* card = new BookCardWidget(books[i], m_gridContainer);
        connect(card, &BookCardWidget::clicked, this, &BookshelfView::bookSelected);
        connect(card, &BookCardWidget::detailsRequested, this, [this](const QString& bookId) {
            auto b = DatabaseManager::instance().getBookById(bookId);
            if (b.has_value()) {
                auto* dlg = new BookDetailsDialog(*b, this);
                connect(dlg, &BookDetailsDialog::bookUpdated, this, &BookshelfView::refreshBooks);
                connect(dlg, &BookDetailsDialog::bookDeleted, this, &BookshelfView::refreshBooks);
                dlg->show();
            }
        });
        connect(card, &BookCardWidget::exportRequested, this, [this](const QString& bookId) {
            auto b = DatabaseManager::instance().getBookById(bookId);
            if (b.has_value()) {
                auto anns = DatabaseManager::instance().getAnnotationsForBook(bookId);
                auto* dlg = new AnnotationExportDialog(b->title, anns, this);
                dlg->show();
            }
        });
        connect(card, &BookCardWidget::deleteRequested, this, [this](const QString& bookId) {
            auto res = QMessageBox::question(this, tr("Delete Book"), tr("Are you sure you want to remove this book from your library?"));
            if (res == QMessageBox::Yes) {
                DatabaseManager::instance().deleteBook(bookId);
                refreshBooks();
            }
        });
        connect(card, &BookCardWidget::favoriteToggled, this, [this](const QString& bookId, bool /*fav*/) {
            DatabaseManager::instance().toggleFavorite(bookId);
            refreshBooks();
        });
        connect(card, &BookCardWidget::markFinishedRequested, this, [this](const QString& bookId) {
            DatabaseManager::instance().updateBookProgress(bookId, 9999, 9999, 100.0);
            refreshBooks();
        });
        connect(card, &BookCardWidget::resetProgressRequested, this, [this](const QString& bookId) {
            DatabaseManager::instance().updateBookProgress(bookId, 0, 0, 0.0);
            refreshBooks();
        });
        connect(card, &BookCardWidget::showInFileManagerRequested, this, [this](const QString& bookId) {
            auto b = DatabaseManager::instance().getBookById(bookId);
            if (b.has_value() && !b->filePath.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(b->filePath).absolutePath()));
            }
        });
        m_gridLayout->addWidget(card, row, col);
    }
    m_gridLayout->setRowStretch((books.size() + columns - 1) / columns, 1);
}

void BookshelfView::populateList(const QList<BookItem>& books) {
    clearItems();
    m_gridLayout->setSpacing(8);
    m_gridLayout->setAlignment(Qt::AlignTop);

    m_gridLayout->setColumnStretch(0, 1);
    for (int c = 1; c < 10; ++c) {
        m_gridLayout->setColumnStretch(c, 0);
    }

    if (books.isEmpty()) {
        QLabel* emptyLabel = new QLabel(tr("No books found on shelf.\nClick 'Import Book' to add your first publication."), m_gridContainer);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #71717A; font-size: 12pt; line-height: 1.5;");
        m_gridLayout->addWidget(emptyLabel, 0, 0, 1, 1, Qt::AlignCenter);
        return;
    }

    for (int i = 0; i < books.size(); ++i) {
        m_gridLayout->setRowStretch(i, 0);

        auto* rowWidget = new BookListRowWidget(books[i], m_gridContainer);
        connect(rowWidget, &BookListRowWidget::bookClicked, this, &BookshelfView::bookSelected);
        connect(rowWidget, &BookListRowWidget::openRequested, this, &BookshelfView::bookSelected);
        connect(rowWidget, &BookListRowWidget::detailsRequested, this, [this](const QString& bookId) {
            auto b = DatabaseManager::instance().getBookById(bookId);
            if (b.has_value()) {
                auto* dlg = new BookDetailsDialog(*b, this);
                connect(dlg, &BookDetailsDialog::bookUpdated, this, &BookshelfView::refreshBooks);
                connect(dlg, &BookDetailsDialog::bookDeleted, this, &BookshelfView::refreshBooks);
                dlg->show();
            }
        });
        connect(rowWidget, &BookListRowWidget::deleteRequested, this, [this](const QString& bookId) {
            auto res = QMessageBox::question(this, tr("Delete Book"), tr("Are you sure you want to remove this book from your library?"));
            if (res == QMessageBox::Yes) {
                DatabaseManager::instance().deleteBook(bookId);
                refreshBooks();
            }
        });
        connect(rowWidget, &BookListRowWidget::exportRequested, this, [this](const QString& bookId) {
            auto b = DatabaseManager::instance().getBookById(bookId);
            if (b.has_value()) {
                auto anns = DatabaseManager::instance().getAnnotationsForBook(bookId);
                auto* dlg = new AnnotationExportDialog(b->title, anns, this);
                dlg->show();
            }
        });
        connect(rowWidget, &BookListRowWidget::favoriteToggled, this, [this](const QString& bookId, bool /*fav*/) {
            DatabaseManager::instance().toggleFavorite(bookId);
            refreshBooks();
        });
        connect(rowWidget, &BookListRowWidget::markFinishedRequested, this, [this](const QString& bookId) {
            DatabaseManager::instance().updateBookProgress(bookId, 9999, 9999, 100.0);
            refreshBooks();
        });
        connect(rowWidget, &BookListRowWidget::resetProgressRequested, this, [this](const QString& bookId) {
            DatabaseManager::instance().updateBookProgress(bookId, 0, 0, 0.0);
            refreshBooks();
        });
        connect(rowWidget, &BookListRowWidget::showInFileManagerRequested, this, [this](const QString& bookId) {
            auto b = DatabaseManager::instance().getBookById(bookId);
            if (b.has_value() && !b->filePath.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(b->filePath).absolutePath()));
            }
        });
        m_gridLayout->addWidget(rowWidget, i, 0);
    }
    m_gridLayout->setRowStretch(books.size(), 1);
}

} // namespace LunaRead
