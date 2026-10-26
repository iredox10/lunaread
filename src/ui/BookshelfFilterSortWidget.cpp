#include "BookshelfFilterSortWidget.h"
#include "../utils/NaturalSort.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QLabel>
#include <QFrame>
#include <algorithm>

namespace LunaRead {

BookshelfFilterSortWidget::BookshelfFilterSortWidget(QWidget* parent)
    : QWidget(parent)
    , m_currentCategory(QStringLiteral("all"))
    , m_currentFormat(QStringLiteral("all"))
    , m_currentSortField(QStringLiteral("last_read_time"))
    , m_currentSortOrder(Qt::DescendingOrder)
    , m_isGrid(true)
{
    setupUi();
    applyStyles();
}

void BookshelfFilterSortWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(8);

    // Row 1: Category Chips & Search
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(8);

    m_chipsContainer = new QWidget(this);
    auto* chipsLayout = new QHBoxLayout(m_chipsContainer);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(6);

    m_categoryGroup = new QButtonGroup(this);
    m_categoryGroup->setExclusive(true);

    m_chipAll = new QPushButton(tr("📚 All Books"), m_chipsContainer);
    m_chipAll->setCheckable(true);
    m_chipAll->setChecked(true);
    m_categoryGroup->addButton(m_chipAll, 0);
    chipsLayout->addWidget(m_chipAll);

    m_chipReadingNow = new QPushButton(tr("📖 Reading Now"), m_chipsContainer);
    m_chipReadingNow->setCheckable(true);
    m_categoryGroup->addButton(m_chipReadingNow, 1);
    chipsLayout->addWidget(m_chipReadingNow);

    m_chipFinished = new QPushButton(tr("✅ Finished"), m_chipsContainer);
    m_chipFinished->setCheckable(true);
    m_categoryGroup->addButton(m_chipFinished, 2);
    chipsLayout->addWidget(m_chipFinished);

    m_chipFavorites = new QPushButton(tr("⭐ Favorites"), m_chipsContainer);
    m_chipFavorites->setCheckable(true);
    m_categoryGroup->addButton(m_chipFavorites, 3);
    chipsLayout->addWidget(m_chipFavorites);

    connect(m_categoryGroup, &QButtonGroup::idClicked, this, &BookshelfFilterSortWidget::onChipClicked);

    row1->addWidget(m_chipsContainer);
    row1->addStretch(1);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("🔍 Search by title, author..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(220);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &BookshelfFilterSortWidget::onSearchEdited);
    row1->addWidget(m_searchEdit);

    mainLayout->addLayout(row1);

    // Row 2: Format Filter, Sort Controls, and View Mode
    auto* row2 = new QHBoxLayout();
    row2->setSpacing(10);

    m_formatLabel = new QLabel(tr("Format:"), this);
    row2->addWidget(m_formatLabel);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("All Formats"), QStringLiteral("all"));
    m_formatCombo->addItem(QStringLiteral("EPUB"), QStringLiteral("epub"));
    m_formatCombo->addItem(QStringLiteral("PDF"), QStringLiteral("pdf"));
    m_formatCombo->addItem(QStringLiteral("MOBI"), QStringLiteral("mobi"));
    m_formatCombo->addItem(QStringLiteral("FB2"), QStringLiteral("fb2"));
    m_formatCombo->addItem(QStringLiteral("TXT"), QStringLiteral("txt"));
    m_formatCombo->addItem(QStringLiteral("Markdown"), QStringLiteral("md"));
    m_formatCombo->addItem(QStringLiteral("HTML"), QStringLiteral("html"));
    m_formatCombo->addItem(QStringLiteral("DJVU"), QStringLiteral("djvu"));
    m_formatCombo->addItem(QStringLiteral("Comic (CBZ/CBR)"), QStringLiteral("cbz"));
    m_formatCombo->addItem(QStringLiteral("DOCX"), QStringLiteral("docx"));
    m_formatCombo->addItem(QStringLiteral("RTF"), QStringLiteral("rtf"));
    m_formatCombo->addItem(QStringLiteral("ODT"), QStringLiteral("odt"));
    m_formatCombo->addItem(QStringLiteral("CHM"), QStringLiteral("chm"));
    m_formatCombo->addItem(QStringLiteral("UMD"), QStringLiteral("umd"));
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookshelfFilterSortWidget::onFormatComboChanged);
    row2->addWidget(m_formatCombo);

    row2->addSpacing(12);

    m_sortLabel = new QLabel(tr("Sort by:"), this);
    row2->addWidget(m_sortLabel);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("Recently Read"), QStringLiteral("last_read_time"));
    m_sortCombo->addItem(tr("Title (A-Z)"), QStringLiteral("title"));
    m_sortCombo->addItem(tr("Author (A-Z)"), QStringLiteral("author"));
    m_sortCombo->addItem(tr("Date Added"), QStringLiteral("date_added"));
    m_sortCombo->addItem(tr("Reading Progress %"), QStringLiteral("progress_percentage"));
    m_sortCombo->addItem(tr("Rating (Highest)"), QStringLiteral("rating"));
    m_sortCombo->addItem(tr("File Size"), QStringLiteral("file_size"));
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookshelfFilterSortWidget::onSortComboChanged);
    row2->addWidget(m_sortCombo);

    m_sortOrderBtn = new QPushButton(QStringLiteral("↓ Desc"), this);
    m_sortOrderBtn->setToolTip(tr("Toggle Sort Direction (Ascending / Descending)"));
    connect(m_sortOrderBtn, &QPushButton::clicked, this, &BookshelfFilterSortWidget::onSortOrderBtnClicked);
    row2->addWidget(m_sortOrderBtn);

    row2->addStretch(1);

    m_viewModeLabel = new QLabel(tr("View:"), this);
    row2->addWidget(m_viewModeLabel);

    m_viewModeFrame = new QFrame(this);
    auto* viewLayout = new QHBoxLayout(m_viewModeFrame);
    viewLayout->setContentsMargins(0, 0, 0, 0);
    viewLayout->setSpacing(2);

    m_viewModeGroup = new QButtonGroup(this);
    m_viewModeGroup->setExclusive(true);

    m_gridBtn = new QPushButton(tr("⊞ Grid"), m_viewModeFrame);
    m_gridBtn->setCheckable(true);
    m_gridBtn->setChecked(true);
    m_viewModeGroup->addButton(m_gridBtn, 0);
    viewLayout->addWidget(m_gridBtn);

    m_listBtn = new QPushButton(tr("≡ List"), m_viewModeFrame);
    m_listBtn->setCheckable(true);
    m_viewModeGroup->addButton(m_listBtn, 1);
    viewLayout->addWidget(m_listBtn);

    connect(m_viewModeGroup, &QButtonGroup::idClicked, this, &BookshelfFilterSortWidget::onViewModeBtnClicked);
    row2->addWidget(m_viewModeFrame);

    mainLayout->addLayout(row2);
}

void BookshelfFilterSortWidget::applyStyles() {
    QString qss = R"(
        QPushButton {
            padding: 5px 12px;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            background-color: #F3F4F6;
            color: #1F2937;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #E5E7EB;
        }
        QPushButton:checked {
            background-color: #3B82F6;
            color: #FFFFFF;
            border-color: #2563EB;
        }
        QComboBox, QLineEdit {
            padding: 5px 10px;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            background-color: #FFFFFF;
            color: #1F2937;
        }
        QLabel {
            font-weight: 500;
            color: #4B5563;
        }
    )";
    setStyleSheet(qss);
}

void BookshelfFilterSortWidget::onChipClicked(int id) {
    switch (id) {
        case 0: m_currentCategory = QStringLiteral("all"); break;
        case 1: m_currentCategory = QStringLiteral("reading_now"); break;
        case 2: m_currentCategory = QStringLiteral("finished"); break;
        case 3: m_currentCategory = QStringLiteral("favorites"); break;
    }
    emit filterChanged(m_currentCategory, m_currentFormat);
}

void BookshelfFilterSortWidget::onFormatComboChanged(int index) {
    m_currentFormat = m_formatCombo->itemData(index).toString();
    emit filterChanged(m_currentCategory, m_currentFormat);
}

void BookshelfFilterSortWidget::onSortComboChanged(int index) {
    m_currentSortField = m_sortCombo->itemData(index).toString();
    emit sortChanged(m_currentSortField, m_currentSortOrder);
}

void BookshelfFilterSortWidget::onSortOrderBtnClicked() {
    if (m_currentSortOrder == Qt::AscendingOrder) {
        m_currentSortOrder = Qt::DescendingOrder;
        m_sortOrderBtn->setText(QStringLiteral("↓ Desc"));
    } else {
        m_currentSortOrder = Qt::AscendingOrder;
        m_sortOrderBtn->setText(QStringLiteral("↑ Asc"));
    }
    emit sortChanged(m_currentSortField, m_currentSortOrder);
}

void BookshelfFilterSortWidget::onViewModeBtnClicked(int id) {
    m_isGrid = (id == 0);
    emit viewModeChanged(m_isGrid);
}

void BookshelfFilterSortWidget::onSearchEdited(const QString& text) {
    emit searchChanged(text);
}

QString BookshelfFilterSortWidget::currentSortField() const { return m_currentSortField; }
Qt::SortOrder BookshelfFilterSortWidget::currentSortOrder() const { return m_currentSortOrder; }
QString BookshelfFilterSortWidget::currentCategory() const { return m_currentCategory; }
QString BookshelfFilterSortWidget::currentFormat() const { return m_currentFormat; }
bool BookshelfFilterSortWidget::isGridView() const { return m_isGrid; }
QString BookshelfFilterSortWidget::searchText() const { return m_searchEdit->text(); }

void BookshelfFilterSortWidget::setSort(const QString& sortField, Qt::SortOrder order, bool emitSignal) {
    int idx = findSortComboIndex(sortField);
    if (idx >= 0) m_sortCombo->setCurrentIndex(idx);
    m_currentSortOrder = order;
    updateSortOrderButtonUi();
    if (emitSignal) emit sortChanged(m_currentSortField, m_currentSortOrder);
}

void BookshelfFilterSortWidget::setSortField(const QString& sortField, bool emitSignal) {
    setSort(sortField, m_currentSortOrder, emitSignal);
}

void BookshelfFilterSortWidget::setSortOrder(Qt::SortOrder order, bool emitSignal) {
    m_currentSortOrder = order;
    updateSortOrderButtonUi();
    if (emitSignal) emit sortChanged(m_currentSortField, m_currentSortOrder);
}

void BookshelfFilterSortWidget::setCategory(const QString& category, bool emitSignal) {
    m_currentCategory = category.toLower();
    if (m_currentCategory == "reading_now") m_chipReadingNow->setChecked(true);
    else if (m_currentCategory == "finished") m_chipFinished->setChecked(true);
    else if (m_currentCategory == "favorites") m_chipFavorites->setChecked(true);
    else m_chipAll->setChecked(true);
    if (emitSignal) emit filterChanged(m_currentCategory, m_currentFormat);
}

void BookshelfFilterSortWidget::setFormat(const QString& format, bool emitSignal) {
    int idx = findFormatComboIndex(format);
    if (idx >= 0) m_formatCombo->setCurrentIndex(idx);
    if (emitSignal) emit filterChanged(m_currentCategory, m_currentFormat);
}

void BookshelfFilterSortWidget::setViewMode(bool isGrid, bool emitSignal) {
    m_isGrid = isGrid;
    if (isGrid) m_gridBtn->setChecked(true);
    else m_listBtn->setChecked(true);
    if (emitSignal) emit viewModeChanged(m_isGrid);
}

void BookshelfFilterSortWidget::setSearchText(const QString& text, bool emitSignal) {
    m_searchEdit->setText(text);
    if (emitSignal) emit searchChanged(text);
}

void BookshelfFilterSortWidget::setSearchVisible(bool visible) {
    m_searchEdit->setVisible(visible);
}

void BookshelfFilterSortWidget::setDarkMode(bool dark) {
    m_darkMode = dark;
    if (dark) {
        setStyleSheet(R"(
            QPushButton {
                padding: 5px 12px;
                border: 1px solid #374151;
                border-radius: 6px;
                background-color: #1F2937;
                color: #F9FAFB;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #374151;
            }
            QPushButton:checked {
                background-color: #3B82F6;
                color: #FFFFFF;
                border-color: #60A5FA;
            }
            QComboBox, QLineEdit {
                padding: 5px 10px;
                border: 1px solid #374151;
                border-radius: 6px;
                background-color: #111827;
                color: #F9FAFB;
            }
            QLabel {
                font-weight: 500;
                color: #9CA3AF;
            }
        )");
    } else {
        applyStyles();
    }
}

bool BookshelfFilterSortWidget::matchesCategory(const BookItem& book) const {
    if (m_currentCategory == "favorites") return book.isFavorite;
    if (m_currentCategory == "reading_now") return book.progressPercentage > 0.0 && book.progressPercentage < 95.0;
    if (m_currentCategory == "finished") return book.progressPercentage >= 95.0;
    return true;
}

bool BookshelfFilterSortWidget::matchesFormat(const BookItem& book) const {
    if (m_currentFormat.isEmpty() || m_currentFormat == "all") return true;
    return book.format.compare(m_currentFormat, Qt::CaseInsensitive) == 0;
}

bool BookshelfFilterSortWidget::matchesSearch(const BookItem& book) const {
    QString q = m_searchEdit->text().trimmed();
    if (q.isEmpty()) return true;
    return book.title.contains(q, Qt::CaseInsensitive) || book.author.contains(q, Qt::CaseInsensitive);
}

bool BookshelfFilterSortWidget::filterBook(const BookItem& book) const {
    return matchesCategory(book) && matchesFormat(book) && matchesSearch(book);
}

void BookshelfFilterSortWidget::sortBooks(QList<BookItem>& books) const {
    std::sort(books.begin(), books.end(), [this](const BookItem& a, const BookItem& b) {
        int cmp = 0;
        if (m_currentSortField == "title") {
            bool less = NaturalSort::naturalCompare(a.title, b.title);
            return (m_currentSortOrder == Qt::AscendingOrder) ? less : !less;
        } else if (m_currentSortField == "author") {
            bool less = NaturalSort::naturalCompare(a.author, b.author);
            return (m_currentSortOrder == Qt::AscendingOrder) ? less : !less;
        } else if (m_currentSortField == "date_added") {
            if (a.dateAdded < b.dateAdded) cmp = -1;
            else if (a.dateAdded > b.dateAdded) cmp = 1;
        } else if (m_currentSortField == "progress_percentage") {
            if (a.progressPercentage < b.progressPercentage) cmp = -1;
            else if (a.progressPercentage > b.progressPercentage) cmp = 1;
        } else if (m_currentSortField == "rating") {
            if (a.rating < b.rating) cmp = -1;
            else if (a.rating > b.rating) cmp = 1;
        } else if (m_currentSortField == "file_size") {
            if (a.fileSize < b.fileSize) cmp = -1;
            else if (a.fileSize > b.fileSize) cmp = 1;
        } else {
            // last_read_time
            if (a.lastReadTime < b.lastReadTime) cmp = -1;
            else if (a.lastReadTime > b.lastReadTime) cmp = 1;
        }
        return (m_currentSortOrder == Qt::AscendingOrder) ? (cmp < 0) : (cmp > 0);
    });
}

QList<BookItem> BookshelfFilterSortWidget::filterAndSort(const QList<BookItem>& books) const {
    QList<BookItem> filtered;
    for (const auto& b : books) {
        if (filterBook(b)) filtered.append(b);
    }
    sortBooks(filtered);
    return filtered;
}

void BookshelfFilterSortWidget::resetFilters() {
    m_chipAll->setChecked(true);
    m_currentCategory = QStringLiteral("all");
    m_formatCombo->setCurrentIndex(0);
    m_currentFormat = QStringLiteral("all");
    m_searchEdit->clear();
    emit filtersReset();
}

void BookshelfFilterSortWidget::emitAllSignals() {
    emit filterChanged(m_currentCategory, m_currentFormat);
    emit sortChanged(m_currentSortField, m_currentSortOrder);
    emit viewModeChanged(m_isGrid);
}

void BookshelfFilterSortWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
}

void BookshelfFilterSortWidget::updateSortOrderButtonUi() {
    if (m_currentSortOrder == Qt::AscendingOrder) {
        m_sortOrderBtn->setText(QStringLiteral("↑ Asc"));
    } else {
        m_sortOrderBtn->setText(QStringLiteral("↓ Desc"));
    }
}

Qt::SortOrder BookshelfFilterSortWidget::defaultOrderForField(const QString& field) {
    if (field == "title" || field == "author") return Qt::AscendingOrder;
    return Qt::DescendingOrder;
}

int BookshelfFilterSortWidget::findSortComboIndex(const QString& field) const {
    for (int i = 0; i < m_sortCombo->count(); ++i) {
        if (m_sortCombo->itemData(i).toString() == field) return i;
    }
    return -1;
}

int BookshelfFilterSortWidget::findFormatComboIndex(const QString& format) const {
    for (int i = 0; i < m_formatCombo->count(); ++i) {
        if (m_formatCombo->itemData(i).toString() == format) return i;
    }
    return -1;
}

} // namespace LunaRead
