#include "SearchInBookDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

namespace LunaRead {

SearchInBookDialog::SearchInBookDialog(const BookDocument& document, QWidget* parent)
    : QDialog(parent), m_document(document) {
    setWindowTitle(tr("Search in Book - LunaRead"));
    resize(520, 480);
    setupUi();
}

void SearchInBookDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    auto* topLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search text across all chapters..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchInBookDialog::performSearch);

    m_searchBtn = new QPushButton(tr("Search"), this);
    connect(m_searchBtn, &QPushButton::clicked, this, &SearchInBookDialog::performSearch);

    m_prevBtn = new QPushButton(tr("▲ Prev"), this);
    m_prevBtn->setEnabled(false);
    connect(m_prevBtn, &QPushButton::clicked, this, &SearchInBookDialog::onPrevMatch);

    m_nextBtn = new QPushButton(tr("▼ Next"), this);
    m_nextBtn->setEnabled(false);
    connect(m_nextBtn, &QPushButton::clicked, this, &SearchInBookDialog::onNextMatch);

    topLayout->addWidget(m_searchEdit, 1);
    topLayout->addWidget(m_searchBtn);
    topLayout->addWidget(m_prevBtn);
    topLayout->addWidget(m_nextBtn);
    mainLayout->addLayout(topLayout);

    auto* optLayout = new QHBoxLayout();
    m_caseSensitiveCheck = new QCheckBox(tr("Match Case"), this);
    m_wholeWordCheck = new QCheckBox(tr("Whole Words Only"), this);
    m_regexCheck = new QCheckBox(tr("Regular Expression (Regex)"), this);
    m_exportBtn = new QPushButton(tr("📤 Export Matches..."), this);
    m_exportBtn->setEnabled(false);
    connect(m_exportBtn, &QPushButton::clicked, this, &SearchInBookDialog::onExportMatches);

    m_statusLabel = new QLabel(tr("Ready to search"), this);
    m_statusLabel->setStyleSheet("color: #64748B; font-size: 9pt;");

    optLayout->addWidget(m_caseSensitiveCheck);
    optLayout->addWidget(m_wholeWordCheck);
    optLayout->addWidget(m_regexCheck);
    optLayout->addWidget(m_exportBtn);
    optLayout->addStretch(1);
    optLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(optLayout);

    m_resultsList = new QListWidget(this);
    m_resultsList->setAlternatingRowColors(true);
    connect(m_resultsList, &QListWidget::itemDoubleClicked, this, &SearchInBookDialog::onResultItemDoubleClicked);
    mainLayout->addWidget(m_resultsList, 1);
}

void SearchInBookDialog::performSearch() {
    QString query = m_searchEdit->text().trimmed();
    m_resultsList->clear();
    m_matches.clear();
    m_currentMatchIndex = -1;

    if (query.isEmpty()) {
        m_statusLabel->setText(tr("Ready to search"));
        m_prevBtn->setEnabled(false);
        m_nextBtn->setEnabled(false);
        return;
    }

    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (!m_caseSensitiveCheck->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }

    QString pattern;
    if (m_regexCheck->isChecked()) {
        pattern = query;
    } else {
        pattern = QRegularExpression::escape(query);
        if (m_wholeWordCheck->isChecked()) {
            pattern = QStringLiteral("\\b%1\\b").arg(pattern);
        }
    }

    QRegularExpression regex(pattern, options);
    if (!regex.isValid()) {
        m_statusLabel->setText(tr("Invalid regular expression pattern."));
        return;
    }

    for (int chIdx = 0; chIdx < m_document.chapters.size(); ++chIdx) {
        const auto& ch = m_document.chapters[chIdx];
        const QString& text = ch.textContent.isEmpty() ? ch.htmlContent : ch.textContent;

        auto matchIt = regex.globalMatch(text);
        while (matchIt.hasNext()) {
            auto m = matchIt.next();
            SearchMatchResult res;
            res.chapterIndex = chIdx;
            res.chapterTitle = ch.title.isEmpty() ? tr("Chapter %1").arg(chIdx + 1) : ch.title;
            res.charOffset = static_cast<int>(m.capturedStart());
            res.matchLength = static_cast<int>(m.capturedLength());
            res.snippet = createSnippet(text, res.charOffset, res.matchLength);
            m_matches.append(res);

            auto* item = new QListWidgetItem();
            item->setText(QStringLiteral("[%1] %2").arg(res.chapterTitle, res.snippet));
            item->setData(Qt::UserRole, m_matches.size() - 1);
            m_resultsList->addItem(item);
        }
    }

    if (m_matches.isEmpty()) {
        m_statusLabel->setText(tr("No matches found for '%1'").arg(query));
        m_prevBtn->setEnabled(false);
        m_nextBtn->setEnabled(false);
        m_exportBtn->setEnabled(false);
    } else {
        m_statusLabel->setText(tr("%1 matches found").arg(m_matches.size()));
        m_prevBtn->setEnabled(true);
        m_nextBtn->setEnabled(true);
        m_exportBtn->setEnabled(true);
        m_currentMatchIndex = 0;
        m_resultsList->setCurrentRow(0);
    }
}

void SearchInBookDialog::onExportMatches() {
    if (m_matches.isEmpty()) return;
    QString path = QFileDialog::getSaveFileName(this, tr("Export Search Results"),
        QStringLiteral("search_results.txt"), tr("Text Files (*.txt);;Markdown (*.md);;All Files (*.*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not write to file %1").arg(path));
        return;
    }

    QTextStream out(&file);
    out << "Search Results for: " << m_searchEdit->text() << "\n";
    out << "Book: " << m_document.title << "\n";
    out << "Total Matches: " << m_matches.size() << "\n\n";

    for (int i = 0; i < m_matches.size(); ++i) {
        const auto& m = m_matches[i];
        out << QStringLiteral("%1. [%2] %3\n").arg(i + 1).arg(m.chapterTitle, m.snippet);
    }

    QMessageBox::information(this, tr("Export Complete"), tr("Saved %1 search matches to %2").arg(m_matches.size()).arg(path));
}

QString SearchInBookDialog::createSnippet(const QString& fullText, int matchPos, int matchLen) const {
    constexpr int contextLen = 35;
    int start = std::max(0, matchPos - contextLen);
    int end = std::min(static_cast<int>(fullText.length()), matchPos + matchLen + contextLen);

    QString prefix = (start > 0) ? QStringLiteral("...") : QString();
    QString suffix = (end < fullText.length()) ? QStringLiteral("...") : QString();
    QString snippet = fullText.mid(start, end - start).simplified();
    return prefix + snippet + suffix;
}

void SearchInBookDialog::onNextMatch() {
    if (m_matches.isEmpty()) return;
    m_currentMatchIndex = (m_currentMatchIndex + 1) % m_matches.size();
    m_resultsList->setCurrentRow(m_currentMatchIndex);
    const auto& match = m_matches[m_currentMatchIndex];
    emit matchSelected(match.chapterIndex, match.charOffset, match.matchLength);
}

void SearchInBookDialog::onPrevMatch() {
    if (m_matches.isEmpty()) return;
    m_currentMatchIndex = (m_currentMatchIndex - 1 + m_matches.size()) % m_matches.size();
    m_resultsList->setCurrentRow(m_currentMatchIndex);
    const auto& match = m_matches[m_currentMatchIndex];
    emit matchSelected(match.chapterIndex, match.charOffset, match.matchLength);
}

void SearchInBookDialog::onResultItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < m_matches.size()) {
        m_currentMatchIndex = idx;
        const auto& match = m_matches[idx];
        emit matchSelected(match.chapterIndex, match.charOffset, match.matchLength);
    }
}

void SearchInBookDialog::applyTheme(const ReaderThemeTokens& tokens) {
    QString qss = QStringLiteral(
        "QDialog { background-color: %1; color: %2; }"
        "QLineEdit { background-color: %3; color: %2; border: 1px solid %4; border-radius: 4px; padding: 6px; }"
        "QListWidget { background-color: %3; color: %2; border: 1px solid %4; border-radius: 4px; }"
        "QPushButton { background-color: %5; color: white; border-radius: 4px; padding: 6px 12px; }"
    ).arg(tokens.backgroundColor.name(),
         tokens.textColor.name(),
         tokens.headerFooterColor.name(),
         tokens.dividerColor.name(),
         tokens.accentColor.name());
    setStyleSheet(qss);
}

} // namespace LunaRead
