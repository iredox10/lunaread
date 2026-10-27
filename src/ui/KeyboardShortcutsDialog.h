#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>

namespace LunaRead {

/**
 * @brief Keyboard shortcuts reference and cheat sheet modal for LunaRead-Qt desktop.
 */
class KeyboardShortcutsDialog : public QDialog {
    Q_OBJECT

public:
    explicit KeyboardShortcutsDialog(QWidget* parent = nullptr);
    ~KeyboardShortcutsDialog() override = default;

private slots:
    void filterShortcuts(const QString& query);

private:
    void setupUi();
    void addShortcutRow(const QString& category, const QString& action, const QString& shortcut, const QString& description);

    QLineEdit* m_filterEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_closeBtn = nullptr;
};

} // namespace LunaRead
