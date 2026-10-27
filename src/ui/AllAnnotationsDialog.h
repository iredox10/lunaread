#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class AllAnnotationsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AllAnnotationsDialog(const QString& bookId, const QString& bookTitle, QWidget* parent = nullptr);
    ~AllAnnotationsDialog() override = default;

    void applyTheme(const ReaderThemeTokens& tokens);

signals:
    void jumpToAnnotationRequested(int chapterIndex, int pageIndex);
    void annotationChanged();

private slots:
    void onFilterChanged();
    void onItemDoubleClicked(QListWidgetItem* item);
    void onJumpClicked(const AnnotationRecord& record);
    void onEditNoteClicked(const AnnotationRecord& record);
    void onDeleteClicked(const AnnotationRecord& record);
    void onExportClicked();
    void onDeleteAllClicked();

private:
    void setupUi();
    void reloadAnnotations();
    QWidget* createAnnotationRowWidget(const AnnotationRecord& record);

    QString m_bookId;
    QString m_bookTitle;
    ReaderThemeTokens m_tokens;
    QList<AnnotationRecord> m_allAnnotations;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_typeFilterCombo = nullptr;
    QComboBox* m_colorFilterCombo = nullptr;
    QListWidget* m_listWidget = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_deleteAllBtn = nullptr;
};

} // namespace LunaRead
