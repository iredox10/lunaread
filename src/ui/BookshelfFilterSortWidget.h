#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <Qt>
#include "../core/Types.h"

class QComboBox;
class QPushButton;
class QButtonGroup;
class QLineEdit;
class QLabel;
class QFrame;

namespace LunaRead {

/**
 * @brief Advanced bookshelf filtering, sorting, and view layout control toolbar.
 *
 * Emulates Moon+ Reader's comprehensive library management:
 * - Shelf Filter Tabs / Chips: "All Books", "Reading Now", "Finished", "Favorites ⭐"
 * - Sort By Dropdown: Recently Read, Title (A - Z), Author (A - Z), Date Added,
 *                     Reading Progress %, Rating (High to Low), File Size
 * - Sort Order direction toggle: Ascending / Descending
 * - Format Filter Dropdown: All, EPUB, PDF, MOBI, TXT, MD, CBZ, etc.
 * - View Mode Toggle: Grid view (Cards) vs Compact List view
 * - Integrated real-time search input
 * - In-memory Moon+ Reader filtering and natural alphanumeric sorting helpers
 */
class BookshelfFilterSortWidget : public QWidget {
    Q_OBJECT

public:
    explicit BookshelfFilterSortWidget(QWidget* parent = nullptr);
    ~BookshelfFilterSortWidget() override = default;

    // --- State Getters ---
    [[nodiscard]] QString currentSortField() const;
    [[nodiscard]] Qt::SortOrder currentSortOrder() const;
    [[nodiscard]] QString currentCategory() const;
    [[nodiscard]] QString currentFormat() const;
    [[nodiscard]] bool isGridView() const;
    [[nodiscard]] QString searchText() const;
    [[nodiscard]] bool isDarkMode() const { return m_darkMode; }

    // --- Programmatic Controls & Setters ---
    void setSort(const QString& sortField, Qt::SortOrder order, bool emitSignal = true);
    void setSortField(const QString& sortField, bool emitSignal = true);
    void setSortOrder(Qt::SortOrder order, bool emitSignal = true);
    void setCategory(const QString& category, bool emitSignal = true);
    void setFormat(const QString& format, bool emitSignal = true);
    void setViewMode(bool isGrid, bool emitSignal = true);
    void setSearchText(const QString& text, bool emitSignal = true);
    void setSearchVisible(bool visible);
    void setDarkMode(bool dark);

    // --- Moon+ Reader Filtering & Sorting Logic ---
    [[nodiscard]] bool matchesCategory(const BookItem& book) const;
    [[nodiscard]] bool matchesFormat(const BookItem& book) const;
    [[nodiscard]] bool matchesSearch(const BookItem& book) const;
    [[nodiscard]] bool filterBook(const BookItem& book) const;

    void sortBooks(QList<BookItem>& books) const;
    [[nodiscard]] QList<BookItem> filterAndSort(const QList<BookItem>& books) const;

    // --- Reset Filters ---
    void resetFilters();
    void emitAllSignals();

signals:
    // Core specification signals
    void sortChanged(const QString& sortField, Qt::SortOrder order);
    void filterChanged(const QString& category, const QString& format);
    void viewModeChanged(bool isGrid);

    // Additional convenience signals
    void searchChanged(const QString& searchText);
    void filtersReset();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onChipClicked(int id);
    void onFormatComboChanged(int index);
    void onSortComboChanged(int index);
    void onSortOrderBtnClicked();
    void onViewModeBtnClicked(int id);
    void onSearchEdited(const QString& text);

private:
    void setupUi();
    void applyStyles();
    void updateSortOrderButtonUi();
    static Qt::SortOrder defaultOrderForField(const QString& field);
    int findSortComboIndex(const QString& field) const;
    int findFormatComboIndex(const QString& format) const;

    // Filter Chips
    QWidget* m_chipsContainer = nullptr;
    QButtonGroup* m_categoryGroup = nullptr;
    QPushButton* m_chipAll = nullptr;
    QPushButton* m_chipReadingNow = nullptr;
    QPushButton* m_chipFinished = nullptr;
    QPushButton* m_chipFavorites = nullptr;

    // Format Filter
    QLabel* m_formatLabel = nullptr;
    QComboBox* m_formatCombo = nullptr;

    // Sort By & Direction
    QLabel* m_sortLabel = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QPushButton* m_sortOrderBtn = nullptr;

    // View Mode Toggle
    QLabel* m_viewModeLabel = nullptr;
    QFrame* m_viewModeFrame = nullptr;
    QButtonGroup* m_viewModeGroup = nullptr;
    QPushButton* m_gridBtn = nullptr;
    QPushButton* m_listBtn = nullptr;

    // Search Box
    QLineEdit* m_searchEdit = nullptr;

    // Internal State
    QString m_currentCategory;
    QString m_currentFormat;
    QString m_currentSortField;
    Qt::SortOrder m_currentSortOrder = Qt::DescendingOrder;
    bool m_isGrid = true;
    bool m_darkMode = false;
};

} // namespace LunaRead
