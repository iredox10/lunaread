#pragma once

#include <QDialog>
#include <QString>
#include <QColor>
#include <QList>
#include "../core/Types.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;

namespace LunaRead {

class QuickNoteDialog : public QDialog {
    Q_OBJECT

public:
    explicit QuickNoteDialog(const QString& selectedText = QString(), QWidget* parent = nullptr);
    ~QuickNoteDialog() override = default;

    void setAnnotation(const AnnotationRecord& ann);
    AnnotationRecord getAnnotation() const;

    [[nodiscard]] QString getNoteText() const;
    [[nodiscard]] QColor getSelectedColor() const;
    [[nodiscard]] QString getTags() const;

signals:
    void noteSaved(const QString& noteText, const QColor& color, const QString& tags);
    void noteDeleted(const QString& annotationId);

private slots:
    void onColorChipClicked(int id);
    void onSaveClicked();
    void onDeleteClicked();

private:
    void setupUi();
    void applyTheme();

    QString m_selectedText;
    AnnotationRecord m_currentAnnotation;
    QColor m_selectedColor = QColor("#FFE082"); // Default Moon+ yellow

    QLabel* m_quoteLabel = nullptr;
    QList<QPushButton*> m_colorChips;
    QTextEdit* m_noteEdit = nullptr;
    QLineEdit* m_tagEdit = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
};

} // namespace LunaRead
