#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include "../features/DictionaryService.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;
class QComboBox;
class QFrame;
class QHBoxLayout;
class QVBoxLayout;
class QScrollArea;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Clean, modern Qt6 dialog for looking up word definitions,
 * pronunciations, examples, and synonyms within LunaRead reader.
 */
class DictionaryDialog : public QDialog {
    Q_OBJECT

public:
    // Constructors supporting various calling conventions
    explicit DictionaryDialog(const QString& word,
                              DictionaryService& service,
                              QWidget* parent = nullptr);

    explicit DictionaryDialog(const QString& word,
                              const QString& language,
                              DictionaryService& service,
                              QWidget* parent = nullptr);

    explicit DictionaryDialog(const QString& word,
                              DictionaryService* service,
                              QWidget* parent = nullptr);

    explicit DictionaryDialog(const QString& word,
                              const QString& language,
                              DictionaryService* service,
                              QWidget* parent = nullptr);

    explicit DictionaryDialog(QWidget* parent = nullptr);

    ~DictionaryDialog() override = default;

    // Configuration / programmatic access
    void setDictionaryService(DictionaryService* service);
    void setWord(const QString& word);
    void setLanguage(const QString& language);
    void applyTheme(const ReaderThemeTokens& tokens);

    [[nodiscard]] QString word() const { return m_currentWord; }
    [[nodiscard]] QString language() const { return m_currentLanguage; }
    [[nodiscard]] DictEntry currentEntry() const { return m_currentEntry; }

public slots:
    void lookup(const QString& word, const QString& language = QString());
    void refreshLookup();

signals:
    void wordLookedUp(const QString& word, bool found);
    void pronounceRequested(const QString& word);

private slots:
    void onSearchTriggered();
    void onLanguageChanged(int index);
    void onSynonymClicked(const QString& synonym);
    void onCopyClicked();

private:
    void setupUi();
    void updateContentUi(const DictEntry& entry);
    void populateLanguageCombo();
    void updateSynonymsUi(const QStringList& synonyms);
    void updateThemeStyles();

    DictionaryService* m_service = nullptr;
    QString m_currentWord;
    QString m_currentLanguage = QStringLiteral("en");
    DictEntry m_currentEntry;
    ReaderThemeTokens m_tokens = ReaderThemeTokens::modernLight();

    // UI Widgets
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QComboBox* m_langCombo = nullptr;
    QLabel* m_statusBadge = nullptr;

    QFrame* m_topDivider = nullptr;
    QFrame* m_bottomDivider = nullptr;

    QLabel* m_wordTitleLabel = nullptr;
    QLabel* m_phoneticLabel = nullptr;
    QLabel* m_posBadge = nullptr;
    QPushButton* m_pronounceBtn = nullptr;

    QFrame* m_definitionCard = nullptr;
    QLabel* m_defHeaderLabel = nullptr;
    QTextBrowser* m_defBrowser = nullptr;

    QFrame* m_exampleCard = nullptr;
    QLabel* m_exampleHeaderLabel = nullptr;
    QLabel* m_exampleTextLabel = nullptr;

    QFrame* m_synonymsCard = nullptr;
    QLabel* m_synonymsHeaderLabel = nullptr;
    QWidget* m_synonymsContainer = nullptr;
    QHBoxLayout* m_synonymsLayout = nullptr;

    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QLabel* m_footerStatusLabel = nullptr;
};

} // namespace LunaRead
