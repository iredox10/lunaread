#include "AnnotationExportDialog.h"
#include "../features/AnnotationExporter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QFile>

namespace LunaRead {

AnnotationExportDialog::AnnotationExportDialog(const QString& bookTitle, const QList<AnnotationRecord>& annotations, QWidget* parent)
    : QDialog(parent)
    , m_bookTitle(bookTitle.isEmpty() ? tr("Untitled Book") : bookTitle)
    , m_annotations(annotations) {
    setupUi();
    updatePreview();
}

void AnnotationExportDialog::setupUi() {
    setWindowTitle(tr("Export Annotations - %1").arg(m_bookTitle));
    resize(700, 560);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // Header Controls
    auto* topLayout = new QHBoxLayout();
    auto* formatLabel = new QLabel(tr("<b>Export Format:</b>"), this);
    topLayout->addWidget(formatLabel);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("Markdown (.md)"), "md");
    m_formatCombo->addItem(tr("Obsidian Markdown Callouts (.md)"), "obsidian");
    m_formatCombo->addItem(tr("Anki Flashcards / CSV (.csv)"), "csv");
    m_formatCombo->addItem(tr("HTML Document (.html)"), "html");
    m_formatCombo->addItem(tr("Plain Text (.txt)"), "txt");
    m_formatCombo->addItem(tr("JSON Data (.json)"), "json");
    topLayout->addWidget(m_formatCombo);

    topLayout->addSpacing(16);

    int hlCount = 0, noteCount = 0, bmCount = 0;
    for (const auto& a : m_annotations) {
        if (a.type == "highlight" || a.type == "underline") hlCount++;
        else if (a.type == "note") noteCount++;
        else if (a.type == "bookmark") bmCount++;
    }

    m_includeHighlightsCheck = new QCheckBox(tr("Highlights (%1)").arg(hlCount), this);
    m_includeHighlightsCheck->setChecked(true);
    topLayout->addWidget(m_includeHighlightsCheck);

    m_includeNotesCheck = new QCheckBox(tr("Notes (%1)").arg(noteCount), this);
    m_includeNotesCheck->setChecked(true);
    topLayout->addWidget(m_includeNotesCheck);

    m_includeBookmarksCheck = new QCheckBox(tr("Bookmarks (%1)").arg(bmCount), this);
    m_includeBookmarksCheck->setChecked(true);
    topLayout->addWidget(m_includeBookmarksCheck);

    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    // Live Preview
    auto* previewLabel = new QLabel(tr("<b>Live Export Preview:</b>"), this);
    mainLayout->addWidget(previewLabel);

    m_previewEdit = new QTextEdit(this);
    m_previewEdit->setReadOnly(true);
    QFont monoFont("monospace", 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_previewEdit->setFont(monoFont);
    m_previewEdit->setStyleSheet("QTextEdit { background-color: #F9FAFB; border: 1px solid #D1D5DB; border-radius: 6px; padding: 8px; }");
    mainLayout->addWidget(m_previewEdit, 1);

    // Bottom Action Row
    auto* bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #059669; font-weight: bold;");
    bottomLayout->addWidget(m_statusLabel);
    bottomLayout->addStretch();

    m_copyBtn = new QPushButton(tr("📋 Copy to Clipboard"), this);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setStyleSheet("QPushButton { padding: 8px 16px; border-radius: 6px; background-color: #E5E7EB; border: 1px solid #D1D5DB; font-weight: 500; }"
                             "QPushButton:hover { background-color: #D1D5DB; }");
    bottomLayout->addWidget(m_copyBtn);

    m_saveBtn = new QPushButton(tr("💾 Save to File..."), this);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setStyleSheet("QPushButton { padding: 8px 18px; border-radius: 6px; background-color: #2563EB; color: white; border: none; font-weight: bold; }"
                             "QPushButton:hover { background-color: #1D4ED8; }");
    bottomLayout->addWidget(m_saveBtn);

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet("QPushButton { padding: 8px 16px; border-radius: 6px; background-color: transparent; border: 1px solid #D1D5DB; }"
                              "QPushButton:hover { background-color: #F3F4F6; }");
    bottomLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(bottomLayout);

    // Connect signals
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AnnotationExportDialog::updatePreview);
    connect(m_includeHighlightsCheck, &QCheckBox::toggled, this, &AnnotationExportDialog::updatePreview);
    connect(m_includeNotesCheck, &QCheckBox::toggled, this, &AnnotationExportDialog::updatePreview);
    connect(m_includeBookmarksCheck, &QCheckBox::toggled, this, &AnnotationExportDialog::updatePreview);
    connect(m_copyBtn, &QPushButton::clicked, this, &AnnotationExportDialog::onCopyToClipboard);
    connect(m_saveBtn, &QPushButton::clicked, this, &AnnotationExportDialog::onSaveToFile);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

QList<AnnotationRecord> AnnotationExportDialog::filteredAnnotations() const {
    QList<AnnotationRecord> list;
    const bool incHl = m_includeHighlightsCheck->isChecked();
    const bool incNote = m_includeNotesCheck->isChecked();
    const bool incBm = m_includeBookmarksCheck->isChecked();

    for (const auto& a : m_annotations) {
        if ((a.type == "highlight" || a.type == "underline" || a.type == "strikethrough") && incHl) list.append(a);
        else if (a.type == "note" && incNote) list.append(a);
        else if (a.type == "bookmark" && incBm) list.append(a);
    }
    return list;
}

void AnnotationExportDialog::updatePreview() {
    auto filtered = filteredAnnotations();
    const QString fmt = m_formatCombo->currentData().toString();
    QString result;

    if (fmt == "md") {
        result = AnnotationExporter::exportToMarkdown(m_bookTitle, filtered);
    } else if (fmt == "obsidian") {
        result = AnnotationExporter::exportToObsidian(m_bookTitle, filtered);
    } else if (fmt == "csv") {
        result = AnnotationExporter::exportToCsv(m_bookTitle, filtered);
    } else if (fmt == "html") {
        result = AnnotationExporter::exportToHtml(m_bookTitle, filtered);
    } else if (fmt == "txt") {
        result = AnnotationExporter::exportToPlainText(m_bookTitle, filtered);
    } else if (fmt == "json") {
        result = AnnotationExporter::exportToJson(m_bookTitle, filtered);
    }

    m_previewEdit->setPlainText(result);
    m_statusLabel->clear();
}

void AnnotationExportDialog::onCopyToClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(m_previewEdit->toPlainText());
    m_statusLabel->setText(tr("✓ Copied to clipboard!"));
}

void AnnotationExportDialog::onSaveToFile() {
    const QString fmt = m_formatCombo->currentData().toString();
    QString filter;
    QString defaultExt;

    if (fmt == "md" || fmt == "obsidian") {
        filter = tr("Markdown (*.md);;All Files (*)");
        defaultExt = ".md";
    } else if (fmt == "csv") {
        filter = tr("CSV Spreadsheets / Anki (*.csv);;All Files (*)");
        defaultExt = ".csv";
    } else if (fmt == "html") {
        filter = tr("HTML Documents (*.html *.htm);;All Files (*)");
        defaultExt = ".html";
    } else if (fmt == "txt") {
        filter = tr("Text Files (*.txt);;All Files (*)");
        defaultExt = ".txt";
    } else {
        filter = tr("JSON Files (*.json);;All Files (*)");
        defaultExt = ".json";
    }

    QString defaultName = m_bookTitle.simplified().replace(" ", "_") + "_annotations" + defaultExt;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Annotations"), defaultName, filter);
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(m_previewEdit->toPlainText().toUtf8());
        file.close();
        m_statusLabel->setText(tr("✓ Saved to %1").arg(QFileInfo(path).fileName()));
    } else {
        QMessageBox::critical(this, tr("Save Error"), tr("Could not write to file:\n%1").arg(path));
    }
}

void AnnotationExportDialog::applyTheme(const ReaderThemeTokens& tokens) {
    if (tokens.isDark) {
        setStyleSheet(QString("QDialog { background-color: %1; color: %2; }").arg(tokens.backgroundColor.name(), tokens.textColor.name()));
        m_previewEdit->setStyleSheet(QString("QTextEdit { background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; }")
            .arg(tokens.backgroundColor.darker(115).name(), tokens.textColor.name(), tokens.dividerColor.name()));
    }
}

} // namespace LunaRead
