#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QDateTime>
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QTreeView;
class QTableWidget;
class QTableWidgetItem;
class QFileSystemModel;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QLabel;
class QSplitter;
class QModelIndex;
class QEvent;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief High-performance built-in local file manager and batch e-book importer.
 *
 * Provides a split-pane interface with directory hierarchy navigation on the left
 * and supported e-book file filtering on the right, featuring format badges,
 * library presence detection, subdirectory recursive scanning, and batch import.
 */
class LocalFilesBrowserDialog : public QDialog {
    Q_OBJECT

public:
    explicit LocalFilesBrowserDialog(const QString& initialDir = QString(), QWidget* parent = nullptr);
    ~LocalFilesBrowserDialog() override = default;

    void applyTheme(const ReaderThemeTokens& tokens);
    void setDarkMode(bool dark);
    [[nodiscard]] bool isDarkMode() const noexcept { return m_isDark; }

    [[nodiscard]] QString currentDirectory() const noexcept { return m_currentDir; }
    [[nodiscard]] QStringList selectedFiles() const;

signals:
    void importRequested(const QStringList& filePaths);
    void openDirectlyRequested(const QString& filePath);

public slots:
    void navigateTo(const QString& path);
    void navigateUp();
    void navigateHome();
    void refresh();
    void selectAll();
    void deselectAll();
    void importSelected();
    void openDirectly();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onDirectoryTreeClicked(const QModelIndex& index);
    void onPathEditSubmitted();
    void onScanSubdirsToggled(bool checked);
    void onTableItemChanged(QTableWidgetItem* item);
    void onTableDoubleClicked(int row, int column);
    void onFilterTextChanged(const QString& filterText);

private:
    void setupUi();
    void setupHeader();
    void setupSplitter();
    void setupBottomBar();
    void setupShortcuts();
    void updateStylesheet();
    void scanCurrentDirectory();
    void populateFileList(const QStringList& paths);
    void updateSelectionCount();
    void refreshLibraryCache();

    static QString formatFileSize(qint64 bytes);
    static QColor formatBadgeColor(const QString& format);

    // Header navigation widgets
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_upBtn = nullptr;
    QPushButton* m_homeBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;

    // Splitter & main panes
    QSplitter* m_splitter = nullptr;
    QTreeView* m_dirTreeView = nullptr;
    QFileSystemModel* m_dirModel = nullptr;

    QLineEdit* m_filterEdit = nullptr;
    QTableWidget* m_filesTable = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Bottom action bar widgets
    QCheckBox* m_scanSubdirsCheck = nullptr;
    QPushButton* m_selectAllBtn = nullptr;
    QPushButton* m_deselectAllBtn = nullptr;
    QPushButton* m_openDirectlyBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // Internal state
    QString m_currentDir;
    QSet<QString> m_libraryPaths;
    bool m_isDark = false;
    ReaderThemeTokens m_tokens = ReaderThemeTokens::modernLight();
    bool m_isPopulating = false;
};

} // namespace LunaRead
