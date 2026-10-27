#include "VocabularyNotebookDialog.h"
#include "../db/DatabaseManager.h"
#include "../features/TtsService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

namespace LunaRead {

VocabularyNotebookDialog::VocabularyNotebookDialog(QWidget* parent)
    : QDialog(parent), m_tokens(ReaderThemeTokens::parchment()) {
    setWindowTitle(tr("📚 Vocabulary Notebook & Flashcards"));
    resize(760, 540);
    setModal(true);

    setupUi();
    reloadWords();
    applyTheme(m_tokens);
}

void VocabularyNotebookDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(14);

    // 1. Header Toolbar
    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(10);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("🔍 Search vocabulary words, definitions, or books..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &VocabularyNotebookDialog::onSearchChanged);
    topLayout->addWidget(m_searchEdit, 2);

    m_quizBtn = new QPushButton(tr("🎴 Flashcard Quiz Mode"), this);
    m_quizBtn->setCheckable(true);
    m_quizBtn->setCursor(Qt::PointingHandCursor);
    connect(m_quizBtn, &QPushButton::clicked, this, &VocabularyNotebookDialog::onToggleQuizMode);
    topLayout->addWidget(m_quizBtn);

    mainLayout->addLayout(topLayout);

    // 2. Count Label
    m_countLabel = new QLabel(tr("Loading vocabulary words..."), this);
    m_countLabel->setStyleSheet("font-size: 12px; color: #78909c; font-weight: bold;");
    mainLayout->addWidget(m_countLabel);

    // 3. Word List Widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_listWidget->setSpacing(8);
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mainLayout->addWidget(m_listWidget, 1);

    // 4. Bottom Actions
    auto* btmLayout = new QHBoxLayout();
    btmLayout->setSpacing(12);

    m_clearBtn = new QPushButton(tr("🗑️ Clear Notebook"), this);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet("color: #e53935;");
    connect(m_clearBtn, &QPushButton::clicked, this, &VocabularyNotebookDialog::onClearAll);
    btmLayout->addWidget(m_clearBtn);

    btmLayout->addStretch();

    m_exportBtn = new QPushButton(tr("📤 Export CSV / Anki..."), this);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportBtn, &QPushButton::clicked, this, &VocabularyNotebookDialog::onExport);
    btmLayout->addWidget(m_exportBtn);

    auto* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btmLayout->addWidget(closeBtn);

    mainLayout->addLayout(btmLayout);
}

void VocabularyNotebookDialog::reloadWords() {
    m_words = DatabaseManager::instance().getAllVocabularyWords();
    onSearchChanged(m_searchEdit ? m_searchEdit->text() : QString());
}

void VocabularyNotebookDialog::onSearchChanged(const QString& query) {
    m_listWidget->clear();
    const QString filter = query.trimmed();

    int count = 0;
    for (const auto& w : m_words) {
        if (!filter.isEmpty()) {
            bool matchWord = w.word.contains(filter, Qt::CaseInsensitive);
            bool matchDef = w.definition.contains(filter, Qt::CaseInsensitive);
            bool matchBook = w.bookTitle.contains(filter, Qt::CaseInsensitive);
            if (!matchWord && !matchDef && !matchBook) continue;
        }

        auto* item = new QListWidgetItem(m_listWidget);
        QWidget* rowWidget = createWordRowWidget(w, m_quizMode);
        item->setSizeHint(rowWidget->sizeHint());
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, rowWidget);
        count++;
    }

    m_countLabel->setText(tr("Showing %1 of %2 vocabulary words in Notebook%3")
        .arg(count)
        .arg(m_words.size())
        .arg(m_quizMode ? tr(" (Quiz Mode Active: Click to reveal definition)") : QString()));
}

QWidget* VocabularyNotebookDialog::createWordRowWidget(const VocabularyWord& w, bool isQuiz) {
    auto* container = new QFrame();
    container->setObjectName("vocabCard");
    container->setFrameShape(QFrame::StyledPanel);

    auto* rowLayout = new QHBoxLayout(container);
    rowLayout->setContentsMargins(16, 12, 16, 12);
    rowLayout->setSpacing(14);

    // Left word details
    auto* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(6);

    auto* wordLine = new QHBoxLayout();
    auto* wordLabel = new QLabel(w.word, container);
    wordLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1565c0;");
    wordLine->addWidget(wordLabel);

    if (!w.phonetic.trimmed().isEmpty()) {
        auto* phoLabel = new QLabel(w.phonetic, container);
        phoLabel->setStyleSheet("font-size: 13px; color: #78909c; font-style: italic;");
        wordLine->addWidget(phoLabel);
    }

    auto* pronBtn = new QPushButton(tr("🔊"), container);
    pronBtn->setCursor(Qt::PointingHandCursor);
    pronBtn->setFixedSize(26, 26);
    pronBtn->setToolTip(tr("Listen to pronunciation"));
    connect(pronBtn, &QPushButton::clicked, this, [w]() {
        TtsService tts;
        tts.speakText(w.word);
    });
    wordLine->addWidget(pronBtn);

    wordLine->addStretch();
    leftLayout->addLayout(wordLine);

    // Definition & Example
    if (isQuiz) {
        auto* revealBtn = new QPushButton(tr("👁️ Click to Reveal Definition"), container);
        revealBtn->setCursor(Qt::PointingHandCursor);
        revealBtn->setStyleSheet("text-align: left; padding: 6px 10px; font-size: 12px; font-style: italic; color: #78909c;");

        auto* defLabel = new QLabel(w.definition, container);
        defLabel->setWordWrap(true);
        defLabel->setStyleSheet("font-size: 13px; color: #37474f; line-height: 1.4;");
        defLabel->hide();

        connect(revealBtn, &QPushButton::clicked, revealBtn, [revealBtn, defLabel]() {
            revealBtn->hide();
            defLabel->show();
        });

        leftLayout->addWidget(revealBtn);
        leftLayout->addWidget(defLabel);
    } else {
        auto* defLabel = new QLabel(w.definition, container);
        defLabel->setWordWrap(true);
        defLabel->setStyleSheet("font-size: 13px; color: #37474f; line-height: 1.4;");
        leftLayout->addWidget(defLabel);

        if (!w.example.trimmed().isEmpty()) {
            auto* exLabel = new QLabel(QString("“%1”").arg(w.example), container);
            exLabel->setWordWrap(true);
            exLabel->setStyleSheet("font-size: 12px; font-style: italic; color: #607d8b;");
            leftLayout->addWidget(exLabel);
        }
    }

    // Origin provenance & date
    auto* subLine = new QHBoxLayout();
    if (!w.bookTitle.isEmpty()) {
        auto* bookLabel = new QLabel(QString("📖 %1").arg(w.bookTitle), container);
        bookLabel->setStyleSheet("font-size: 11px; color: #90a4ae;");
        subLine->addWidget(bookLabel);
    }
    if (w.dateAdded > 0) {
        auto dt = QDateTime::fromSecsSinceEpoch(w.dateAdded);
        auto* dateLabel = new QLabel(dt.toString("yyyy-MM-dd"), container);
        dateLabel->setStyleSheet("font-size: 11px; color: #b0bec5;");
        subLine->addWidget(dateLabel);
    }
    subLine->addStretch();
    leftLayout->addLayout(subLine);

    rowLayout->addLayout(leftLayout, 1);

    // Delete Button
    auto* delBtn = new QPushButton(tr("🗑️"), container);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setFixedSize(28, 28);
    delBtn->setToolTip(tr("Remove from Vocabulary Notebook"));
    delBtn->setStyleSheet("color: #e53935; font-size: 13px;");
    connect(delBtn, &QPushButton::clicked, this, [this, w]() {
        onDeleteWord(w.id);
    });
    rowLayout->addWidget(delBtn, 0, Qt::AlignTop);

    return container;
}

void VocabularyNotebookDialog::onDeleteWord(const QString& id) {
    DatabaseManager::instance().deleteVocabularyWord(id);
    reloadWords();
}

void VocabularyNotebookDialog::onClearAll() {
    if (m_words.isEmpty()) return;
    auto reply = QMessageBox::question(
        this,
        tr("Clear Vocabulary Notebook"),
        tr("Are you sure you want to clear all %1 words in your notebook?").arg(m_words.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        DatabaseManager::instance().clearVocabularyWords();
        reloadWords();
    }
}

void VocabularyNotebookDialog::onToggleQuizMode() {
    m_quizMode = m_quizBtn->isChecked();
    m_quizBtn->setText(m_quizMode ? tr("🎴 Exit Quiz Mode") : tr("🎴 Flashcard Quiz Mode"));
    onSearchChanged(m_searchEdit->text());
}

void VocabularyNotebookDialog::onExport() {
    if (m_words.isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("No vocabulary words to export."));
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Vocabulary Notebook"),
        QString("lunaread_vocabulary_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd")),
        tr("CSV Spreadsheets (*.csv);;Anki Flashcards TSV (*.txt);;Markdown Notes (*.md)"));

    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"), tr("Failed to open file for writing."));
        return;
    }

    QTextStream out(&file);
    if (savePath.endsWith(".md", Qt::CaseInsensitive)) {
        out << "# LunaRead Vocabulary Notebook\n\n";
        for (const auto& w : m_words) {
            out << "### " << w.word << " " << w.phonetic << "\n";
            out << "- **Definition:** " << w.definition << "\n";
            if (!w.example.isEmpty()) out << "- *Example:* " << w.example << "\n";
            if (!w.bookTitle.isEmpty()) out << "- *Source:* " << w.bookTitle << "\n";
            out << "\n---\n\n";
        }
    } else {
        // CSV / TSV format
        out << "Word,Phonetic,Definition,Example,SourceBook\n";
        for (const auto& w : m_words) {
            QString cleanWord = w.word;
            QString cleanPho = w.phonetic;
            QString cleanDef = QString(w.definition).replace("\"", "\"\"");
            QString cleanEx = QString(w.example).replace("\"", "\"\"");
            QString cleanBook = QString(w.bookTitle).replace("\"", "\"\"");
            out << "\"" << cleanWord << "\",\"" << cleanPho << "\",\"" << cleanDef << "\",\"" << cleanEx << "\",\"" << cleanBook << "\"\n";
        }
    }

    file.close();
    QMessageBox::information(this, tr("Export Completed"), tr("Successfully exported %1 vocabulary words to:\n%2").arg(m_words.size()).arg(savePath));
}

void VocabularyNotebookDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    setStyleSheet(QString(
        "QDialog { background-color: %1; color: %2; }"
        "QListWidget { background-color: transparent; border: none; }"
        "QListWidget::item { border: none; padding: 2px 0; }"
        "QFrame#vocabCard { background-color: %3; border: 1px solid %4; border-radius: 8px; }"
        "QLineEdit { background-color: %5; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 10px; }"
        "QPushButton { background-color: %5; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: %6; }"
        "QPushButton:checked { background-color: %7; color: #ffffff; }"
    ).arg(tokens.backgroundColor.name(),
         tokens.textColor.name(),
         tokens.isDark ? "#242424" : "#ffffff",
         tokens.isDark ? "#383838" : "#e0e0e0",
         tokens.isDark ? "#2d2d2d" : "#f5f5f5",
         tokens.isDark ? "#3a3a3a" : "#eaeaea",
         tokens.accentColor.name()));
}

} // namespace LunaRead
