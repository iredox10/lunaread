#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QVector>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

struct SearchMatchResult {
    int chapterIndex = 0;
    QString chapterTitle;
    int charOffset = 0;
    int matchLength = 0;
    QString snippet;
};

class SearchInBookDialog : public QDialog {
    Q_OBJECT

public:
    explicit SearchInBookDialog(const BookDocument& document, QWidget* parent = nullptr);
    ~SearchInBookDialog() override = default;

    void applyTheme(const ReaderThemeTokens& tokens);
    const QVector<SearchMatchResult>& matches() const { return m_matches; }

signals:
    void matchSelected(int chapterIndex, int charOffset, int matchLength);

private slots:
    void performSearch();
    void onNextMatch();
    void onPrevMatch();
    void onResultItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUi();
    QString createSnippet(const QString& fullText, int matchPos, int matchLen) const;

    BookDocument m_document;
    QVector<SearchMatchResult> m_matches;
    int m_currentMatchIndex = -1;

    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QPushButton* m_prevBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QCheckBox* m_caseSensitiveCheck = nullptr;
    QCheckBox* m_wholeWordCheck = nullptr;
    QLabel* m_statusLabel = nullptr;
    QListWidget* m_resultsList = nullptr;
};

} // namespace LunaRead
