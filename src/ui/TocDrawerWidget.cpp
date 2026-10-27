#include "TocDrawerWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QKeyEvent>

namespace LunaRead {

TocDrawerWidget::TocDrawerWidget(QWidget* parent)
    : QWidget(parent) {
    setFixedWidth(DefaultWidth);
    m_slideAnimation = new QPropertyAnimation(this, "drawerWidth", this);
    m_slideAnimation->setDuration(220);
    m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);

    setupUi();
}

int TocDrawerWidget::drawerWidth() const {
    return width();
}

void TocDrawerWidget::setDrawerWidth(int width) {
    setFixedWidth(width);
    emit drawerToggled(width > 0);
}

void TocDrawerWidget::openDrawer() {
    m_isOpen = true;
    show();
    m_slideAnimation->stop();
    m_slideAnimation->setStartValue(width());
    m_slideAnimation->setEndValue(m_targetWidth);
    m_slideAnimation->start();
}

void TocDrawerWidget::closeDrawer() {
    m_isOpen = false;
    m_slideAnimation->stop();
    m_slideAnimation->setStartValue(width());
    m_slideAnimation->setEndValue(0);
    connect(m_slideAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (!m_isOpen) hide();
    });
    m_slideAnimation->start();
    emit closeRequested();
}

void TocDrawerWidget::toggleDrawer() {
    if (m_isOpen) {
        closeDrawer();
    } else {
        openDrawer();
    }
}

void TocDrawerWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header bar
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(44);
    auto* headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(12, 0, 8, 0);

    m_titleLabel = new QLabel(tr("Navigation"), m_headerBar);
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    m_titleLabel->setFont(titleFont);

    m_closeButton = new QPushButton(QStringLiteral("✕"), m_headerBar);
    m_closeButton->setFixedSize(28, 28);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet("QPushButton { border: none; font-size: 13pt; background: transparent; } QPushButton:hover { color: #EF4444; }");
    connect(m_closeButton, &QPushButton::clicked, this, &TocDrawerWidget::closeDrawer);

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_closeButton);
    layout->addWidget(m_headerBar);

    // Tabs
    m_tabWidget = new QTabWidget(this);
    setupTocTab();
    setupAnnotationsTab();
    layout->addWidget(m_tabWidget, 1);
}

void TocDrawerWidget::setupTocTab() {
    m_tocTree = new QTreeWidget(this);
    m_tocTree->setHeaderLabels({tr("Chapter / Section"), tr("Page")});
    m_tocTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tocTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tocTree->setAnimated(true);
    m_tocTree->setIndentation(16);

    connect(m_tocTree, &QTreeWidget::itemClicked, this, &TocDrawerWidget::onTocItemClicked);
    connect(m_tocTree, &QTreeWidget::itemDoubleClicked, this, &TocDrawerWidget::onTocItemDoubleClicked);

    m_tabWidget->addTab(m_tocTree, tr("Contents"));
}

void TocDrawerWidget::setupAnnotationsTab() {
    auto* annContainer = new QWidget(this);
    auto* annLayout = new QVBoxLayout(annContainer);
    annLayout->setContentsMargins(4, 4, 4, 4);
    annLayout->setSpacing(6);

    m_annotationList = new QListWidget(this);
    m_annotationList->setAlternatingRowColors(true);
    connect(m_annotationList, &QListWidget::itemClicked, this, &TocDrawerWidget::onAnnotationItemClicked);
    connect(m_annotationList, &QListWidget::itemDoubleClicked, this, &TocDrawerWidget::onAnnotationItemDoubleClicked);

    annLayout->addWidget(m_annotationList, 1);

    auto* exportBtn = new QPushButton(tr("📤 Export Annotations..."), annContainer);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet("QPushButton { padding: 8px; border-radius: 4px; font-weight: bold; background-color: #2563EB; color: white; border: none; }"
                             "QPushButton:hover { background-color: #1D4ED8; }");
    connect(exportBtn, &QPushButton::clicked, this, &TocDrawerWidget::exportAnnotationsRequested);
    annLayout->addWidget(exportBtn);

    m_tabWidget->addTab(annContainer, tr("Bookmarks & Notes"));
}

void TocDrawerWidget::setToc(const QVector<TocItem>& toc) {
    m_tocTree->clear();
    m_activeIndicatorItem = nullptr;
    populateTocSubtree(nullptr, toc);
}

void TocDrawerWidget::populateTocSubtree(QTreeWidgetItem* parentItem, const QVector<TocItem>& items) {
    for (int i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        auto* treeItem = new QTreeWidgetItem();
        treeItem->setText(0, item.title);
        treeItem->setText(1, QString::number(item.pageIndex + 1));
        treeItem->setData(0, Qt::UserRole, item.chapterIndex);
        treeItem->setData(1, Qt::UserRole, item.pageIndex);

        if (parentItem) {
            parentItem->addChild(treeItem);
        } else {
            m_tocTree->addTopLevelItem(treeItem);
        }

        if (!item.children.isEmpty()) {
            populateTocSubtree(treeItem, item.children);
            treeItem->setExpanded(true);
        }
    }
}

QTreeWidgetItem* TocDrawerWidget::findTocItem(QTreeWidgetItem* parent, int chapterIndex, int pageIndex) {
    int count = parent ? parent->childCount() : m_tocTree->topLevelItemCount();
    for (int i = 0; i < count; ++i) {
        auto* item = parent ? parent->child(i) : m_tocTree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toInt() == chapterIndex) {
            return item;
        }
        auto* childFound = findTocItem(item, chapterIndex, pageIndex);
        if (childFound) return childFound;
    }
    return nullptr;
}

void TocDrawerWidget::updateCurrentIndicator(QTreeWidgetItem* targetItem) {
    if (m_activeIndicatorItem) {
        QFont f = m_activeIndicatorItem->font(0);
        f.setBold(false);
        m_activeIndicatorItem->setFont(0, f);
        m_activeIndicatorItem->setForeground(0, m_tokens.textColor);
    }
    m_activeIndicatorItem = targetItem;
    if (m_activeIndicatorItem) {
        QFont f = m_activeIndicatorItem->font(0);
        f.setBold(true);
        m_activeIndicatorItem->setFont(0, f);
        m_activeIndicatorItem->setForeground(0, m_tokens.accentColor);
        m_tocTree->scrollToItem(m_activeIndicatorItem);
    }
}

void TocDrawerWidget::setCurrentReadingPosition(int chapterIndex, int pageIndex) {
    auto* item = findTocItem(nullptr, chapterIndex, pageIndex);
    if (item) {
        updateCurrentIndicator(item);
    }
}

void TocDrawerWidget::setAnnotations(const QList<AnnotationRecord>& annotations) {
    clearAnnotations();
    for (const auto& ann : annotations) {
        addAnnotation(ann);
    }
}

void TocDrawerWidget::addAnnotation(const AnnotationRecord& annotation) {
    auto* item = new QListWidgetItem();
    item->setData(Qt::UserRole, annotation.id);
    item->setData(Qt::UserRole + 1, annotation.pageIndex);
    item->setData(Qt::UserRole + 2, annotation.chapterIndex);

    auto* widget = createAnnotationRowWidget(annotation, item);
    item->setSizeHint(widget->sizeHint());
    m_annotationList->addItem(item);
    m_annotationList->setItemWidget(item, widget);
}

void TocDrawerWidget::removeAnnotation(const QString& annotationId) {
    for (int i = 0; i < m_annotationList->count(); ++i) {
        auto* item = m_annotationList->item(i);
        if (item->data(Qt::UserRole).toString() == annotationId) {
            delete m_annotationList->takeItem(i);
            break;
        }
    }
}

void TocDrawerWidget::clearAnnotations() {
    m_annotationList->clear();
}

QWidget* TocDrawerWidget::createAnnotationRowWidget(const AnnotationRecord& ann, QListWidgetItem* listItem) {
    Q_UNUSED(listItem);
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(6, 6, 6, 6);
    l->setSpacing(2);

    auto* topH = new QHBoxLayout();
    auto* colorChip = new QLabel();
    colorChip->setFixedSize(12, 12);
    colorChip->setStyleSheet(QStringLiteral("border-radius: 6px; background-color: %1;").arg(ann.color.name()));
    topH->addWidget(colorChip);

    auto* typeLabel = new QLabel(ann.type.toUpper());
    QFont f = typeLabel->font();
    f.setBold(true);
    f.setPointSize(8);
    typeLabel->setFont(f);
    topH->addWidget(typeLabel);

    topH->addStretch(1);

    auto* delBtn = new QPushButton(QStringLiteral("✕"));
    delBtn->setFixedSize(18, 18);
    delBtn->setStyleSheet("QPushButton { border: none; font-size: 8pt; color: #94A3B8; } QPushButton:hover { color: #EF4444; }");
    connect(delBtn, &QPushButton::clicked, this, [this, annId = ann.id]() {
        emit annotationDeleted(annId);
        removeAnnotation(annId);
    });
    topH->addWidget(delBtn);
    l->addLayout(topH);

    if (!ann.selectedText.isEmpty()) {
        auto* textLabel = new QLabel(QStringLiteral("\"%1\"").arg(ann.selectedText));
        textLabel->setWordWrap(true);
        textLabel->setStyleSheet("font-style: italic; color: #475569; font-size: 9pt;");
        l->addWidget(textLabel);
    }

    if (!ann.note.isEmpty()) {
        auto* noteLabel = new QLabel(ann.note);
        noteLabel->setWordWrap(true);
        noteLabel->setStyleSheet("color: #1E293B; font-size: 9pt;");
        l->addWidget(noteLabel);
    }

    auto* footerLabel = new QLabel(QStringLiteral("Page %1 • %2").arg(ann.pageIndex + 1).arg(ann.createdAt.toString("MMM d, yyyy")));
    footerLabel->setStyleSheet("color: #94A3B8; font-size: 8pt;");
    l->addWidget(footerLabel);

    return w;
}

void TocDrawerWidget::onTocItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item) return;
    int chIdx = item->data(0, Qt::UserRole).toInt();
    int pgIdx = item->data(1, Qt::UserRole).toInt();
    emit chapterSelected(chIdx);
    emit pageSelected(pgIdx);
}

void TocDrawerWidget::onTocItemDoubleClicked(QTreeWidgetItem* item, int column) {
    onTocItemClicked(item, column);
}

void TocDrawerWidget::onAnnotationItemClicked(QListWidgetItem* item) {
    if (!item) return;
    QString id = item->data(Qt::UserRole).toString();
    int pgIdx = item->data(Qt::UserRole + 1).toInt();
    int chIdx = item->data(Qt::UserRole + 2).toInt();
    emit annotationSelected(id);
    emit chapterSelected(chIdx);
    emit pageSelected(pgIdx);
}

void TocDrawerWidget::onAnnotationItemDoubleClicked(QListWidgetItem* item) {
    onAnnotationItemClicked(item);
}

void TocDrawerWidget::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    QString qss = QStringLiteral(
        "TocDrawerWidget { background-color: %1; border-right: 1px solid %2; }"
        "QTabWidget::pane { border: none; background-color: %1; }"
        "QTabBar::tab { background-color: %3; color: %4; padding: 8px 16px; border: none; }"
        "QTabBar::tab:selected { background-color: %1; color: %5; font-weight: bold; border-bottom: 2px solid %5; }"
        "QTreeWidget { background-color: %1; color: %4; border: none; }"
        "QTreeWidget::item:hover { background-color: %3; }"
        "QTreeWidget::item:selected { background-color: %3; color: %5; }"
        "QListWidget { background-color: %1; color: %4; border: none; }"
        "QListWidget::item:hover { background-color: %3; }"
        "QHeaderView::section { background-color: %3; color: %4; border: none; padding: 4px; }"
    ).arg(tokens.backgroundColor.name(),
         tokens.dividerColor.name(),
         tokens.dividerColor.name(),
         tokens.textColor.name(),
         tokens.accentColor.name());
    setStyleSheet(qss);
}

void TocDrawerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void TocDrawerWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        closeDrawer();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace LunaRead
