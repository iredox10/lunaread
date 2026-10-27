#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QTextEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

namespace LunaRead {

class AnnotationExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit AnnotationExportDialog(const QString& bookTitle, const QList<AnnotationRecord>& annotations, QWidget* parent = nullptr);
    ~AnnotationExportDialog() override = default;

    void applyTheme(const ReaderThemeTokens& tokens);

private slots:
    void updatePreview();
    void onCopyToClipboard();
    void onSaveToFile();

private:
    void setupUi();
    QList<AnnotationRecord> filteredAnnotations() const;

    QString m_bookTitle;
    QList<AnnotationRecord> m_annotations;

    QComboBox* m_formatCombo = nullptr;
    QCheckBox* m_includeHighlightsCheck = nullptr;
    QCheckBox* m_includeNotesCheck = nullptr;
    QCheckBox* m_includeBookmarksCheck = nullptr;
    QTextEdit* m_previewEdit = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // namespace LunaRead
