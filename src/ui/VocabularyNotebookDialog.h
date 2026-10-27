#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class VocabularyNotebookDialog : public QDialog {
    Q_OBJECT
public:
    explicit VocabularyNotebookDialog(QWidget* parent = nullptr);
    ~VocabularyNotebookDialog() override = default;

    void applyTheme(const ReaderThemeTokens& tokens);

signals:
    void pronounceRequested(const QString& word);

private slots:
    void onSearchChanged(const QString& text);
    void onDeleteWord(const QString& id);
    void onClearAll();
    void onExport();
    void onToggleQuizMode();

private:
    void setupUi();
    void reloadWords();
    QWidget* createWordRowWidget(const VocabularyWord& word, bool isQuiz);

    QList<VocabularyWord> m_words;
    ReaderThemeTokens m_tokens;
    bool m_quizMode = false;

    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_countLabel = nullptr;
    QListWidget* m_listWidget = nullptr;
    QPushButton* m_quizBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
};

} // namespace LunaRead
