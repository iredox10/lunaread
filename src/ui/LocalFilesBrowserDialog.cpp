#include "LocalFilesBrowserDialog.h"
#include "../parsers/ParserFactory.h"
#include "../db/DatabaseManager.h"
#include "../utils/NaturalSort.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTreeView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QFileSystemModel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QDateTime>
#include <QCompleter>
#include <QShortcut>
#include <QKeySequence>
#include <QMessageBox>
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QKeyEvent>

namespace LunaRead {

namespace {

// ============================================================================
// Custom Table Widget Items for Natural & Numeric Sorting
// ============================================================================

class NaturalSortTableWidgetItem : public QTableWidgetItem {
public:
    explicit NaturalSortTableWidgetItem(const QString& text) : QTableWidgetItem(text) {}

    bool operator<(const QTableWidgetItem& other) const override {
        return NaturalSort::naturalCompare(text(), other.text(), false);
    }
};

class NumericTableWidgetItem : public QTableWidgetItem {
public:
    NumericTableWidgetItem(const QString& text, qint64 val)
        : QTableWidgetItem(text), m_val(val) {}

    bool operator<(const QTableWidgetItem& other) const override {
        const auto* o = dynamic_cast<const NumericTableWidgetItem*>(&other);
        if (o) {
            return m_val < o->m_val;
        }
        return QTableWidgetItem::operator<(other);
    }

private:
    qint64 m_val = 0;
};

class DateTimeTableWidgetItem : public QTableWidgetItem {
public:
    DateTimeTableWidgetItem(const QString& text, const QDateTime& dt)
        : QTableWidgetItem(text), m_dt(dt) {}

    bool operator<(const QTableWidgetItem& other) const override {
        const auto* o = dynamic_cast<const DateTimeTableWidgetItem*>(&other);
        if (o) {
            return m_dt < o->m_dt;
        }
        return QTableWidgetItem::operator<(other);
    }

private:
    QDateTime m_dt;
};

// ============================================================================
// FileBadgeDelegate: Paints Format & Status Pills
// ============================================================================

class FileBadgeDelegate : public QStyledItemDelegate {
public:
    explicit FileBadgeDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, option.palette.highlight());
        }

        const int col = index.column();
        if (col == 1) {
            // Format Badge Pill
            QString badgeText = index.data(Qt::DisplayRole).toString();
            if (badgeText.isEmpty()) return;

            QColor badgeBg = index.data(Qt::UserRole + 1).value<QColor>();
            if (!badgeBg.isValid()) {
                badgeBg = QColor(99, 102, 241);
            }

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);

            QFont badgeFont = option.font;
            badgeFont.setBold(true);
            badgeFont.setPointSize(8);
            painter->setFont(badgeFont);

            QFontMetrics fm(badgeFont);
            int textWidth = fm.horizontalAdvance(badgeText);
            int badgeW = std::max(textWidth + 14, 52);
            int badgeH = 20;

            QRect badgeRect(option.rect.center().x() - badgeW / 2,
                            option.rect.center().y() - badgeH / 2,
                            badgeW, badgeH);

            QPainterPath path;
            path.addRoundedRect(badgeRect, 4.0, 4.0);
            painter->fillPath(path, badgeBg);

            painter->setPen(Qt::white);
            painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
            painter->restore();
            return;
        }

        if (col == 5) {
            // Already In Library Status Pill
            QString statusText = index.data(Qt::DisplayRole).toString();
            if (statusText.isEmpty()) return;

            bool inLib = index.data(Qt::UserRole + 2).toBool();
            bool isDark = index.data(Qt::UserRole + 3).toBool();

            QColor bg;
            QColor fg;
            if (isDark) {
                bg = inLib ? QColor(20, 83, 45) : QColor(30, 58, 138);
                fg = inLib ? QColor(134, 239, 172) : QColor(147, 197, 253);
            } else {
                bg = inLib ? QColor(220, 252, 231) : QColor(219, 234, 254);
                fg = inLib ? QColor(21, 128, 61) : QColor(29, 78, 216);
            }

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);

            QFont statusFont = option.font;
            statusFont.setBold(true);
            statusFont.setPointSize(8);
            painter->setFont(statusFont);

            QFontMetrics fm(statusFont);
            int textWidth = fm.horizontalAdvance(statusText);
            int badgeW = textWidth + 16;
            int badgeH = 20;

            QRect badgeRect(option.rect.center().x() - badgeW / 2,
                            option.rect.center().y() - badgeH / 2,
                            badgeW, badgeH);

            QPainterPath path;
            path.addRoundedRect(badgeRect, 10.0, 10.0);
            painter->fillPath(path, bg);

            painter->setPen(fg);
            painter->drawText(badgeRect, Qt::AlignCenter, statusText);
            painter->restore();
            return;
        }

        QStyledItemDelegate::paint(painter, option, index);
    }
};

} // namespace

// ============================================================================
// LocalFilesBrowserDialog Implementation
// ============================================================================

LocalFilesBrowserDialog::LocalFilesBrowserDialog(const QString& initialDir, QWidget* parent)
    : QDialog(parent) {
    setupUi();

    QString startDir = initialDir;
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        QSettings settings("LunaRead", "LunaReadDesktop");
        startDir = settings.value("LocalFilesBrowser/lastDir").toString();
    }
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        startDir = QDir::homePath();
    }

    navigateTo(startDir);
}

void LocalFilesBrowserDialog::setupUi() {
    setWindowTitle(tr("Local File Manager & Batch Importer - LunaRead"));
    resize(1020, 650);
    setMinimumSize(800, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    setupHeader();
    mainLayout->addWidget(m_pathEdit->parentWidget());

    setupSplitter();
    mainLayout->addWidget(m_splitter, 1);

    setupBottomBar();
    mainLayout->addWidget(m_closeBtn->parentWidget());

    setupShortcuts();
    updateStylesheet();
}

void LocalFilesBrowserDialog::setupHeader() {
    auto* headerContainer = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(headerContainer);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    auto* pathIconLabel = new QLabel(QStringLiteral("📁"), headerContainer);
    QFont iconFont = pathIconLabel->font();
    iconFont.setPointSize(12);
    pathIconLabel->setFont(iconFont);
    headerLayout->addWidget(pathIconLabel);

    m_pathEdit = new QLineEdit(headerContainer);
    m_pathEdit->setPlaceholderText(tr("Enter or paste directory path..."));
    m_pathEdit->setFixedHeight(36);

    auto* completer = new QCompleter(this);
    auto* compModel = new QFileSystemModel(completer);
    compModel->setRootPath(QDir::rootPath());
    compModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    completer->setModel(compModel);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_pathEdit->setCompleter(completer);

    connect(m_pathEdit, &QLineEdit::returnPressed, this, &LocalFilesBrowserDialog::onPathEditSubmitted);
    headerLayout->addWidget(m_pathEdit, 1);

    m_upBtn = new QPushButton(tr("⬆ Up"), headerContainer);
    m_upBtn->setFixedHeight(36);
    m_upBtn->setCursor(Qt::PointingHandCursor);
    m_upBtn->setToolTip(tr("Navigate to parent directory (Alt+Up)"));
    connect(m_upBtn, &QPushButton::clicked, this, &LocalFilesBrowserDialog::navigateUp);
    headerLayout->addWidget(m_upBtn);

    m_homeBtn = new QPushButton(tr("🏠 Home"), headerContainer);
    m_homeBtn->setFixedHeight(36);
    m_homeBtn->setCursor(Qt::PointingHandCursor);
    m_homeBtn->setToolTip(tr("Navigate to user Documents or Home directory"));
    connect(m_homeBtn, &QPushButton::clicked, this, &LocalFilesBrowserDialog::navigateHome);
    headerLayout->addWidget(m_homeBtn);

    m_refreshBtn = new QPushButton(tr("🔄 Refresh"), headerContainer);
    m_refreshBtn->setFixedHeight(36);
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setToolTip(tr("Refresh current directory and library status (F5)"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &LocalFilesBrowserDialog::refresh);
    headerLayout->addWidget(m_refreshBtn);
}

void LocalFilesBrowserDialog::setupSplitter() {
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // --- Left Pane: Directory Tree ---
    auto* leftContainer = new QWidget(m_splitter);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    auto* leftTitle = new QLabel(tr("<b>Directories</b>"), leftContainer);
    leftLayout->addWidget(leftTitle);

    m_dirModel = new QFileSystemModel(this);
    m_dirModel->setRootPath(QDir::rootPath());
    m_dirModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);

    m_dirTreeView = new QTreeView(leftContainer);
    m_dirTreeView->setModel(m_dirModel);
    m_dirTreeView->setAnimated(true);
    m_dirTreeView->setIndentation(16);
    m_dirTreeView->setSortingEnabled(true);
    m_dirTreeView->sortByColumn(0, Qt::AscendingOrder);
    m_dirTreeView->setHeaderHidden(true);

    // Hide size, type, and date columns in directory tree
    m_dirTreeView->setColumnHidden(1, true);
    m_dirTreeView->setColumnHidden(2, true);
    m_dirTreeView->setColumnHidden(3, true);

    connect(m_dirTreeView, &QTreeView::clicked, this, &LocalFilesBrowserDialog::onDirectoryTreeClicked);
    connect(m_dirTreeView, &QTreeView::activated, this, &LocalFilesBrowserDialog::onDirectoryTreeClicked);

    // Sync tree selection when directory finishes loading asynchronously
    connect(m_dirModel, &QFileSystemModel::directoryLoaded, this, [this](const QString& loadedPath) {
        if (!m_currentDir.isEmpty() && m_currentDir.startsWith(loadedPath)) {
            QModelIndex idx = m_dirModel->index(m_currentDir);
            if (idx.isValid()) {
                m_dirTreeView->setCurrentIndex(idx);
                m_dirTreeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
            }
        }
    });

    leftLayout->addWidget(m_dirTreeView, 1);
    m_splitter->addWidget(leftContainer);

    // --- Right Pane: Filterable E-book Files Table ---
    auto* rightContainer = new QWidget(m_splitter);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    auto* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    auto* filesTitle = new QLabel(tr("<b>Supported E-Books</b>"), rightContainer);
    filterLayout->addWidget(filesTitle);

    filterLayout->addStretch(1);

    m_filterEdit = new QLineEdit(rightContainer);
    m_filterEdit->setPlaceholderText(tr("🔍 Filter by filename or format..."));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setFixedWidth(260);
    m_filterEdit->setFixedHeight(30);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &LocalFilesBrowserDialog::onFilterTextChanged);
    filterLayout->addWidget(m_filterEdit);

    rightLayout->addLayout(filterLayout);

    m_filesTable = new QTableWidget(rightContainer);
    m_filesTable->setColumnCount(6);
    m_filesTable->setHorizontalHeaderLabels({
        QStringLiteral(""),
        tr("Format"),
        tr("File Name"),
        tr("Size"),
        tr("Modified Date"),
        tr("Library Status")
    });

    m_filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_filesTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_filesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_filesTable->setShowGrid(true);
    m_filesTable->setAlternatingRowColors(false);
    m_filesTable->verticalHeader()->setVisible(false);
    m_filesTable->verticalHeader()->setDefaultSectionSize(34);

    m_filesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_filesTable->horizontalHeader()->resizeSection(0, 36);

    m_filesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_filesTable->horizontalHeader()->resizeSection(1, 80);

    m_filesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_filesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_filesTable->horizontalHeader()->resizeSection(3, 95);

    m_filesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_filesTable->horizontalHeader()->resizeSection(4, 140);

    m_filesTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_filesTable->horizontalHeader()->resizeSection(5, 120);

    // Custom delegate for format and status badges
    m_filesTable->setItemDelegate(new FileBadgeDelegate(m_filesTable));
    m_filesTable->installEventFilter(this);

    connect(m_filesTable, &QTableWidget::itemChanged, this, &LocalFilesBrowserDialog::onTableItemChanged);
    connect(m_filesTable, &QTableWidget::cellDoubleClicked, this, &LocalFilesBrowserDialog::onTableDoubleClicked);

    rightLayout->addWidget(m_filesTable, 1);

    m_statusLabel = new QLabel(tr("Scanning directory..."), rightContainer);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #64748B; font-size: 8.5pt;"));
    rightLayout->addWidget(m_statusLabel);

    m_splitter->addWidget(rightContainer);
    m_splitter->setSizes({280, 700});
}

void LocalFilesBrowserDialog::setupBottomBar() {
    auto* bottomContainer = new QWidget(this);
    auto* bottomLayout = new QHBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(10);

    m_scanSubdirsCheck = new QCheckBox(tr("Scan Subdirectories"), bottomContainer);
    m_scanSubdirsCheck->setToolTip(tr("Recursively search child folders for supported e-book files"));
    connect(m_scanSubdirsCheck, &QCheckBox::toggled, this, &LocalFilesBrowserDialog::onScanSubdirsToggled);
    bottomLayout->addWidget(m_scanSubdirsCheck);

    bottomLayout->addSpacing(8);

    m_selectAllBtn = new QPushButton(tr("Select All"), bottomContainer);
    m_selectAllBtn->setToolTip(tr("Select all visible files in the list (Ctrl+A)"));
    connect(m_selectAllBtn, &QPushButton::clicked, this, &LocalFilesBrowserDialog::selectAll);
    bottomLayout->addWidget(m_selectAllBtn);

    m_deselectAllBtn = new QPushButton(tr("Deselect All"), bottomContainer);
    m_deselectAllBtn->setToolTip(tr("Clear selection for all files"));
    connect(m_deselectAllBtn, &QPushButton::clicked, this, &LocalFilesBrowserDialog::deselectAll);
    bottomLayout->addWidget(m_deselectAllBtn);

    bottomLayout->addStretch(1);

    m_openDirectlyBtn = new QPushButton(tr("📖 Open Directly"), bottomContainer);
    m_openDirectlyBtn->setToolTip(tr("Open the selected book immediately in the reader view"));
    m_openDirectlyBtn->setFixedHeight(36);
    m_openDirectlyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openDirectlyBtn, &QPushButton::clicked, this, &LocalFilesBrowserDialog::openDirectly);
    bottomLayout->addWidget(m_openDirectlyBtn);

    m_importBtn = new QPushButton(tr("⬇ Import Selected (0)"), bottomContainer);
    m_importBtn->setObjectName(QStringLiteral("primaryAction"));
    m_importBtn->setFixedHeight(36);
    m_importBtn->setCursor(Qt::PointingHandCursor);
    m_importBtn->setEnabled(false);
    m_importBtn->setToolTip(tr("Batch import all checked e-books into your LunaRead bookshelf"));
    connect(m_importBtn, &QPushButton::clicked, this, &LocalFilesBrowserDialog::importSelected);
    bottomLayout->addWidget(m_importBtn);

    m_closeBtn = new QPushButton(tr("Close"), bottomContainer);
    m_closeBtn->setFixedHeight(36);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(m_closeBtn);
}

void LocalFilesBrowserDialog::setupShortcuts() {
    auto* refreshShortcut = new QShortcut(QKeySequence::Refresh, this);
    connect(refreshShortcut, &QShortcut::activated, this, &LocalFilesBrowserDialog::refresh);

    auto* selectAllShortcut = new QShortcut(QKeySequence::SelectAll, m_filesTable);
    connect(selectAllShortcut, &QShortcut::activated, this, &LocalFilesBrowserDialog::selectAll);

    auto* upShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Up), this);
    connect(upShortcut, &QShortcut::activated, this, &LocalFilesBrowserDialog::navigateUp);
}

bool LocalFilesBrowserDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_filesTable && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Space) {
            auto selectedItems = m_filesTable->selectedItems();
            QSet<int> rows;
            for (auto* it : selectedItems) {
                rows.insert(it->row());
            }
            if (!rows.isEmpty()) {
                int firstRow = *rows.begin();
                auto* firstItem = m_filesTable->item(firstRow, 0);
                Qt::CheckState targetState = (firstItem && firstItem->checkState() == Qt::Checked)
                    ? Qt::Unchecked : Qt::Checked;

                m_isPopulating = true;
                for (int r : rows) {
                    if (auto* it = m_filesTable->item(r, 0)) {
                        it->setCheckState(targetState);
                    }
                }
                m_isPopulating = false;
                updateSelectionCount();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void LocalFilesBrowserDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    m_isDark = tokens.isDark;
    updateStylesheet();

    // Trigger viewport repainting for updated badge theme colors
    if (m_filesTable) {
        m_filesTable->viewport()->update();
    }
}

void LocalFilesBrowserDialog::setDarkMode(bool dark) {
    m_isDark = dark;
    m_tokens = dark ? ReaderThemeTokens::night() : ReaderThemeTokens::modernLight();
    updateStylesheet();

    if (m_filesTable) {
        m_filesTable->viewport()->update();
    }
}

void LocalFilesBrowserDialog::navigateTo(const QString& path) {
    if (path.isEmpty()) return;

    QDir dir(path);
    if (!dir.exists()) return;

    QString cleanPath = dir.canonicalPath();
    if (cleanPath.isEmpty()) {
        cleanPath = dir.absolutePath();
    }

    m_currentDir = cleanPath;
    m_pathEdit->setText(cleanPath);

    QModelIndex idx = m_dirModel->index(cleanPath);
    if (idx.isValid()) {
        m_dirTreeView->setCurrentIndex(idx);
        m_dirTreeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        m_dirTreeView->expand(idx);
    }

    scanCurrentDirectory();

    QSettings settings("LunaRead", "LunaReadDesktop");
    settings.setValue("LocalFilesBrowser/lastDir", cleanPath);
}

void LocalFilesBrowserDialog::navigateUp() {
    if (m_currentDir.isEmpty()) return;
    QDir dir(m_currentDir);
    if (dir.cdUp()) {
        navigateTo(dir.absolutePath());
    }
}

void LocalFilesBrowserDialog::navigateHome() {
    QString home = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (home.isEmpty() || !QDir(home).exists()) {
        home = QDir::homePath();
    }
    navigateTo(home);
}

void LocalFilesBrowserDialog::refresh() {
    refreshLibraryCache();
    scanCurrentDirectory();
}

void LocalFilesBrowserDialog::onDirectoryTreeClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    QString path = m_dirModel->filePath(index);
    if (QFileInfo(path).isDir()) {
        navigateTo(path);
    }
}

void LocalFilesBrowserDialog::onPathEditSubmitted() {
    QString entered = m_pathEdit->text().trimmed();
    if (entered.isEmpty()) return;

    QDir dir(entered);
    if (dir.exists()) {
        navigateTo(dir.absolutePath());
    } else {
        QMessageBox::warning(this, tr("Invalid Directory"), tr("The directory '%1' does not exist.").arg(entered));
        m_pathEdit->setText(m_currentDir);
    }
}

void LocalFilesBrowserDialog::onScanSubdirsToggled(bool checked) {
    Q_UNUSED(checked);
    scanCurrentDirectory();
}

void LocalFilesBrowserDialog::onFilterTextChanged(const QString& filterText) {
    const QString query = filterText.trimmed().toLower();
    for (int r = 0; r < m_filesTable->rowCount(); ++r) {
        if (query.isEmpty()) {
            m_filesTable->setRowHidden(r, false);
            continue;
        }

        auto* nameItem = m_filesTable->item(r, 2);
        auto* formatItem = m_filesTable->item(r, 1);
        bool match = false;
        if (nameItem && nameItem->text().toLower().contains(query)) match = true;
        if (formatItem && formatItem->text().toLower().contains(query)) match = true;
        m_filesTable->setRowHidden(r, !match);
    }
}

void LocalFilesBrowserDialog::onTableItemChanged(QTableWidgetItem* item) {
    if (m_isPopulating || !item) return;
    if (item->column() == 0) {
        updateSelectionCount();
    }
}

void LocalFilesBrowserDialog::onTableDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row >= 0 && row < m_filesTable->rowCount()) {
        auto* it = m_filesTable->item(row, 0);
        if (it) {
            QString path = it->data(Qt::UserRole).toString();
            if (!path.isEmpty() && QFileInfo::exists(path)) {
                emit openDirectlyRequested(path);
                accept();
            }
        }
    }
}

void LocalFilesBrowserDialog::selectAll() {
    m_isPopulating = true;
    for (int r = 0; r < m_filesTable->rowCount(); ++r) {
        if (!m_filesTable->isRowHidden(r)) {
            if (auto* it = m_filesTable->item(r, 0)) {
                it->setCheckState(Qt::Checked);
            }
        }
    }
    m_isPopulating = false;
    updateSelectionCount();
}

void LocalFilesBrowserDialog::deselectAll() {
    m_isPopulating = true;
    for (int r = 0; r < m_filesTable->rowCount(); ++r) {
        if (auto* it = m_filesTable->item(r, 0)) {
            it->setCheckState(Qt::Unchecked);
        }
    }
    m_isPopulating = false;
    updateSelectionCount();
}

void LocalFilesBrowserDialog::updateSelectionCount() {
    int count = 0;
    for (int r = 0; r < m_filesTable->rowCount(); ++r) {
        auto* it = m_filesTable->item(r, 0);
        if (it && it->checkState() == Qt::Checked) {
            count++;
        }
    }

    m_importBtn->setText(tr("⬇ Import Selected (%1)").arg(count));
    m_importBtn->setEnabled(count > 0);
}

QStringList LocalFilesBrowserDialog::selectedFiles() const {
    QStringList result;
    for (int r = 0; r < m_filesTable->rowCount(); ++r) {
        auto* it = m_filesTable->item(r, 0);
        if (it && it->checkState() == Qt::Checked) {
            QString path = it->data(Qt::UserRole).toString();
            if (!path.isEmpty() && QFileInfo::exists(path)) {
                result.append(path);
            }
        }
    }
    return result;
}

void LocalFilesBrowserDialog::importSelected() {
    QStringList files = selectedFiles();
    if (files.isEmpty()) return;

    emit importRequested(files);
    accept();
}

void LocalFilesBrowserDialog::openDirectly() {
    QString targetPath;
    int currentRow = m_filesTable->currentRow();
    if (currentRow >= 0 && currentRow < m_filesTable->rowCount()) {
        auto* it = m_filesTable->item(currentRow, 0);
        if (it) {
            targetPath = it->data(Qt::UserRole).toString();
        }
    }

    if (targetPath.isEmpty()) {
        for (int r = 0; r < m_filesTable->rowCount(); ++r) {
            auto* it = m_filesTable->item(r, 0);
            if (it && it->checkState() == Qt::Checked) {
                targetPath = it->data(Qt::UserRole).toString();
                break;
            }
        }
    }

    if (!targetPath.isEmpty() && QFileInfo::exists(targetPath)) {
        emit openDirectlyRequested(targetPath);
        accept();
    } else {
        QMessageBox::information(this, tr("Open Directly"), tr("Please select an e-book from the list to open."));
    }
}

void LocalFilesBrowserDialog::refreshLibraryCache() {
    m_libraryPaths.clear();
    const auto books = DatabaseManager::instance().getAllBooks();
    for (const auto& b : books) {
        if (!b.filePath.isEmpty()) {
            m_libraryPaths.insert(QDir::cleanPath(b.filePath));
            QString canonical = QFileInfo(b.filePath).canonicalFilePath();
            if (!canonical.isEmpty()) {
                m_libraryPaths.insert(canonical);
            }
        }
    }
}

void LocalFilesBrowserDialog::scanCurrentDirectory() {
    if (m_currentDir.isEmpty()) return;
    QDir dir(m_currentDir);
    if (!dir.exists()) return;

    refreshLibraryCache();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    QStringList supportedFiles;
    if (m_scanSubdirsCheck && m_scanSubdirsCheck->isChecked()) {
        QDirIterator it(m_currentDir, QDir::Files | QDir::Readable | QDir::NoSymLinks,
                        QDirIterator::Subdirectories);
        int scanned = 0;
        while (it.hasNext()) {
            QString filePath = it.next();
            if (ParserFactory::isSupported(filePath)) {
                supportedFiles.append(filePath);
            }
            scanned++;
            if (scanned % 250 == 0) {
                QCoreApplication::processEvents();
            }
        }
    } else {
        const auto list = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const auto& fi : list) {
            if (ParserFactory::isSupported(fi.filePath())) {
                supportedFiles.append(fi.absoluteFilePath());
            }
        }
    }

    QApplication::restoreOverrideCursor();

    populateFileList(supportedFiles);
}

void LocalFilesBrowserDialog::populateFileList(const QStringList& paths) {
    m_isPopulating = true;
    m_filesTable->setSortingEnabled(false);
    m_filesTable->setRowCount(0);
    m_filesTable->setRowCount(static_cast<int>(paths.size()));

    int inLibraryCount = 0;
    int newCount = 0;

    for (int r = 0; r < paths.size(); ++r) {
        const QString& filePath = paths[r];
        QFileInfo fi(filePath);

        bool inLib = m_libraryPaths.contains(QDir::cleanPath(filePath))
                  || (!fi.canonicalFilePath().isEmpty() && m_libraryPaths.contains(fi.canonicalFilePath()));
        if (inLib) {
            inLibraryCount++;
        } else {
            newCount++;
        }

        // Col 0: Checkbox
        auto* checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(Qt::UserRole, filePath);
        checkItem->setTextAlignment(Qt::AlignCenter);
        m_filesTable->setItem(r, 0, checkItem);

        // Col 1: Format Badge
        QString ext = fi.suffix().toUpper();
        if (ext.isEmpty()) ext = QStringLiteral("TXT");
        auto* formatItem = new QTableWidgetItem(ext);
        formatItem->setTextAlignment(Qt::AlignCenter);
        formatItem->setData(Qt::UserRole, filePath);
        formatItem->setData(Qt::UserRole + 1, formatBadgeColor(ext));
        formatItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_filesTable->setItem(r, 1, formatItem);

        // Col 2: File Name (with NaturalSort support)
        auto* nameItem = new NaturalSortTableWidgetItem(fi.fileName());
        nameItem->setData(Qt::UserRole, filePath);
        nameItem->setToolTip(filePath);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_filesTable->setItem(r, 2, nameItem);

        // Col 3: File Size (with numeric sorting)
        auto* sizeItem = new NumericTableWidgetItem(formatFileSize(fi.size()), fi.size());
        sizeItem->setData(Qt::UserRole, filePath);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sizeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_filesTable->setItem(r, 3, sizeItem);

        // Col 4: Modified Date (with chronological sorting)
        auto* dateItem = new DateTimeTableWidgetItem(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm")), fi.lastModified());
        dateItem->setData(Qt::UserRole, filePath);
        dateItem->setTextAlignment(Qt::AlignCenter);
        dateItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_filesTable->setItem(r, 4, dateItem);

        // Col 5: Library Status
        auto* statusItem = new QTableWidgetItem(inLib ? tr("✓ In Library") : tr("New"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setData(Qt::UserRole, filePath);
        statusItem->setData(Qt::UserRole + 2, inLib);
        statusItem->setData(Qt::UserRole + 3, m_isDark);
        statusItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_filesTable->setItem(r, 5, statusItem);
    }

    m_filesTable->setSortingEnabled(true);
    m_isPopulating = false;

    if (m_filterEdit && !m_filterEdit->text().isEmpty()) {
        onFilterTextChanged(m_filterEdit->text());
    }

    if (paths.isEmpty()) {
        m_statusLabel->setText(tr("No supported e-book files found in current directory."));
    } else {
        m_statusLabel->setText(tr("Found %1 supported e-book%2 (%3 already in library, %4 new)")
            .arg(paths.size())
            .arg(paths.size() > 1 ? QStringLiteral("s") : QStringLiteral(""))
            .arg(inLibraryCount)
            .arg(newCount));
    }

    updateSelectionCount();
}

QString LocalFilesBrowserDialog::formatFileSize(qint64 bytes) {
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QColor LocalFilesBrowserDialog::formatBadgeColor(const QString& format) {
    const QString f = format.toLower();
    if (f == QStringLiteral("epub")) return QColor(16, 185, 129);       // Emerald green
    if (f == QStringLiteral("pdf")) return QColor(239, 68, 68);         // Crimson red
    if (f == QStringLiteral("mobi") || f == QStringLiteral("azw") || f == QStringLiteral("azw3")) return QColor(245, 158, 11); // Amber
    if (f == QStringLiteral("fb2")) return QColor(139, 92, 246);        // Purple
    if (f == QStringLiteral("djvu")) return QColor(14, 165, 233);       // Cyan
    if (f == QStringLiteral("cbz") || f == QStringLiteral("cbr")) return QColor(236, 72, 153); // Pink
    if (f == QStringLiteral("docx") || f == QStringLiteral("odt")) return QColor(59, 130, 246); // Blue
    if (f == QStringLiteral("txt") || f == QStringLiteral("md")) return QColor(107, 114, 128); // Slate
    if (f == QStringLiteral("html") || f == QStringLiteral("htm") || f == QStringLiteral("xhtml")) return QColor(249, 115, 22); // Orange
    if (f == QStringLiteral("chm") || f == QStringLiteral("umd") || f == QStringLiteral("rtf")) return QColor(99, 102, 241); // Indigo
    return QColor(99, 102, 241);                                        // Default Indigo
}

void LocalFilesBrowserDialog::updateStylesheet() {
    if (m_isDark) {
        setStyleSheet(QStringLiteral(
            "QDialog {"
            "    background-color: #18181B;"
            "    color: #F4F4F5;"
            "    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;"
            "}"
            "QLabel {"
            "    color: #E4E4E7;"
            "}"
            "QLineEdit {"
            "    background-color: #27272A;"
            "    border: 1px solid #3F3F46;"
            "    border-radius: 6px;"
            "    padding: 6px 10px;"
            "    color: #F4F4F5;"
            "    font-size: 9.5pt;"
            "}"
            "QLineEdit:focus {"
            "    border: 1px solid #3B82F6;"
            "}"
            "QPushButton {"
            "    background-color: #27272A;"
            "    border: 1px solid #3F3F46;"
            "    border-radius: 6px;"
            "    padding: 6px 14px;"
            "    color: #E4E4E7;"
            "    font-weight: 500;"
            "    font-size: 9pt;"
            "}"
            "QPushButton:hover {"
            "    background-color: #3F3F46;"
            "    border-color: #52525B;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #18181B;"
            "}"
            "QPushButton:disabled {"
            "    background-color: #27272A;"
            "    color: #71717A;"
            "    border-color: #27272A;"
            "}"
            "QPushButton#primaryAction {"
            "    background-color: #2563EB;"
            "    border: 1px solid #1D4ED8;"
            "    color: #FFFFFF;"
            "    font-weight: 600;"
            "}"
            "QPushButton#primaryAction:hover {"
            "    background-color: #1D4ED8;"
            "}"
            "QPushButton#primaryAction:disabled {"
            "    background-color: #1E293B;"
            "    color: #64748B;"
            "    border-color: #1E293B;"
            "}"
            "QTreeView, QTableWidget {"
            "    background-color: #18181B;"
            "    border: 1px solid #27272A;"
            "    border-radius: 6px;"
            "    color: #E4E4E7;"
            "    gridline-color: #27272A;"
            "    selection-background-color: #312E81;"
            "    selection-color: #EEF2FF;"
            "}"
            "QHeaderView::section {"
            "    background-color: #27272A;"
            "    color: #A1A1AA;"
            "    padding: 6px 8px;"
            "    font-weight: 600;"
            "    font-size: 8.5pt;"
            "    border: none;"
            "    border-right: 1px solid #3F3F46;"
            "    border-bottom: 1px solid #3F3F46;"
            "}"
            "QCheckBox {"
            "    color: #E4E4E7;"
            "    font-size: 9.5pt;"
            "    spacing: 6px;"
            "}"
            "QCheckBox::indicator {"
            "    width: 16px;"
            "    height: 16px;"
            "    border-radius: 3px;"
            "    border: 1px solid #3F3F46;"
            "    background-color: #27272A;"
            "}"
            "QCheckBox::indicator:checked {"
            "    background-color: #3B82F6;"
            "    border-color: #2563EB;"
            "}"
            "QSplitter::handle {"
            "    background-color: #27272A;"
            "}"
        ));
    } else {
        setStyleSheet(QStringLiteral(
            "QDialog {"
            "    background-color: #F8FAFC;"
            "    color: #0F172A;"
            "    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;"
            "}"
            "QLabel {"
            "    color: #334155;"
            "}"
            "QLineEdit {"
            "    background-color: #FFFFFF;"
            "    border: 1px solid #CBD5E1;"
            "    border-radius: 6px;"
            "    padding: 6px 10px;"
            "    color: #1E293B;"
            "    font-size: 9.5pt;"
            "}"
            "QLineEdit:focus {"
            "    border: 1px solid #3B82F6;"
            "}"
            "QPushButton {"
            "    background-color: #FFFFFF;"
            "    border: 1px solid #CBD5E1;"
            "    border-radius: 6px;"
            "    padding: 6px 14px;"
            "    color: #334155;"
            "    font-weight: 500;"
            "    font-size: 9pt;"
            "}"
            "QPushButton:hover {"
            "    background-color: #F1F5F9;"
            "    border-color: #94A3B8;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #E2E8F0;"
            "}"
            "QPushButton:disabled {"
            "    background-color: #F1F5F9;"
            "    color: #94A3B8;"
            "    border-color: #E2E8F0;"
            "}"
            "QPushButton#primaryAction {"
            "    background-color: #2563EB;"
            "    border: 1px solid #1D4ED8;"
            "    color: #FFFFFF;"
            "    font-weight: 600;"
            "}"
            "QPushButton#primaryAction:hover {"
            "    background-color: #1D4ED8;"
            "}"
            "QPushButton#primaryAction:disabled {"
            "    background-color: #93C5FD;"
            "    color: #FFFFFF;"
            "    border-color: #93C5FD;"
            "}"
            "QTreeView, QTableWidget {"
            "    background-color: #FFFFFF;"
            "    border: 1px solid #E2E8F0;"
            "    border-radius: 6px;"
            "    color: #1E293B;"
            "    gridline-color: #F1F5F9;"
            "    selection-background-color: #E0E7FF;"
            "    selection-color: #1E1B4B;"
            "}"
            "QHeaderView::section {"
            "    background-color: #F8FAFC;"
            "    color: #64748B;"
            "    padding: 6px 8px;"
            "    font-weight: 600;"
            "    font-size: 8.5pt;"
            "    border: none;"
            "    border-right: 1px solid #E2E8F0;"
            "    border-bottom: 1px solid #E2E8F0;"
            "}"
            "QCheckBox {"
            "    color: #1E293B;"
            "    font-size: 9.5pt;"
            "    spacing: 6px;"
            "}"
            "QCheckBox::indicator {"
            "    width: 16px;"
            "    height: 16px;"
            "    border-radius: 3px;"
            "    border: 1px solid #CBD5E1;"
            "    background-color: #FFFFFF;"
            "}"
            "QCheckBox::indicator:checked {"
            "    background-color: #3B82F6;"
            "    border-color: #2563EB;"
            "}"
            "QSplitter::handle {"
            "    background-color: #E2E8F0;"
            "}"
        ));
    }
}

} // namespace LunaRead
