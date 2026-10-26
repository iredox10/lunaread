#include "QuickNoteDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>

namespace LunaRead {

static const QStringList s_moonPlusColors = {
    "#FFE082", // Yellow
    "#A5D6A7", // Green
    "#80DEEA", // Cyan
    "#F48FB1", // Pink
    "#CE93D8"  // Purple
};

QuickNoteDialog::QuickNoteDialog(const QString& selectedText, QWidget* parent)
    : QDialog(parent)
    , m_selectedText(selectedText)
{
    setWindowTitle(tr("Add Note & Highlight"));
    resize(460, 380);
    setupUi();
    applyTheme();
}

void QuickNoteDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    // Selected text quote block
    if (!m_selectedText.isEmpty()) {
        auto* quoteFrame = new QFrame(this);
        quoteFrame->setFrameShape(QFrame::StyledPanel);
        quoteFrame->setStyleSheet("background-color: #F3F4F6; border-left: 4px solid #3B82F6; border-radius: 4px; padding: 8px;");
        auto* quoteLayout = new QVBoxLayout(quoteFrame);
        quoteLayout->setContentsMargins(8, 4, 8, 4);

        m_quoteLabel = new QLabel(quoteFrame);
        m_quoteLabel->setWordWrap(true);
        QString display = m_selectedText.length() > 200 ? m_selectedText.left(197) + "..." : m_selectedText;
        m_quoteLabel->setText(QString("<i>\"%1\"</i>").arg(display.toHtmlEscaped()));
        m_quoteLabel->setStyleSheet("color: #4B5563; font-size: 11pt;");
        quoteLayout->addWidget(m_quoteLabel);
        layout->addWidget(quoteFrame);
    }

    // Color selector row
    auto* colorRow = new QHBoxLayout();
    colorRow->setSpacing(8);
    auto* colorLabel = new QLabel(tr("Highlight Color:"), this);
    colorLabel->setStyleSheet("font-weight: 600; color: #374151;");
    colorRow->addWidget(colorLabel);

    for (int i = 0; i < s_moonPlusColors.size(); ++i) {
        auto* chip = new QPushButton(this);
        chip->setFixedSize(26, 26);
        chip->setCheckable(true);
        chip->setStyleSheet(QString(
            "QPushButton { background-color: %1; border: 2px solid #D1D5DB; border-radius: 13px; }"
            "QPushButton:checked { border: 3px solid #1F2937; }"
        ).arg(s_moonPlusColors[i]));

        if (i == 0) chip->setChecked(true);
        connect(chip, &QPushButton::clicked, this, [this, i]() { onColorChipClicked(i); });
        m_colorChips.append(chip);
        colorRow->addWidget(chip);
    }
    colorRow->addStretch(1);
    layout->addLayout(colorRow);

    // Note text editor
    auto* noteLabel = new QLabel(tr("Your Note / Thoughts:"), this);
    noteLabel->setStyleSheet("font-weight: 600; color: #374151;");
    layout->addWidget(noteLabel);

    m_noteEdit = new QTextEdit(this);
    m_noteEdit->setPlaceholderText(tr("Write your insights, commentary, or summary here..."));
    m_noteEdit->setStyleSheet("border: 1px solid #D1D5DB; border-radius: 6px; padding: 8px; font-size: 11pt;");
    layout->addWidget(m_noteEdit, 1);

    // Tags line
    auto* tagRow = new QHBoxLayout();
    auto* tagLabel = new QLabel(tr("Tags:"), this);
    tagLabel->setStyleSheet("font-weight: 600; color: #374151;");
    tagRow->addWidget(tagLabel);

    m_tagEdit = new QLineEdit(this);
    m_tagEdit->setPlaceholderText(tr("#quote, #vocabulary, #ideas (comma separated)"));
    m_tagEdit->setStyleSheet("border: 1px solid #D1D5DB; border-radius: 6px; padding: 6px;");
    tagRow->addWidget(m_tagEdit, 1);
    layout->addLayout(tagRow);

    // Buttons
    auto* btnRow = new QHBoxLayout();
    m_deleteBtn = new QPushButton(tr("🗑️ Delete"), this);
    m_deleteBtn->setStyleSheet("color: #DC2626; border: 1px solid #FCA5A5; border-radius: 6px; padding: 6px 12px; font-weight: 500;");
    m_deleteBtn->hide(); // only shown when editing existing note
    connect(m_deleteBtn, &QPushButton::clicked, this, &QuickNoteDialog::onDeleteClicked);
    btnRow->addWidget(m_deleteBtn);

    btnRow->addStretch(1);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setStyleSheet("padding: 6px 14px; border: 1px solid #D1D5DB; border-radius: 6px; font-weight: 500;");
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_cancelBtn);

    m_saveBtn = new QPushButton(tr("💾 Save Note"), this);
    m_saveBtn->setStyleSheet("background-color: #3B82F6; color: white; border: none; border-radius: 6px; padding: 6px 16px; font-weight: 600;");
    connect(m_saveBtn, &QPushButton::clicked, this, &QuickNoteDialog::onSaveClicked);
    btnRow->addWidget(m_saveBtn);

    layout->addLayout(btnRow);
}

void QuickNoteDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background-color: #FFFFFF; }
    )");
}

void QuickNoteDialog::onColorChipClicked(int id) {
    for (int i = 0; i < m_colorChips.size(); ++i) {
        m_colorChips[i]->setChecked(i == id);
    }
    m_selectedColor = QColor(s_moonPlusColors[id]);
}

void QuickNoteDialog::onSaveClicked() {
    emit noteSaved(getNoteText(), getSelectedColor(), getTags());
    accept();
}

void QuickNoteDialog::onDeleteClicked() {
    if (QMessageBox::question(this, tr("Delete Note"), tr("Are you sure you want to delete this annotation?")) == QMessageBox::Yes) {
        emit noteDeleted(m_currentAnnotation.id);
        accept();
    }
}

void QuickNoteDialog::setAnnotation(const AnnotationRecord& ann) {
    m_currentAnnotation = ann;
    m_selectedText = ann.selectedText;
    if (m_quoteLabel) {
        QString display = m_selectedText.length() > 200 ? m_selectedText.left(197) + "..." : m_selectedText;
        m_quoteLabel->setText(QString("<i>\"%1\"</i>").arg(display.toHtmlEscaped()));
    }
    m_noteEdit->setPlainText(ann.note);
    m_selectedColor = ann.color;

    for (int i = 0; i < s_moonPlusColors.size(); ++i) {
        bool match = (QColor(s_moonPlusColors[i]) == ann.color);
        m_colorChips[i]->setChecked(match);
    }

    if (!ann.id.isEmpty()) {
        m_deleteBtn->show();
    }
}

AnnotationRecord QuickNoteDialog::getAnnotation() const {
    AnnotationRecord ann = m_currentAnnotation;
    ann.note = getNoteText();
    ann.color = getSelectedColor();
    ann.selectedText = m_selectedText;
    return ann;
}

QString QuickNoteDialog::getNoteText() const {
    return m_noteEdit->toPlainText().trimmed();
}

QColor QuickNoteDialog::getSelectedColor() const {
    return m_selectedColor;
}

QString QuickNoteDialog::getTags() const {
    return m_tagEdit->text().trimmed();
}

} // namespace LunaRead
