#include "DictionaryDialog.h"
#include "../db/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QComboBox>
#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QGuiApplication>
#include <QClipboard>
#include <QSignalBlocker>
#include <QFont>

namespace LunaRead {

DictionaryDialog::DictionaryDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Dictionary - LunaRead"));
    resize(520, 560);
    setMinimumSize(440, 460);

    setupUi();
    applyTheme(ReaderThemeTokens::modernLight());
}

DictionaryDialog::DictionaryDialog(const QString& word,
                                   DictionaryService& service,
                                   QWidget* parent)
    : DictionaryDialog(parent)
{
    m_service = &service;
    m_currentWord = word;
    lookup(word, m_currentLanguage);
}

DictionaryDialog::DictionaryDialog(const QString& word,
                                   const QString& language,
                                   DictionaryService& service,
                                   QWidget* parent)
    : DictionaryDialog(parent)
{
    m_service = &service;
    m_currentWord = word;
    m_currentLanguage = language.trimmed().isEmpty() ? QStringLiteral("en") : language.trimmed().toLower();
    setLanguage(m_currentLanguage);
    lookup(word, m_currentLanguage);
}

DictionaryDialog::DictionaryDialog(const QString& word,
                                   DictionaryService* service,
                                   QWidget* parent)
    : DictionaryDialog(parent)
{
    m_service = service;
    m_currentWord = word;
    lookup(word, m_currentLanguage);
}

DictionaryDialog::DictionaryDialog(const QString& word,
                                   const QString& language,
                                   DictionaryService* service,
                                   QWidget* parent)
    : DictionaryDialog(parent)
{
    m_service = service;
    m_currentWord = word;
    m_currentLanguage = language.trimmed().isEmpty() ? QStringLiteral("en") : language.trimmed().toLower();
    setLanguage(m_currentLanguage);
    lookup(word, m_currentLanguage);
}

void DictionaryDialog::setDictionaryService(DictionaryService* service) {
    m_service = service;
    if (!m_currentWord.isEmpty()) {
        refreshLookup();
    }
}

void DictionaryDialog::setWord(const QString& word) {
    m_currentWord = word.trimmed();
    if (m_searchEdit && m_searchEdit->text() != m_currentWord) {
        const QSignalBlocker blocker(m_searchEdit);
        m_searchEdit->setText(m_currentWord);
    }
    lookup(m_currentWord, m_currentLanguage);
}

void DictionaryDialog::setLanguage(const QString& language) {
    m_currentLanguage = language.trimmed().isEmpty() ? QStringLiteral("en") : language.trimmed().toLower();
    if (m_langCombo) {
        int idx = m_langCombo->findData(m_currentLanguage);
        if (idx >= 0 && m_langCombo->currentIndex() != idx) {
            const QSignalBlocker blocker(m_langCombo);
            m_langCombo->setCurrentIndex(idx);
        }
    }
}

void DictionaryDialog::lookup(const QString& word, const QString& language) {
    m_currentWord = word.trimmed();
    if (!language.isEmpty()) {
        m_currentLanguage = language.trimmed().toLower();
        setLanguage(m_currentLanguage);
    }

    if (m_searchEdit && m_searchEdit->text() != m_currentWord) {
        const QSignalBlocker blocker(m_searchEdit);
        m_searchEdit->setText(m_currentWord);
    }

    if (!m_service) {
        DictEntry fallback;
        fallback.word = m_currentWord;
        fallback.definition = tr("Dictionary service unavailable.");
        fallback.found = false;
        m_currentEntry = fallback;
        updateContentUi(fallback);
        emit wordLookedUp(m_currentWord, false);
        return;
    }

    if (m_currentWord.isEmpty()) {
        DictEntry emptyEntry;
        emptyEntry.definition = tr("Enter a word above and click Look Up.");
        emptyEntry.found = false;
        m_currentEntry = emptyEntry;
        updateContentUi(emptyEntry);
        emit wordLookedUp(QString(), false);
        return;
    }

    m_currentEntry = m_service->lookup(m_currentWord, m_currentLanguage);
    updateContentUi(m_currentEntry);
    emit wordLookedUp(m_currentWord, m_currentEntry.found);
}

void DictionaryDialog::refreshLookup() {
    lookup(m_currentWord, m_currentLanguage);
}

void DictionaryDialog::onSearchTriggered() {
    const QString query = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    lookup(query, m_currentLanguage);
}

void DictionaryDialog::onLanguageChanged(int index) {
    if (index < 0 || !m_langCombo) return;
    m_currentLanguage = m_langCombo->itemData(index).toString();
    if (!m_currentWord.isEmpty()) {
        lookup(m_currentWord, m_currentLanguage);
    }
}

void DictionaryDialog::onSynonymClicked(const QString& synonym) {
    lookup(synonym, m_currentLanguage);
}

void DictionaryDialog::onCopyClicked() {
    QString text;
    if (!m_currentEntry.word.isEmpty()) {
        text += m_currentEntry.word;
        if (!m_currentEntry.phonetic.isEmpty()) {
            text += QStringLiteral(" ") + m_currentEntry.phonetic;
        }
        if (!m_currentEntry.partOfSpeech.isEmpty()) {
            text += QStringLiteral(" [") + m_currentEntry.partOfSpeech + QStringLiteral("]");
        }
        text += QStringLiteral("\n\n");
    }
    if (!m_currentEntry.definition.isEmpty()) {
        text += tr("Definition:\n") + m_currentEntry.definition + QStringLiteral("\n\n");
    }
    if (!m_currentEntry.example.isEmpty()) {
        text += tr("Example:\n\"") + m_currentEntry.example + QStringLiteral("\"\n\n");
    }
    if (!m_currentEntry.synonyms.isEmpty()) {
        text += tr("Synonyms:\n") + m_currentEntry.synonyms.join(QStringLiteral(", ")) + QStringLiteral("\n");
    }

    if (auto* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text.trimmed());
    }

    m_copyBtn->setText(tr("✓ Copied!"));
    QTimer::singleShot(1500, this, [this]() {
        if (m_copyBtn) {
            m_copyBtn->setText(tr("Copy Entry"));
        }
    });
}

void DictionaryDialog::onSaveVocabularyClicked() {
    if (m_currentWord.isEmpty()) return;
    VocabularyWord v;
    v.id = QString("vocab_%1_%2").arg(m_currentWord).arg(QDateTime::currentMSecsSinceEpoch());
    v.word = m_currentWord;
    v.phonetic = m_currentEntry.phonetic;
    v.definition = m_currentEntry.definition;
    v.example = m_currentEntry.example;
    v.bookId = m_bookId;
    v.bookTitle = m_bookTitle;
    v.dateAdded = QDateTime::currentSecsSinceEpoch();

    DatabaseManager::instance().insertVocabularyWord(v);
    if (m_saveVocabBtn) {
        m_saveVocabBtn->setText(tr("✅ Saved!"));
        m_saveVocabBtn->setEnabled(false);
    }
    emit wordSavedToVocabulary(m_currentWord);
}

void DictionaryDialog::populateLanguageCombo() {
    m_langCombo->clear();
    m_langCombo->addItem(tr("English (en)"), QStringLiteral("en"));
    m_langCombo->addItem(tr("French (fr)"), QStringLiteral("fr"));
    m_langCombo->addItem(tr("Spanish (es)"), QStringLiteral("es"));
    m_langCombo->addItem(tr("German (de)"), QStringLiteral("de"));
    m_langCombo->addItem(tr("Italian (it)"), QStringLiteral("it"));
    m_langCombo->addItem(tr("Chinese (zh)"), QStringLiteral("zh"));
    m_langCombo->addItem(tr("Japanese (ja)"), QStringLiteral("ja"));
    m_langCombo->addItem(tr("Latin (la)"), QStringLiteral("la"));
}

void DictionaryDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // 1. Top Search and Controls Row
    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Look up a word..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &DictionaryDialog::onSearchTriggered);

    m_searchBtn = new QPushButton(tr("Look Up"), this);
    m_searchBtn->setObjectName(QStringLiteral("searchBtn"));
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    connect(m_searchBtn, &QPushButton::clicked, this, &DictionaryDialog::onSearchTriggered);

    m_langCombo = new QComboBox(this);
    m_langCombo->setToolTip(tr("Select dictionary language"));
    populateLanguageCombo();
    connect(m_langCombo, &QComboBox::currentIndexChanged, this, &DictionaryDialog::onLanguageChanged);

    m_statusBadge = new QLabel(tr("● Offline Lexicon"), this);
    m_statusBadge->setToolTip(tr("Offline lexicon is active"));
    m_statusBadge->setAlignment(Qt::AlignCenter);

    topLayout->addWidget(m_searchEdit, 1);
    topLayout->addWidget(m_searchBtn);
    topLayout->addWidget(m_langCombo);
    topLayout->addWidget(m_statusBadge);
    mainLayout->addLayout(topLayout);

    // Top Divider
    m_topDivider = new QFrame(this);
    m_topDivider->setFrameShape(QFrame::HLine);
    m_topDivider->setFrameShadow(QFrame::Plain);
    m_topDivider->setFixedHeight(1);
    mainLayout->addWidget(m_topDivider);

    // 2. Middle Scrollable Area
    auto* contentScroll = new QScrollArea(this);
    contentScroll->setWidgetResizable(true);
    contentScroll->setFrameShape(QFrame::NoFrame);
    contentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* contentContainer = new QWidget(contentScroll);
    auto* scrollLayout = new QVBoxLayout(contentContainer);
    scrollLayout->setContentsMargins(2, 4, 2, 4);
    scrollLayout->setSpacing(14);

    // Word Header Row
    auto* wordHeaderLayout = new QVBoxLayout();
    wordHeaderLayout->setSpacing(4);

    m_wordTitleLabel = new QLabel(contentContainer);
    QFont titleFont = font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    m_wordTitleLabel->setFont(titleFont);
    m_wordTitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    wordHeaderLayout->addWidget(m_wordTitleLabel);

    auto* metaLayout = new QHBoxLayout();
    metaLayout->setSpacing(8);

    m_phoneticLabel = new QLabel(contentContainer);
    QFont phoneticFont = font();
    phoneticFont.setPointSize(11);
    phoneticFont.setItalic(true);
    m_phoneticLabel->setFont(phoneticFont);
    m_phoneticLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_posBadge = new QLabel(contentContainer);
    m_posBadge->setAlignment(Qt::AlignCenter);

    m_pronounceBtn = new QPushButton(tr("🔊 Pronounce"), contentContainer);
    m_pronounceBtn->setObjectName(QStringLiteral("pronounceBtn"));
    m_pronounceBtn->setCursor(Qt::PointingHandCursor);
    m_pronounceBtn->setToolTip(tr("Listen to pronunciation"));
    connect(m_pronounceBtn, &QPushButton::clicked, this, [this]() {
        emit pronounceRequested(m_currentWord);
    });

    m_saveVocabBtn = new QPushButton(tr("⭐ Save to Notebook"), contentContainer);
    m_saveVocabBtn->setObjectName(QStringLiteral("saveVocabBtn"));
    m_saveVocabBtn->setCursor(Qt::PointingHandCursor);
    m_saveVocabBtn->setToolTip(tr("Save to Vocabulary Notebook for study"));
    connect(m_saveVocabBtn, &QPushButton::clicked, this, &DictionaryDialog::onSaveVocabularyClicked);

    metaLayout->addWidget(m_phoneticLabel);
    metaLayout->addWidget(m_posBadge);
    metaLayout->addWidget(m_pronounceBtn);
    metaLayout->addWidget(m_saveVocabBtn);
    metaLayout->addStretch(1);
    wordHeaderLayout->addLayout(metaLayout);
    scrollLayout->addLayout(wordHeaderLayout);

    // Definition Card
    m_definitionCard = new QFrame(contentContainer);
    auto* defLayout = new QVBoxLayout(m_definitionCard);
    defLayout->setContentsMargins(14, 12, 14, 14);
    defLayout->setSpacing(8);

    m_defHeaderLabel = new QLabel(tr("DEFINITION"), m_definitionCard);
    m_defBrowser = new QTextBrowser(m_definitionCard);
    m_defBrowser->setOpenExternalLinks(false);
    m_defBrowser->setFrameShape(QFrame::NoFrame);
    m_defBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_defBrowser->setMinimumHeight(70);

    defLayout->addWidget(m_defHeaderLabel);
    defLayout->addWidget(m_defBrowser);
    scrollLayout->addWidget(m_definitionCard);

    // Examples Card
    m_exampleCard = new QFrame(contentContainer);
    auto* exampleLayout = new QVBoxLayout(m_exampleCard);
    exampleLayout->setContentsMargins(14, 12, 14, 14);
    exampleLayout->setSpacing(6);

    m_exampleHeaderLabel = new QLabel(tr("EXAMPLE USAGE"), m_exampleCard);
    m_exampleTextLabel = new QLabel(m_exampleCard);
    m_exampleTextLabel->setWordWrap(true);
    m_exampleTextLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont exampleFont = font();
    exampleFont.setPointSize(11);
    exampleFont.setItalic(true);
    m_exampleTextLabel->setFont(exampleFont);

    exampleLayout->addWidget(m_exampleHeaderLabel);
    exampleLayout->addWidget(m_exampleTextLabel);
    scrollLayout->addWidget(m_exampleCard);

    // Synonyms Card
    m_synonymsCard = new QFrame(contentContainer);
    auto* synLayout = new QVBoxLayout(m_synonymsCard);
    synLayout->setContentsMargins(14, 12, 14, 14);
    synLayout->setSpacing(8);

    m_synonymsHeaderLabel = new QLabel(tr("SYNONYMS"), m_synonymsCard);
    synLayout->addWidget(m_synonymsHeaderLabel);

    auto* synScroll = new QScrollArea(m_synonymsCard);
    synScroll->setWidgetResizable(true);
    synScroll->setFrameShape(QFrame::NoFrame);
    synScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    synScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    synScroll->setFixedHeight(36);
    synScroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));

    m_synonymsContainer = new QWidget(synScroll);
    m_synonymsLayout = new QHBoxLayout(m_synonymsContainer);
    m_synonymsLayout->setContentsMargins(0, 0, 0, 0);
    m_synonymsLayout->setSpacing(6);
    synScroll->setWidget(m_synonymsContainer);
    synLayout->addWidget(synScroll);

    scrollLayout->addWidget(m_synonymsCard);
    scrollLayout->addStretch(1);

    contentScroll->setWidget(contentContainer);
    mainLayout->addWidget(contentScroll, 1);

    // Bottom Divider
    m_bottomDivider = new QFrame(this);
    m_bottomDivider->setFrameShape(QFrame::HLine);
    m_bottomDivider->setFrameShadow(QFrame::Plain);
    m_bottomDivider->setFixedHeight(1);
    mainLayout->addWidget(m_bottomDivider);

    // 3. Footer Action Bar
    auto* footerLayout = new QHBoxLayout();
    footerLayout->setSpacing(8);

    m_footerStatusLabel = new QLabel(this);

    m_copyBtn = new QPushButton(tr("Copy Entry"), this);
    m_copyBtn->setObjectName(QStringLiteral("copyBtn"));
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_copyBtn, &QPushButton::clicked, this, &DictionaryDialog::onCopyClicked);

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setObjectName(QStringLiteral("closeBtn"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setDefault(true);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    footerLayout->addWidget(m_footerStatusLabel);
    footerLayout->addStretch(1);
    footerLayout->addWidget(m_copyBtn);
    footerLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(footerLayout);
}

void DictionaryDialog::updateContentUi(const DictEntry& entry) {
    const QString displayWord = entry.word.isEmpty() ? m_currentWord : entry.word;
    m_wordTitleLabel->setText(displayWord.isEmpty() ? tr("No Word Selected") : displayWord);

    if (!entry.phonetic.isEmpty()) {
        m_phoneticLabel->setText(entry.phonetic);
        m_phoneticLabel->show();
    } else {
        m_phoneticLabel->hide();
    }

    if (!entry.partOfSpeech.isEmpty()) {
        m_posBadge->setText(entry.partOfSpeech.toUpper());
        m_posBadge->show();
    } else {
        m_posBadge->hide();
    }

    m_pronounceBtn->setVisible(!displayWord.isEmpty() && entry.found);
    if (m_saveVocabBtn) {
        m_saveVocabBtn->setVisible(!displayWord.isEmpty() && entry.found);
        m_saveVocabBtn->setText(tr("⭐ Save to Notebook"));
        m_saveVocabBtn->setEnabled(true);
    }

    // Definition HTML
    QString html;
    if (entry.found) {
        html = QStringLiteral(
            "<div style=\"font-size: 14px; line-height: 1.6; color: %1;\">%2</div>"
        ).arg(m_tokens.textColor.name(), entry.definition.toHtmlEscaped());
    } else {
        html = QStringLiteral(
            "<div style=\"font-size: 13px; line-height: 1.5; color: %1;\">"
            "<em>%2</em><br><br>"
            "<span style=\"color: %3; font-size: 12px;\">%4</span>"
            "</div>"
        ).arg(
            m_tokens.headerFooterColor.name(),
            entry.definition.toHtmlEscaped(),
            m_tokens.headerFooterColor.name(),
            tr("Tip: Check word spelling or select another language from the dropdown.")
        );
    }
    m_defBrowser->setHtml(html);

    // Examples
    if (!entry.example.isEmpty()) {
        m_exampleTextLabel->setText(QStringLiteral("“%1”").arg(entry.example));
        m_exampleCard->show();
    } else {
        m_exampleCard->hide();
    }

    // Synonyms
    updateSynonymsUi(entry.synonyms);

    // Footer status
    if (entry.found) {
        m_footerStatusLabel->setText(tr("Found in offline vocabulary (%1)").arg(m_currentLanguage.toUpper()));
    } else if (!m_currentWord.isEmpty()) {
        m_footerStatusLabel->setText(tr("Not in offline vocabulary (%1)").arg(m_currentLanguage.toUpper()));
    } else {
        m_footerStatusLabel->setText(tr("Ready"));
    }
}

void DictionaryDialog::updateSynonymsUi(const QStringList& synonyms) {
    if (!m_synonymsLayout || !m_synonymsContainer) return;

    QLayoutItem* item = nullptr;
    while ((item = m_synonymsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (synonyms.isEmpty()) {
        m_synonymsCard->hide();
        return;
    }

    m_synonymsCard->show();
    for (const auto& syn : synonyms) {
        auto* synBtn = new QPushButton(syn, m_synonymsContainer);
        synBtn->setCursor(Qt::PointingHandCursor);
        synBtn->setToolTip(tr("Look up '%1'").arg(syn));
        synBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 12px;"
            "  padding: 4px 10px;"
            "  font-size: 11px;"
            "  font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "  background-color: %4;"
            "  color: white;"
            "  border-color: %4;"
            "}"
        ).arg(
            m_tokens.controlBgColor.name(),
            m_tokens.textColor.name(),
            m_tokens.dividerColor.name(),
            m_tokens.accentColor.name()
        ));

        connect(synBtn, &QPushButton::clicked, this, [this, syn]() {
            onSynonymClicked(syn);
        });
        m_synonymsLayout->addWidget(synBtn);
    }
    m_synonymsLayout->addStretch(1);
}

void DictionaryDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    updateThemeStyles();
}

void DictionaryDialog::updateThemeStyles() {
    const QString qss = QStringLiteral(
        "QDialog {"
        "  background-color: %1;"
        "  color: %2;"
        "}"
        "QLineEdit {"
        "  background-color: %3;"
        "  color: %2;"
        "  border: 1px solid %4;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid %5;"
        "}"
        "QComboBox {"
        "  background-color: %3;"
        "  color: %2;"
        "  border: 1px solid %4;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "  font-size: 12px;"
        "  min-width: 105px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: %3;"
        "  color: %2;"
        "  border: 1px solid %4;"
        "  selection-background-color: %5;"
        "  selection-color: white;"
        "}"
        "QPushButton#searchBtn {"
        "  background-color: %5;"
        "  color: white;"
        "  font-weight: 600;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton#searchBtn:hover {"
        "  background-color: %6;"
        "}"
        "QPushButton#pronounceBtn {"
        "  background-color: transparent;"
        "  color: %5;"
        "  border: 1px solid %4;"
        "  border-radius: 6px;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#pronounceBtn:hover {"
        "  background-color: %3;"
        "}"
        "QPushButton#copyBtn {"
        "  background-color: transparent;"
        "  color: %2;"
        "  border: 1px solid %4;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
        "  font-size: 12px;"
        "}"
        "QPushButton#copyBtn:hover {"
        "  background-color: %3;"
        "}"
        "QPushButton#closeBtn {"
        "  background-color: %5;"
        "  color: white;"
        "  font-weight: 600;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 18px;"
        "  font-size: 12px;"
        "}"
        "QPushButton#closeBtn:hover {"
        "  background-color: %6;"
        "}"
        "QScrollArea {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 7px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %4;"
        "  min-height: 20px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: %7;"
        "}"
    ).arg(
        m_tokens.backgroundColor.name(),
        m_tokens.textColor.name(),
        m_tokens.controlBgColor.name(),
        m_tokens.dividerColor.name(),
        m_tokens.accentColor.name(),
        m_tokens.accentColor.lighter(115).name(),
        m_tokens.headerFooterColor.name()
    );
    setStyleSheet(qss);

    // Divider lines
    const QString dividerStyle = QStringLiteral("background-color: %1; border: none;").arg(m_tokens.dividerColor.name());
    if (m_topDivider) m_topDivider->setStyleSheet(dividerStyle);
    if (m_bottomDivider) m_bottomDivider->setStyleSheet(dividerStyle);

    // Card frame styles
    const QString cardStyle = QStringLiteral(
        "QFrame {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
    ).arg(m_tokens.controlBgColor.name(), m_tokens.dividerColor.name());

    m_definitionCard->setStyleSheet(cardStyle);
    m_exampleCard->setStyleSheet(cardStyle);
    m_synonymsCard->setStyleSheet(cardStyle);

    // Section header labels
    const QString headerStyle = QStringLiteral(
        "QLabel {"
        "  color: %1;"
        "  font-size: 9px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.5px;"
        "  background: transparent;"
        "}"
    ).arg(m_tokens.headerFooterColor.name());

    m_defHeaderLabel->setStyleSheet(headerStyle);
    m_exampleHeaderLabel->setStyleSheet(headerStyle);
    m_synonymsHeaderLabel->setStyleSheet(headerStyle);

    // Word title label
    m_wordTitleLabel->setStyleSheet(QStringLiteral(
        "color: %1; background: transparent;"
    ).arg(m_tokens.textColor.name()));

    // Phonetic label
    m_phoneticLabel->setStyleSheet(QStringLiteral(
        "color: %1; background: transparent;"
    ).arg(m_tokens.headerFooterColor.name()));

    // POS badge style
    m_posBadge->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "  border-radius: 8px;"
        "  padding: 2px 7px;"
        "}"
    ).arg(m_tokens.selectionColor.name(), m_tokens.accentColor.name()));

    // Status badge style (green dot / offline lexicon indicator)
    m_statusBadge->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: rgba(16, 185, 129, 0.12);"
        "  color: #10B981;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  border-radius: 10px;"
        "  padding: 3px 8px;"
        "}"
    ));

    // Example quote text style
    m_exampleTextLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: %1;"
        "  font-size: 13px;"
        "  font-style: italic;"
        "  background: transparent;"
        "}"
    ).arg(m_tokens.textColor.name()));

    // Footer status text
    m_footerStatusLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: %1;"
        "  font-size: 11px;"
        "  background: transparent;"
        "}"
    ).arg(m_tokens.headerFooterColor.name()));

    // Refresh content with current tokens
    updateContentUi(m_currentEntry);
}

} // namespace LunaRead
