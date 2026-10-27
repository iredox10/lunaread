#include "AllAnnotationsDialog.h"
#include "AnnotationExportDialog.h"
#include "../db/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <algorithm>

namespace LunaRead {

AllAnnotationsDialog::AllAnnotationsDialog(const QString& bookId, const QString& bookTitle, QWidget* parent)
    : QDialog(parent), m_bookId(bookId), m_bookTitle(bookTitle), m_tokens(ReaderThemeTokens::parchment()) {
    setWindowTitle(tr("🔖 My Highlights, Bookmarks & Notes - %1").arg(bookTitle.isEmpty() ? tr("Library") : bookTitle));
    resize(780, 560);
    setModal(true);

    setupUi();
    reloadAnnotations();
    applyTheme(m_tokens);
}

void AllAnnotationsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // 1. Filter Header Bar
    auto* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(10);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("🔍 Search quotes, notes or thoughts..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AllAnnotationsDialog::onFilterChanged);
    filterLayout->addWidget(m_searchEdit, 2);

    m_typeFilterCombo = new QComboBox(this);
    m_typeFilterCombo->addItem(tr("All Types"), "all");
    m_typeFilterCombo->addItem(tr("🖍️ Highlights"), "highlight");
    m_typeFilterCombo->addItem(tr("🔖 Bookmarks"), "bookmark");
    m_typeFilterCombo->addItem(tr("📝 User Notes"), "note");
    connect(m_typeFilterCombo, &QComboBox::currentIndexChanged, this, &AllAnnotationsDialog::onFilterChanged);
    filterLayout->addWidget(m_typeFilterCombo, 1);

    m_colorFilterCombo = new QComboBox(this);
    m_colorFilterCombo->addItem(tr("All Colors"), "all");
    m_colorFilterCombo->addItem(tr("🟡 Yellow"), "yellow");
    m_colorFilterCombo->addItem(tr("🟢 Green"), "green");
    m_colorFilterCombo->addItem(tr("🔵 Blue"), "blue");
    m_colorFilterCombo->addItem(tr("🔴 Pink / Red"), "red");
    m_colorFilterCombo->addItem(tr("🟣 Purple"), "purple");
    connect(m_colorFilterCombo, &QComboBox::currentIndexChanged, this, &AllAnnotationsDialog::onFilterChanged);
    filterLayout->addWidget(m_colorFilterCombo, 1);

    mainLayout->addLayout(filterLayout);

    // 2. Count Label
    m_countLabel = new QLabel(tr("Loading annotations..."), this);
    m_countLabel->setStyleSheet("font-size: 12px; color: #78909c; font-weight: bold;");
    mainLayout->addWidget(m_countLabel);

    // 3. Annotations List View
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setSpacing(8);
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &AllAnnotationsDialog::onItemDoubleClicked);
    mainLayout->addWidget(m_listWidget, 1);

    // 4. Bottom Action Bar
    auto* btmLayout = new QHBoxLayout();
    btmLayout->setSpacing(12);

    m_deleteAllBtn = new QPushButton(tr("🗑️ Delete All"), this);
    m_deleteAllBtn->setCursor(Qt::PointingHandCursor);
    m_deleteAllBtn->setStyleSheet("color: #e53935;");
    connect(m_deleteAllBtn, &QPushButton::clicked, this, &AllAnnotationsDialog::onDeleteAllClicked);
    btmLayout->addWidget(m_deleteAllBtn);

    btmLayout->addStretch();

    m_exportBtn = new QPushButton(tr("📤 Export Notes..."), this);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportBtn, &QPushButton::clicked, this, &AllAnnotationsDialog::onExportClicked);
    btmLayout->addWidget(m_exportBtn);

    auto* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btmLayout->addWidget(closeBtn);

    mainLayout->addLayout(btmLayout);
}

void AllAnnotationsDialog::reloadAnnotations() {
    if (m_bookId.isEmpty()) {
        m_allAnnotations = DatabaseManager::instance().getAnnotationsByType("");
    } else {
        m_allAnnotations = DatabaseManager::instance().getAnnotationsForBook(m_bookId);
    }
    onFilterChanged();
}

void AllAnnotationsDialog::onFilterChanged() {
    m_listWidget->clear();

    const QString search = m_searchEdit->text().trimmed();
    const QString typeFilter = m_typeFilterCombo->currentData().toString();
    const QString colorFilter = m_colorFilterCombo->currentData().toString();

    int displayedCount = 0;

    for (const auto& a : m_allAnnotations) {
        // Type filter check
        if (typeFilter != "all") {
            if (typeFilter == "note" && a.note.trimmed().isEmpty()) continue;
            if (typeFilter == "bookmark" && a.type != "bookmark") continue;
            if (typeFilter == "highlight" && a.type != "highlight") continue;
        }

        // Color filter check
        if (colorFilter != "all") {
            if (colorFilter == "yellow" && (a.color.red() < 200 || a.color.green() < 180 || a.color.blue() > 180)) continue;
            if (colorFilter == "green" && (a.color.green() < 150 || a.color.red() > a.color.green())) continue;
            if (colorFilter == "blue" && (a.color.blue() < 180 || a.color.red() > a.color.blue())) continue;
            if (colorFilter == "red" && (a.color.red() < 200 || a.color.blue() > 180)) continue;
            if (colorFilter == "purple" && (a.color.blue() < 150 || a.color.red() < 150)) continue;
        }

        // Search text check
        if (!search.isEmpty()) {
            bool matchQuote = a.selectedText.contains(search, Qt::CaseInsensitive);
            bool matchNote = a.note.contains(search, Qt::CaseInsensitive);
            if (!matchQuote && !matchNote) continue;
        }

        // Create list item and row widget
        auto* item = new QListWidgetItem(m_listWidget);
        item->setData(Qt::UserRole, a.id);
        item->setData(Qt::UserRole + 1, a.chapterIndex);
        item->setData(Qt::UserRole + 2, a.pageIndex);

        QWidget* rowWidget = createAnnotationRowWidget(a);
        item->setSizeHint(rowWidget->sizeHint());
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, rowWidget);

        displayedCount++;
    }

    m_countLabel->setText(tr("Showing %1 of %2 annotations")
        .arg(displayedCount)
        .arg(m_allAnnotations.size()));
}

QWidget* AllAnnotationsDialog::createAnnotationRowWidget(const AnnotationRecord& a) {
    auto* container = new QFrame();
    container->setObjectName("annotationCard");
    container->setFrameShape(QFrame::StyledPanel);

    auto* rowLayout = new QHBoxLayout(container);
    rowLayout->setContentsMargins(14, 12, 14, 12);
    rowLayout->setSpacing(12);

    // Left Colored Stripe
    auto* stripe = new QFrame(container);
    stripe->setFixedWidth(5);
    QColor stripeColor = (a.type == "bookmark") ? QColor(255, 179, 0) : a.color;
    stripe->setStyleSheet(QString("background-color: %1; border-radius: 2px;").arg(stripeColor.name()));
    rowLayout->addWidget(stripe);

    // Middle Content Column
    auto* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(6);

    // Top info metadata
    auto* metaLayout = new QHBoxLayout();
    QString typeIcon = (a.type == "bookmark") ? tr("🔖 Bookmark") : (!a.note.isEmpty() ? tr("📝 Note & Highlight") : tr("🖍️ Highlight"));
    auto* typeLabel = new QLabel(typeIcon, container);
    typeLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #546e7a;");
    metaLayout->addWidget(typeLabel);

    auto* locLabel = new QLabel(tr("Chapter %1, Page %2").arg(a.chapterIndex + 1).arg(a.pageIndex + 1), container);
    locLabel->setStyleSheet("font-size: 11px; color: #78909c;");
    metaLayout->addWidget(locLabel);

    metaLayout->addStretch();

    if (a.createdAt.isValid()) {
        auto* dateLabel = new QLabel(a.createdAt.toString("yyyy-MM-dd hh:mm"), container);
        dateLabel->setStyleSheet("font-size: 10px; color: #90a4ae;");
        metaLayout->addWidget(dateLabel);
    }

    contentLayout->addLayout(metaLayout);

    // Quoted text excerpt
    if (!a.selectedText.trimmed().isEmpty()) {
        auto* quoteLabel = new QLabel(QString("“%1”").arg(a.selectedText), container);
        quoteLabel->setWordWrap(true);
        quoteLabel->setStyleSheet("font-size: 13px; font-style: italic; color: #37474f; padding: 2px 0;");
        contentLayout->addWidget(quoteLabel);
    }

    // User note / thought
    if (!a.note.trimmed().isEmpty()) {
        auto* noteFrame = new QFrame(container);
        noteFrame->setStyleSheet("background-color: rgba(0, 0, 0, 0.04); border-radius: 4px; padding: 6px;");
        auto* noteLayout = new QVBoxLayout(noteFrame);
        noteLayout->setContentsMargins(6, 4, 6, 4);

        auto* noteLabel = new QLabel(QString("💭 %1").arg(a.note), noteFrame);
        noteLabel->setWordWrap(true);
        noteLabel->setStyleSheet("font-size: 12px; color: #263238; font-weight: 500;");
        noteLayout->addWidget(noteLabel);
        contentLayout->addWidget(noteFrame);
    }

    rowLayout->addLayout(contentLayout, 1);

    // Right Action Buttons Column
    auto* btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(6);
    btnLayout->setAlignment(Qt::AlignTop);

    auto* jumpBtn = new QPushButton(tr("📖 Read Here"), container);
    jumpBtn->setCursor(Qt::PointingHandCursor);
    jumpBtn->setStyleSheet("padding: 4px 8px; font-size: 11px;");
    connect(jumpBtn, &QPushButton::clicked, this, [this, a]() { onJumpClicked(a); });
    btnLayout->addWidget(jumpBtn);

    auto* editBtn = new QPushButton(tr("✏️ Edit Note"), container);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet("padding: 4px 8px; font-size: 11px;");
    connect(editBtn, &QPushButton::clicked, this, [this, a]() { onEditNoteClicked(a); });
    btnLayout->addWidget(editBtn);

    auto* copyBtn = new QPushButton(tr("📋 Copy"), container);
    copyBtn->setCursor(Qt::PointingHandCursor);
    copyBtn->setStyleSheet("padding: 4px 8px; font-size: 11px;");
    connect(copyBtn, &QPushButton::clicked, this, [a]() {
        QString text = a.selectedText.isEmpty() ? a.note : a.selectedText;
        QGuiApplication::clipboard()->setText(text);
    });
    btnLayout->addWidget(copyBtn);

    auto* delBtn = new QPushButton(tr("🗑️ Delete"), container);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet("padding: 4px 8px; font-size: 11px; color: #e53935;");
    connect(delBtn, &QPushButton::clicked, this, [this, a]() { onDeleteClicked(a); });
    btnLayout->addWidget(delBtn);

    rowLayout->addLayout(btnLayout);

    return container;
}

void AllAnnotationsDialog::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    int ch = item->data(Qt::UserRole + 1).toInt();
    int pg = item->data(Qt::UserRole + 2).toInt();
    emit jumpToAnnotationRequested(ch, pg);
    accept();
}

void AllAnnotationsDialog::onJumpClicked(const AnnotationRecord& record) {
    emit jumpToAnnotationRequested(record.chapterIndex, record.pageIndex);
    accept();
}

void AllAnnotationsDialog::onEditNoteClicked(const AnnotationRecord& record) {
    bool ok = false;
    QString newNote = QInputDialog::getMultiLineText(
        this,
        tr("Edit Personal Note"),
        tr("Note for excerpt: \"%1\"").arg(record.selectedText.left(60)),
        record.note,
        &ok);

    if (ok) {
        AnnotationRecord updated = record;
        updated.note = newNote.trimmed();
        updated.updatedAt = QDateTime::currentDateTime();
        DatabaseManager::instance().updateAnnotation(updated);
        emit annotationChanged();
        reloadAnnotations();
    }
}

void AllAnnotationsDialog::onDeleteClicked(const AnnotationRecord& record) {
    auto reply = QMessageBox::question(
        this,
        tr("Delete Annotation"),
        tr("Are you sure you want to delete this annotation?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        DatabaseManager::instance().deleteAnnotation(record.id);
        emit annotationChanged();
        reloadAnnotations();
    }
}

void AllAnnotationsDialog::onExportClicked() {
    auto* dlg = new AnnotationExportDialog(m_bookTitle.isEmpty() ? tr("Library") : m_bookTitle, m_allAnnotations, this);
    dlg->show();
}

void AllAnnotationsDialog::onDeleteAllClicked() {
    if (m_allAnnotations.isEmpty()) return;

    auto reply = QMessageBox::warning(
        this,
        tr("Delete All Annotations"),
        tr("Are you sure you want to permanently delete all %1 annotations for this book?").arg(m_allAnnotations.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (!m_bookId.isEmpty()) {
            DatabaseManager::instance().deleteAnnotationsForBook(m_bookId);
        } else {
            for (const auto& a : m_allAnnotations) {
                DatabaseManager::instance().deleteAnnotation(a.id);
            }
        }
        emit annotationChanged();
        reloadAnnotations();
    }
}

void AllAnnotationsDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    setStyleSheet(QString(
        "QDialog { background-color: %1; color: %2; }"
        "QListWidget { background-color: transparent; border: none; }"
        "QListWidget::item { border: none; padding: 2px 0; }"
        "QFrame#annotationCard { background-color: %3; border: 1px solid %4; border-radius: 8px; }"
        "QLineEdit, QComboBox { background-color: %5; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 10px; }"
        "QPushButton { background-color: %5; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: %6; }"
    ).arg(tokens.backgroundColor.name(),
         tokens.textColor.name(),
         tokens.isDark ? "#242424" : "#ffffff",
         tokens.isDark ? "#383838" : "#e0e0e0",
         tokens.isDark ? "#2d2d2d" : "#f5f5f5",
         tokens.isDark ? "#3a3a3a" : "#eaeaea"));
}

} // namespace LunaRead
