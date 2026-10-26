#include "BookshelfView.h"
#include "BookDetailsDialog.h"
#include "../db/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

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

void BookshelfView::refreshBooks() {
    QString filter = m_formatFilter->currentText();
    if (filter == "All Formats") filter = "";
    else if (filter == "Markdown") filter = "md";

    QString search = m_searchEdit->text().trimmed();

    QList<BookItem> books = DatabaseManager::instance().getBooks(filter, false, search);
    populateGrid(books);
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
            auto item = DatabaseManager::instance().getBookById(bookId);
            if (item.has_value()) {
                auto* dlg = new BookDetailsDialog(*item, this);
                connect(dlg, &BookDetailsDialog::bookUpdated, this, &BookshelfView::refreshBooks);
                connect(dlg, &BookDetailsDialog::bookDeleted, this, &BookshelfView::refreshBooks);
                dlg->show();
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
