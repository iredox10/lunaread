#include "BookshelfView.h"
#include "BookDetailsDialog.h"
#include "AnnotationExportDialog.h"
#include "../db/DatabaseManager.h"
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

    // Filter & Search Header
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search books by title or author...");
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setStyleSheet("QLineEdit { border: 1px solid #D4D4D8; border-radius: 6px; padding: 0 12px; font-size: 11pt; }");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &BookshelfView::refreshBooks);
    headerLayout->addWidget(m_searchEdit, 1);

    m_formatFilter = new QComboBox(this);
    m_formatFilter->setFixedHeight(36);
    m_formatFilter->addItems({"All Formats", "EPUB", "PDF", "TXT", "MOBI", "FB2", "DOCX", "CBZ", "Markdown"});
    connect(m_formatFilter, &QComboBox::currentIndexChanged, this, &BookshelfView::refreshBooks);
    headerLayout->addWidget(m_formatFilter);

    mainLayout->addLayout(headerLayout);

    // Continue Reading Hero Banner
    setupContinueBanner();
    mainLayout->addWidget(m_continueBanner);

    // Scroll Area with Grid
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    m_gridContainer = new QWidget(m_scrollArea);
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(20);
    m_scrollArea->setWidget(m_gridContainer);

    mainLayout->addWidget(m_scrollArea, 1);

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
    populateGrid(books);

    auto allBooks = DatabaseManager::instance().getAllBooks();
    updateContinueBanner(allBooks);
}

void BookshelfView::populateGrid(const QList<BookItem>& books) {
    // Clear existing
    QLayoutItem* item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (books.isEmpty()) {
        QLabel* emptyLabel = new QLabel("No books found on shelf.\nClick 'Import Book' to add your first publication.", m_gridContainer);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #71717A; font-size: 12pt; line-height: 1.5;");
        m_gridLayout->addWidget(emptyLabel, 0, 0, 1, 4, Qt::AlignCenter);
        return;
    }

    int columns = 5;
    for (int i = 0; i < books.size(); ++i) {
        int row = i / columns;
        int col = i % columns;

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
        m_gridLayout->addWidget(card, row, col);
    }
}

} // namespace LunaRead
