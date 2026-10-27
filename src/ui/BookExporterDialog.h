#pragma once

#include <QDialog>
#include <QString>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QRadioButton;
class QCheckBox;
class QSpinBox;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QButtonGroup;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Dialog allowing the user to configure and execute multi-format book conversion/export.
 */
class BookExporterDialog : public QDialog {
    Q_OBJECT

public:
    explicit BookExporterDialog(const BookDocument& doc, QWidget* parent = nullptr);
    explicit BookExporterDialog(const BookDocument& doc, int currentChapterIndex, QWidget* parent = nullptr);
    ~BookExporterDialog() override = default;

    void applyTheme(const ReaderThemeTokens& tokens);

signals:
    void exportFinished(bool success, const QString& path);

private slots:
    void onFormatChanged(int index);
    void onChapterScopeChanged();
    void onStartChapterChanged(int val);
    void onEndChapterChanged(int val);
    void onBrowseClicked();
    void onExportClicked();
    void onOpenFolderClicked();

private:
    void setupUi();
    void updateDefaultOutputPath();
    QString currentFormatExtension() const;
    QString currentFormatFilter() const;

    BookDocument m_doc;
    int m_currentChapterIndex = 0;
    QString m_lastExportedPath;

    // Controls
    QComboBox* m_formatCombo = nullptr;

    QButtonGroup* m_scopeGroup = nullptr;
    QRadioButton* m_allChaptersRadio = nullptr;
    QRadioButton* m_currentChapterRadio = nullptr;
    QRadioButton* m_customRangeRadio = nullptr;
    QSpinBox* m_startChapterSpin = nullptr;
    QSpinBox* m_endChapterSpin = nullptr;
    QLabel* m_rangeToLabel = nullptr;

    QCheckBox* m_includeHeadingsCheck = nullptr;
    QCheckBox* m_includeMetadataTocCheck = nullptr;

    QLineEdit* m_outputPathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;

    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_openFolderBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
};

} // namespace LunaRead
