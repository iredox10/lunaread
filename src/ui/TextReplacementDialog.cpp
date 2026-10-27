#include "TextReplacementDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QMessageBox>
#include <QUuid>

namespace LunaRead {

TextReplacementDialog::TextReplacementDialog(const QString& currentBookId, QWidget* parent)
    : QDialog(parent), m_bookId(currentBookId) {
    setWindowTitle(tr("Text Replacement & Content Filter Rules"));
    resize(760, 560);
    setupUi();
    refreshTable();
}

void TextReplacementDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto* descLabel = new QLabel(
        tr("Define string or regular expression replacement rules applied dynamically while reading.\n"
           "Useful for stripping translator notices/ads, correcting OCR typos, or customizing terminology."), this);
    descLabel->setStyleSheet("color: #64748B; font-size: 10pt;");
    mainLayout->addWidget(descLabel);

    // Rules Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Enabled"), tr("Pattern"), tr("Replacement"), tr("Type"), tr("Scope")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table);

    // Action buttons for table
    auto* tableBtnLayout = new QHBoxLayout();
    m_deleteBtn = new QPushButton(tr("🗑️ Delete Selected"), this);
    connect(m_deleteBtn, &QPushButton::clicked, this, &TextReplacementDialog::onDeleteRule);
    tableBtnLayout->addWidget(m_deleteBtn);
    tableBtnLayout->addStretch();
    mainLayout->addLayout(tableBtnLayout);

    // New Rule Group
    auto* addGroup = new QGroupBox(tr("Add New Filter Rule"), this);
    auto* addLayout = new QVBoxLayout(addGroup);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(new QLabel(tr("Pattern:"), this));
    m_patternEdit = new QLineEdit(this);
    m_patternEdit->setPlaceholderText(tr("Text or Regex pattern..."));
    row1->addWidget(m_patternEdit);

    row1->addWidget(new QLabel(tr("Replace with:"), this));
    m_replacementEdit = new QLineEdit(this);
    m_replacementEdit->setPlaceholderText(tr("Replacement text (or empty to remove)"));
    row1->addWidget(m_replacementEdit);
    addLayout->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    m_regexCheck = new QCheckBox(tr("Regular Expression (Regex)"), this);
    m_caseCheck = new QCheckBox(tr("Case Sensitive"), this);
    m_globalScopeCheck = new QCheckBox(tr("Global Rule (Applies to all books)"), this);
    m_globalScopeCheck->setChecked(true);
    m_addBtn = new QPushButton(tr("➕ Add Rule"), this);
    m_addBtn->setStyleSheet("background-color: #2563EB; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(m_addBtn, &QPushButton::clicked, this, &TextReplacementDialog::onAddRule);

    row2->addWidget(m_regexCheck);
    row2->addWidget(m_caseCheck);
    row2->addWidget(m_globalScopeCheck);
    row2->addStretch();
    row2->addWidget(m_addBtn);
    addLayout->addLayout(row2);
    mainLayout->addWidget(addGroup);

    // Live Test Area
    auto* testGroup = new QGroupBox(tr("Live Test Playground"), this);
    auto* testLayout = new QHBoxLayout(testGroup);

    auto* inCol = new QVBoxLayout();
    inCol->addWidget(new QLabel(tr("Sample Input Text:"), this));
    m_testInput = new QTextEdit(this);
    m_testInput->setPlainText(tr("Visit http://pirate-novel.com for more updates!\n\n\n\nChapter 1\nIt was a dark and stormy night...   "));
    m_testInput->setMaximumHeight(80);
    inCol->addWidget(m_testInput);
    testLayout->addLayout(inCol);

    auto* outCol = new QVBoxLayout();
    outCol->addWidget(new QLabel(tr("Filtered Output Preview:"), this));
    m_testOutput = new QTextEdit(this);
    m_testOutput->setReadOnly(true);
    m_testOutput->setMaximumHeight(80);
    outCol->addWidget(m_testOutput);
    testLayout->addLayout(outCol);

    mainLayout->addWidget(testGroup);

    connect(m_testInput, &QTextEdit::textChanged, this, &TextReplacementDialog::onTestLive);

    auto* closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    onTestLive();
}

void TextReplacementDialog::refreshTable() {
    m_table->setRowCount(0);
    const auto rules = TextFilterManager::instance().getRules(m_bookId);
    m_table->setRowCount(static_cast<int>(rules.size()));

    for (int i = 0; i < rules.size(); ++i) {
        const auto& r = rules[i];

        auto* checkItem = new QTableWidgetItem();
        checkItem->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        checkItem->setData(Qt::UserRole, r.id);
        m_table->setItem(i, 0, checkItem);

        auto* patternItem = new QTableWidgetItem(r.pattern);
        m_table->setItem(i, 1, patternItem);

        auto* replItem = new QTableWidgetItem(r.replacement);
        m_table->setItem(i, 2, replItem);

        auto* typeItem = new QTableWidgetItem(r.isRegex ? tr("Regex") : tr("Plain"));
        m_table->setItem(i, 3, typeItem);

        auto* scopeItem = new QTableWidgetItem(r.scope == QStringLiteral("global") ? tr("Global") : tr("Current Book"));
        m_table->setItem(i, 4, scopeItem);
    }

    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() == 0) {
            QString ruleId = item->data(Qt::UserRole).toString();
            TextFilterManager::instance().setRuleEnabled(ruleId, item->checkState() == Qt::Checked);
            onTestLive();
        }
    });
}

void TextReplacementDialog::onAddRule() {
    QString pat = m_patternEdit->text();
    if (pat.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Rule"), tr("Pattern cannot be empty."));
        return;
    }

    FilterRule r;
    r.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.pattern = pat;
    r.replacement = m_replacementEdit->text();
    r.isRegex = m_regexCheck->isChecked();
    r.caseSensitive = m_caseCheck->isChecked();
    r.scope = (m_globalScopeCheck->isChecked() || m_bookId.isEmpty()) ? QStringLiteral("global") : m_bookId;
    r.enabled = true;

    TextFilterManager::instance().addRule(r);
    m_patternEdit->clear();
    m_replacementEdit->clear();
    refreshTable();
    onTestLive();
}

void TextReplacementDialog::onDeleteRule() {
    int row = m_table->currentRow();
    if (row < 0) return;

    auto* item = m_table->item(row, 0);
    if (!item) return;

    QString ruleId = item->data(Qt::UserRole).toString();
    TextFilterManager::instance().deleteRule(ruleId);
    refreshTable();
    onTestLive();
}

void TextReplacementDialog::onToggleRule(int row) {
    Q_UNUSED(row);
}

void TextReplacementDialog::onTestLive() {
    QString sample = m_testInput->toPlainText();
    QString filtered = TextFilterManager::instance().applyFilters(sample, m_bookId);
    m_testOutput->setPlainText(filtered);
}

} // namespace LunaRead
