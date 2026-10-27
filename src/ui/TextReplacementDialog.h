#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "../features/TextFilterManager.h"

namespace LunaRead {

/**
 * @brief Dialog to view, add, edit, and test custom text replacement rules and content filters.
 */
class TextReplacementDialog : public QDialog {
    Q_OBJECT

public:
    explicit TextReplacementDialog(const QString& currentBookId = QString(), QWidget* parent = nullptr);
    ~TextReplacementDialog() override = default;

private slots:
    void refreshTable();
    void onAddRule();
    void onDeleteRule();
    void onToggleRule(int row);
    void onTestLive();

private:
    void setupUi();

    QString m_bookId;
    QTableWidget* m_table = nullptr;

    QLineEdit* m_patternEdit = nullptr;
    QLineEdit* m_replacementEdit = nullptr;
    QCheckBox* m_regexCheck = nullptr;
    QCheckBox* m_caseCheck = nullptr;
    QCheckBox* m_globalScopeCheck = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;

    QTextEdit* m_testInput = nullptr;
    QTextEdit* m_testOutput = nullptr;
    QPushButton* m_testBtn = nullptr;
};

} // namespace LunaRead
