#include "BookExporterDialog.h"
#include "../features/BookExporter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QRegularExpression>
#include <algorithm>

namespace LunaRead {

BookExporterDialog::BookExporterDialog(const BookDocument& doc, QWidget* parent)
    : BookExporterDialog(doc, 0, parent)
{
}

BookExporterDialog::BookExporterDialog(const BookDocument& doc, int currentChapterIndex, QWidget* parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_currentChapterIndex(currentChapterIndex)
{
    if (m_currentChapterIndex < 0) {
        m_currentChapterIndex = 0;
    }
    if (!m_doc.chapters.isEmpty() && m_currentChapterIndex >= m_doc.chapters.size()) {
        m_currentChapterIndex = static_cast<int>(m_doc.chapters.size()) - 1;
    }

    setupUi();
    updateDefaultOutputPath();
}

void BookExporterDialog::setupUi() {
    const QString displayTitle = m_doc.title.trimmed().isEmpty() ? tr("Untitled Book") : m_doc.title.trimmed();
    setWindowTitle(tr("Export Book - %1").arg(displayTitle));
    resize(620, 520);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // Book Information Header
    auto* headerLayout = new QVBoxLayout();
    auto* headerTitle = new QLabel(QStringLiteral("<h3>📖 %1</h3>").arg(displayTitle.toHtmlEscaped()), this);
    headerLayout->addWidget(headerTitle);

    const QString author = m_doc.author.trimmed().isEmpty() ? tr("Unknown Author") : m_doc.author.trimmed();
    const int chCount = static_cast<int>(m_doc.chapters.size());
    auto* headerSub = new QLabel(tr("Author: <b>%1</b> &nbsp;|&nbsp; Total Chapters: <b>%2</b>")
                                 .arg(author.toHtmlEscaped())
                                 .arg(chCount), this);
    headerSub->setStyleSheet(QStringLiteral("color: #4B5563; font-size: 11pt;"));
    headerLayout->addWidget(headerSub);
    mainLayout->addLayout(headerLayout);

    // Format Selector
    auto* formatLayout = new QHBoxLayout();
    auto* formatLabel = new QLabel(tr("<b>Export Format:</b>"), this);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("Plain Text (.txt)"), QStringLiteral("txt"));
    m_formatCombo->addItem(tr("Markdown (.md)"), QStringLiteral("md"));
    m_formatCombo->addItem(tr("Clean HTML (.html)"), QStringLiteral("html"));
    m_formatCombo->setStyleSheet(QStringLiteral("QComboBox { padding: 6px 12px; border: 1px solid #D1D5DB; border-radius: 6px; }"));
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(m_formatCombo, 1);
    mainLayout->addLayout(formatLayout);

    // Chapter Selection Group
    auto* scopeGroup = new QGroupBox(tr("Chapter Selection"), this);
    auto* scopeLayout = new QVBoxLayout(scopeGroup);
    scopeLayout->setSpacing(8);

    m_scopeGroup = new QButtonGroup(this);

    m_allChaptersRadio = new QRadioButton(tr("All chapters (1 to %1)").arg(std::max(1, chCount)), scopeGroup);
    m_allChaptersRadio->setChecked(true);
    m_scopeGroup->addButton(m_allChaptersRadio, 0);
    scopeLayout->addWidget(m_allChaptersRadio);

    QString currTitle = tr("Chapter %1").arg(m_currentChapterIndex + 1);
    if (m_currentChapterIndex >= 0 && m_currentChapterIndex < m_doc.chapters.size() && !m_doc.chapters[m_currentChapterIndex].title.trimmed().isEmpty()) {
        currTitle = m_doc.chapters[m_currentChapterIndex].title.trimmed();
    }
    m_currentChapterRadio = new QRadioButton(tr("Current chapter (%1)").arg(currTitle), scopeGroup);
    m_currentChapterRadio->setEnabled(chCount > 0);
    m_scopeGroup->addButton(m_currentChapterRadio, 1);
    scopeLayout->addWidget(m_currentChapterRadio);

    auto* customRangeLayout = new QHBoxLayout();
    m_customRangeRadio = new QRadioButton(tr("Custom range:"), scopeGroup);
    m_customRangeRadio->setEnabled(chCount > 0);
    m_scopeGroup->addButton(m_customRangeRadio, 2);
    customRangeLayout->addWidget(m_customRangeRadio);

    m_startChapterSpin = new QSpinBox(scopeGroup);
    m_startChapterSpin->setRange(1, std::max(1, chCount));
    m_startChapterSpin->setValue(1);
    m_startChapterSpin->setEnabled(false);
    customRangeLayout->addWidget(m_startChapterSpin);

    m_rangeToLabel = new QLabel(tr("to"), scopeGroup);
    m_rangeToLabel->setEnabled(false);
    customRangeLayout->addWidget(m_rangeToLabel);

    m_endChapterSpin = new QSpinBox(scopeGroup);
    m_endChapterSpin->setRange(1, std::max(1, chCount));
    m_endChapterSpin->setValue(std::max(1, chCount));
    m_endChapterSpin->setEnabled(false);
    customRangeLayout->addWidget(m_endChapterSpin);

    customRangeLayout->addStretch(1);
    scopeLayout->addLayout(customRangeLayout);

    mainLayout->addWidget(scopeGroup);

    // Options Group
    auto* optionsGroup = new QGroupBox(tr("Options"), this);
    auto* optionsLayout = new QVBoxLayout(optionsGroup);
    optionsLayout->setSpacing(6);

    m_includeHeadingsCheck = new QCheckBox(tr("Include Chapter Headings"), optionsGroup);
    m_includeHeadingsCheck->setChecked(true);
    optionsLayout->addWidget(m_includeHeadingsCheck);

    m_includeMetadataTocCheck = new QCheckBox(tr("Include Book Metadata & Table of Contents"), optionsGroup);
    m_includeMetadataTocCheck->setChecked(true);
    optionsLayout->addWidget(m_includeMetadataTocCheck);

    mainLayout->addWidget(optionsGroup);

    // Output Destination Group
    auto* outputGroup = new QGroupBox(tr("Output File"), this);
    auto* outputLayout = new QHBoxLayout(outputGroup);

    m_outputPathEdit = new QLineEdit(outputGroup);
    m_outputPathEdit->setPlaceholderText(tr("Choose output destination path..."));
    m_outputPathEdit->setStyleSheet(QStringLiteral("QLineEdit { padding: 6px 10px; border: 1px solid #D1D5DB; border-radius: 6px; }"));
    outputLayout->addWidget(m_outputPathEdit, 1);

    m_browseBtn = new QPushButton(tr("Browse..."), outputGroup);
    m_browseBtn->setCursor(Qt::PointingHandCursor);
    m_browseBtn->setStyleSheet(QStringLiteral("QPushButton { padding: 6px 14px; border: 1px solid #D1D5DB; border-radius: 6px; background-color: #F3F4F6; } QPushButton:hover { background-color: #E5E7EB; }"));
    outputLayout->addWidget(m_browseBtn);

    mainLayout->addWidget(outputGroup);

    // Progress Bar & Status
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(QStringLiteral("QProgressBar { border: 1px solid #D1D5DB; border-radius: 6px; text-align: center; height: 18px; } QProgressBar::chunk { background-color: #3B82F6; border-radius: 5px; }"));
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(tr("Ready to export."), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #4B5563; font-weight: 500;"));
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch(1);

    // Bottom Action Row
    auto* bottomLayout = new QHBoxLayout();
    m_openFolderBtn = new QPushButton(tr("📂 Open Containing Folder"), this);
    m_openFolderBtn->setCursor(Qt::PointingHandCursor);
    m_openFolderBtn->setStyleSheet(QStringLiteral("QPushButton { padding: 8px 16px; border-radius: 6px; background-color: #E5E7EB; border: 1px solid #D1D5DB; font-weight: 500; } QPushButton:hover { background-color: #D1D5DB; }"));
    m_openFolderBtn->hide();
    bottomLayout->addWidget(m_openFolderBtn);

    bottomLayout->addStretch(1);

    m_exportBtn = new QPushButton(tr("🚀 Export Book"), this);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setStyleSheet(QStringLiteral("QPushButton { padding: 8px 20px; border-radius: 6px; background-color: #2563EB; color: white; border: none; font-weight: bold; } QPushButton:hover { background-color: #1D4ED8; }"));
    bottomLayout->addWidget(m_exportBtn);

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(QStringLiteral("QPushButton { padding: 8px 16px; border-radius: 6px; background-color: transparent; border: 1px solid #D1D5DB; } QPushButton:hover { background-color: #F3F4F6; }"));
    bottomLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(bottomLayout);

    // Connect signals
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookExporterDialog::onFormatChanged);
    connect(m_allChaptersRadio, &QRadioButton::toggled, this, &BookExporterDialog::onChapterScopeChanged);
    connect(m_currentChapterRadio, &QRadioButton::toggled, this, &BookExporterDialog::onChapterScopeChanged);
    connect(m_customRangeRadio, &QRadioButton::toggled, this, &BookExporterDialog::onChapterScopeChanged);
    connect(m_startChapterSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BookExporterDialog::onStartChapterChanged);
    connect(m_endChapterSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BookExporterDialog::onEndChapterChanged);
    connect(m_browseBtn, &QPushButton::clicked, this, &BookExporterDialog::onBrowseClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &BookExporterDialog::onExportClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &BookExporterDialog::onOpenFolderClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString BookExporterDialog::currentFormatExtension() const {
    if (!m_formatCombo) return QStringLiteral("txt");
    return m_formatCombo->currentData().toString();
}

QString BookExporterDialog::currentFormatFilter() const {
    const QString fmt = currentFormatExtension();
    if (fmt == QLatin1String("md")) {
        return tr("Markdown Documents (*.md);;All Files (*)");
    } else if (fmt == QLatin1String("html")) {
        return tr("HTML Documents (*.html *.htm);;All Files (*)");
    } else {
        return tr("Plain Text Documents (*.txt);;All Files (*)");
    }
}

void BookExporterDialog::updateDefaultOutputPath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty() || !QDir(dir).exists()) {
        dir = QDir::homePath();
    }

    QString safeTitle = m_doc.title.trimmed();
    if (safeTitle.isEmpty()) {
        safeTitle = QStringLiteral("exported_book");
    }
    static const QRegularExpression invalidChars(QStringLiteral(R"([\\/:*?"<>|])"));
    safeTitle.replace(invalidChars, QStringLiteral("_"));

    const QString ext = currentFormatExtension();
    const QString fileName = QStringLiteral("%1.%2").arg(safeTitle, ext);
    const QString fullPath = QDir(dir).filePath(fileName);

    m_outputPathEdit->setText(fullPath);
}

void BookExporterDialog::onFormatChanged(int /*index*/) {
    const QString curPath = m_outputPathEdit->text().trimmed();
    if (!curPath.isEmpty()) {
        QFileInfo fi(curPath);
        const QString newPath = fi.dir().filePath(QStringLiteral("%1.%2").arg(fi.completeBaseName(), currentFormatExtension()));
        m_outputPathEdit->setText(newPath);
    }
}

void BookExporterDialog::onChapterScopeChanged() {
    const bool custom = m_customRangeRadio->isChecked();
    m_startChapterSpin->setEnabled(custom);
    m_rangeToLabel->setEnabled(custom);
    m_endChapterSpin->setEnabled(custom);
}

void BookExporterDialog::onStartChapterChanged(int val) {
    if (val > m_endChapterSpin->value()) {
        m_endChapterSpin->setValue(val);
    }
}

void BookExporterDialog::onEndChapterChanged(int val) {
    if (val < m_startChapterSpin->value()) {
        m_startChapterSpin->setValue(val);
    }
}

void BookExporterDialog::onBrowseClicked() {
    const QString curPath = m_outputPathEdit->text().trimmed();
    const QString selected = QFileDialog::getSaveFileName(this, tr("Select Output File"), curPath, currentFormatFilter());
    if (!selected.isEmpty()) {
        m_outputPathEdit->setText(selected);
    }
}

void BookExporterDialog::onExportClicked() {
    const QString path = m_outputPathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Destination"), tr("Please provide a valid output file path."));
        return;
    }

    const int totalChapters = static_cast<int>(m_doc.chapters.size());
    int start = 0;
    int end = totalChapters - 1;

    if (m_currentChapterRadio->isChecked()) {
        start = m_currentChapterIndex;
        end = m_currentChapterIndex;
    } else if (m_customRangeRadio->isChecked()) {
        start = m_startChapterSpin->value() - 1;
        end = m_endChapterSpin->value() - 1;
    }

    BookExportOptions options;
    options.startChapter = start;
    options.endChapter = end;
    options.includeChapterTitles = m_includeHeadingsCheck->isChecked();
    options.includeMetadataAndToc = m_includeMetadataTocCheck->isChecked();
    options.progressCallback = [this](int current, int total) {
        const int pct = (total > 0) ? (current * 100 / total) : 100;
        m_progressBar->setValue(pct);
        m_statusLabel->setText(tr("Exporting chapter %1 of %2...").arg(current).arg(total));
        QCoreApplication::processEvents();
    };

    m_exportBtn->setEnabled(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Exporting..."));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #2563EB; font-weight: 500;"));
    QCoreApplication::processEvents();

    const QString fmt = currentFormatExtension();
    const bool success = BookExporter::exportBook(m_doc, path, fmt, options);

    m_exportBtn->setEnabled(true);

    if (success) {
        m_lastExportedPath = path;
        m_progressBar->setValue(100);
        m_statusLabel->setText(tr("✓ Successfully exported: %1").arg(QFileInfo(path).fileName()));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #059669; font-weight: bold;"));
        m_openFolderBtn->show();
        emit exportFinished(true, path);
    } else {
        m_progressBar->setValue(0);
        m_statusLabel->setText(tr("✗ Export failed. Could not write to destination."));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #DC2626; font-weight: bold;"));
        QMessageBox::critical(this, tr("Export Error"), tr("Failed to write to file:\n%1\nPlease verify permissions and path.").arg(path));
        emit exportFinished(false, path);
    }
}

void BookExporterDialog::onOpenFolderClicked() {
    if (m_lastExportedPath.isEmpty()) return;
    const QFileInfo fi(m_lastExportedPath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

void BookExporterDialog::applyTheme(const ReaderThemeTokens& tokens) {
    if (tokens.isDark) {
        setStyleSheet(QStringLiteral(
            "QDialog { background-color: %1; color: %2; } "
            "QGroupBox { color: %2; font-weight: bold; border: 1px solid %3; border-radius: 6px; margin-top: 10px; padding-top: 10px; } "
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; } "
            "QLineEdit, QComboBox, QSpinBox { background-color: %4; color: %2; border: 1px solid %3; border-radius: 4px; padding: 4px; } "
            "QLabel { color: %2; }"
        ).arg(tokens.backgroundColor.name(),
             tokens.textColor.name(),
             tokens.dividerColor.name(),
             tokens.controlBgColor.name()));
    }
}

} // namespace LunaRead
