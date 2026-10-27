#include "KeyboardShortcutsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

namespace LunaRead {

KeyboardShortcutsDialog::KeyboardShortcutsDialog(QWidget* parent) : QDialog(parent) {
    setupUi();
}

void KeyboardShortcutsDialog::setupUi() {
    setWindowTitle(tr("Keyboard Shortcuts Reference"));
    resize(650, 520);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(tr("<h3>⌨️ LunaRead Keyboard Shortcuts</h3>"), this);
    layout->addWidget(titleLabel);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter shortcuts by name or key..."));
    m_filterEdit->setStyleSheet("QLineEdit { padding: 8px 12px; border: 1px solid #D1D5DB; border-radius: 6px; font-size: 10pt; }");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &KeyboardShortcutsDialog::filterShortcuts);
    layout->addWidget(m_filterEdit);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Category"), tr("Action"), tr("Shortcut"), tr("Description")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet("QTableWidget { border: 1px solid #E5E7EB; border-radius: 6px; }");
    layout->addWidget(m_table, 1);

    // Populate shortcuts
    // Navigation
    addShortcutRow(tr("Navigation"), tr("Next Page"), tr("Space / Right / PgDown / J"), tr("Advance to next page"));
    addShortcutRow(tr("Navigation"), tr("Previous Page"), tr("Left / PgUp / K"), tr("Return to previous page"));
    addShortcutRow(tr("Navigation"), tr("Next Chapter"), tr("] / Ctrl+Right"), tr("Jump directly to next chapter"));
    addShortcutRow(tr("Navigation"), tr("Previous Chapter"), tr("[ / Ctrl+Left"), tr("Jump back to previous chapter"));
    addShortcutRow(tr("Navigation"), tr("Table of Contents"), tr("Tab / T / C"), tr("Open/close navigation drawer"));

    // Reading Tools
    addShortcutRow(tr("Reader Tools"), tr("Bookmark"), tr("B"), tr("Toggle dog-ear bookmark for current page"));
    addShortcutRow(tr("Reader Tools"), tr("Search in Book"), tr("Ctrl+F / /"), tr("Search text across all chapters"));
    addShortcutRow(tr("Reader Tools"), tr("Text-to-Speech"), tr("T"), tr("Toggle text-to-speech spoken narration"));
    addShortcutRow(tr("Reader Tools"), tr("Auto-Scroll HUD"), tr("Shift+Space"), tr("Toggle hands-free auto-scrolling"));
    addShortcutRow(tr("Reader Tools"), tr("Reading Ruler"), tr("Alt+R / R"), tr("Toggle focus guide overlay"));
    addShortcutRow(tr("Reader Tools"), tr("Night Dimmer Filter"), tr("Ctrl+Shift+B / Ctrl+Shift+N"), tr("Toggle blue light & software brightness filter"));

    // Typography & View
    addShortcutRow(tr("View & Font"), tr("Typography Settings"), tr("A / S"), tr("Open font family, size & line spacing dialog"));
    addShortcutRow(tr("View & Font"), tr("Increase Font Size"), tr("Ctrl++ / Ctrl+WheelUp"), tr("Increase body font point size"));
    addShortcutRow(tr("View & Font"), tr("Decrease Font Size"), tr("Ctrl+- / Ctrl+WheelDown"), tr("Decrease body font point size"));
    addShortcutRow(tr("View & Font"), tr("Cycle Theme"), tr("N"), tr("Cycle between light, sepia, OLED, night themes"));
    addShortcutRow(tr("View & Font"), tr("Full Screen"), tr("F11"), tr("Toggle distraction-free full screen mode"));
    addShortcutRow(tr("View & Font"), tr("Back / Close"), tr("Escape"), tr("Close drawer, toolbar, or return to shelf"));

    // Bookshelf
    addShortcutRow(tr("Library"), tr("Import Book"), tr("Ctrl+O"), tr("Browse filesystem to import e-books"));
    addShortcutRow(tr("Library"), tr("Reading Stats"), tr("Ctrl+I"), tr("Open reading analytics dashboard"));
    addShortcutRow(tr("Library"), tr("WebDAV Sync"), tr("Ctrl+Y"), tr("Open cloud synchronization manager"));

    // Bottom action
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet("QPushButton { padding: 8px 24px; border-radius: 6px; background-color: #2563EB; color: white; border: none; font-weight: bold; }"
                              "QPushButton:hover { background-color: #1D4ED8; }");
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_closeBtn);
    layout->addLayout(bottomLayout);
}

void KeyboardShortcutsDialog::addShortcutRow(const QString& category, const QString& action, const QString& shortcut, const QString& description) {
    int row = m_table->rowCount();
    m_table->insertRow(row);

    auto* catItem = new QTableWidgetItem(category);
    QFont f = catItem->font();
    f.setBold(true);
    catItem->setFont(f);
    m_table->setItem(row, 0, catItem);

    m_table->setItem(row, 1, new QTableWidgetItem(action));

    auto* keyItem = new QTableWidgetItem(shortcut);
    keyItem->setTextAlignment(Qt::AlignCenter);
    keyItem->setForeground(QColor(37, 99, 235));
    QFont kf = keyItem->font();
    kf.setBold(true);
    keyItem->setFont(kf);
    m_table->setItem(row, 2, keyItem);

    m_table->setItem(row, 3, new QTableWidgetItem(description));
}

void KeyboardShortcutsDialog::filterShortcuts(const QString& query) {
    QString q = query.trimmed().toLower();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        bool match = q.isEmpty();
        for (int c = 0; c < m_table->columnCount(); ++c) {
            auto* item = m_table->item(r, c);
            if (item && item->text().toLower().contains(q)) {
                match = true;
                break;
            }
        }
        m_table->setRowHidden(r, !match);
    }
}

} // namespace LunaRead
