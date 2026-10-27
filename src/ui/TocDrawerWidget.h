#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QPropertyAnimation>
#include <QVector>
#include <QList>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

/**
 * @brief Sliding sidebar drawer for Moon+ Reader style book navigation and annotation management.
 *
 * Provides a 320px width drawer with smooth slide open/close animations and two tabs:
 * 1. Table of Contents: Recursive expandable chapter tree with page links and active reading position indicator.
 * 2. Bookmarks & Highlights: Rich list of user annotations with colored chips, notes, timestamps, and delete actions.
 */
class TocDrawerWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int drawerWidth READ drawerWidth WRITE setDrawerWidth)

public:
    static constexpr int DefaultWidth = 320;

    explicit TocDrawerWidget(QWidget* parent = nullptr);
    ~TocDrawerWidget() override = default;

    /// Drawer width property used for sliding animation
    int drawerWidth() const;
    void setDrawerWidth(int width);

    /// Drawer open/close state control
    bool isOpen() const { return m_isOpen; }
    void openDrawer();
    void closeDrawer();
    void toggleDrawer();

    /// Table of Contents data and active position
    void setToc(const QVector<TocItem>& toc);
    void setCurrentReadingPosition(int chapterIndex, int pageIndex = 0);

    /// Annotations (bookmarks, highlights, notes)
    void setAnnotations(const QList<AnnotationRecord>& annotations);
    void addAnnotation(const AnnotationRecord& annotation);
    void removeAnnotation(const QString& annotationId);
    void clearAnnotations();

    /// Theme styling
    void applyTheme(const ReaderThemeTokens& tokens);

signals:
    void chapterSelected(int chapterIndex);
    void pageSelected(int pageIndex);
    void annotationSelected(const QString& annotationId);
    void annotationDeleted(const QString& annotationId);
    void exportAnnotationsRequested();
    void drawerToggled(bool open);
    void closeRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTocItemClicked(QTreeWidgetItem* item, int column);
    void onTocItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAnnotationItemClicked(QListWidgetItem* item);
    void onAnnotationItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUi();
    void setupTocTab();
    void setupAnnotationsTab();
    void populateTocSubtree(QTreeWidgetItem* parentItem, const QVector<TocItem>& items);
    QTreeWidgetItem* findTocItem(QTreeWidgetItem* parent, int chapterIndex, int pageIndex);
    void updateCurrentIndicator(QTreeWidgetItem* targetItem);
    QWidget* createAnnotationRowWidget(const AnnotationRecord& ann, QListWidgetItem* listItem);
    void updateEmptyAnnotationState();

    bool m_isOpen = true;
    int m_targetWidth = DefaultWidth;
    QPropertyAnimation* m_slideAnimation = nullptr;
    ReaderThemeTokens m_tokens;

    // Header bar widgets
    QWidget* m_headerBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_closeButton = nullptr;

    // Main navigation tabs
    QTabWidget* m_tabWidget = nullptr;

    // Tab 1: Table of Contents
    QTreeWidget* m_tocTree = nullptr;
    QTreeWidgetItem* m_activeIndicatorItem = nullptr;
    int m_currentChapter = -1;
    int m_currentPage = -1;

    // Tab 2: Bookmarks & Highlights
    QListWidget* m_annotationList = nullptr;
    QLabel* m_emptyPlaceholder = nullptr;
    QList<AnnotationRecord> m_annotations;
};

} // namespace LunaRead
