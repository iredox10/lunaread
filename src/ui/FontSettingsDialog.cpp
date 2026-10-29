#include "FontSettingsDialog.h"
#include "../features/CustomFontManager.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QButtonGroup>
#include <QFontDatabase>
#include <QScrollArea>

namespace LunaRead {

FontSettingsDialog::FontSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Visual Personalization & Typography"));
    resize(480, 560);
    setupUi();
    updateUiFromState();
}

FontSettingsDialog::~FontSettingsDialog() = default;

void FontSettingsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(14);

    // 1. Font Family & Size Section
    auto* fontHeader = new QLabel(tr("FONT & SIZE"), this);
    fontHeader->setStyleSheet("font-weight: bold; color: #64748B; font-size: 8pt;");
    mainLayout->addWidget(fontHeader);

    auto* fontRow = new QHBoxLayout();
    m_fontFamilyCombo = new QComboBox(this);
    populateFontFamilies();
    connect(m_fontFamilyCombo, &QComboBox::currentIndexChanged, this, &FontSettingsDialog::onFamilyComboChanged);

    m_loadCustomFontBtn = new QPushButton(tr("📁 Font..."), this);
    m_loadCustomFontBtn->setFixedHeight(32);
    m_loadCustomFontBtn->setToolTip(tr("Load custom .ttf or .otf font file"));
    connect(m_loadCustomFontBtn, &QPushButton::clicked, this, &FontSettingsDialog::onLoadCustomFontClicked);

    m_fontSizeDecreaseBtn = new QPushButton("A-", this);
    m_fontSizeDecreaseBtn->setFixedSize(36, 32);
    connect(m_fontSizeDecreaseBtn, &QPushButton::clicked, this, &FontSettingsDialog::onDecreaseFontSizeClicked);

    m_fontSizeSlider = new QSlider(Qt::Horizontal, this);
    m_fontSizeSlider->setRange(10, 48);
    m_fontSizeSlider->setValue(m_currentFont.pointSize());
    connect(m_fontSizeSlider, &QSlider::valueChanged, this, &FontSettingsDialog::onFontSizeSliderChanged);

    m_fontSizeIncreaseBtn = new QPushButton("A+", this);
    m_fontSizeIncreaseBtn->setFixedSize(36, 32);
    connect(m_fontSizeIncreaseBtn, &QPushButton::clicked, this, &FontSettingsDialog::onIncreaseFontSizeClicked);

    m_fontSizeValueLabel = new QLabel(QString::number(m_currentFont.pointSize()) + "pt", this);
    m_fontSizeValueLabel->setFixedWidth(36);
    m_fontSizeValueLabel->setAlignment(Qt::AlignCenter);

    fontRow->addWidget(m_fontFamilyCombo, 2);
    fontRow->addWidget(m_loadCustomFontBtn);
    fontRow->addWidget(m_fontSizeDecreaseBtn);
    fontRow->addWidget(m_fontSizeSlider, 3);
    fontRow->addWidget(m_fontSizeIncreaseBtn);
    fontRow->addWidget(m_fontSizeValueLabel);
    mainLayout->addLayout(fontRow);

    // 2. Line Spacing & Margins Section
    auto* layoutHeader = new QLabel(tr("SPACING & LAYOUT"), this);
    layoutHeader->setStyleSheet("font-weight: bold; color: #64748B; font-size: 8pt;");
    mainLayout->addWidget(layoutHeader);

    auto* gridLayout = new QGridLayout();

    gridLayout->addWidget(new QLabel(tr("Line Height:"), this), 0, 0);
    m_lineSpacingSlider = new QSlider(Qt::Horizontal, this);
    m_lineSpacingSlider->setRange(10, 30); // 1.0 to 3.0
    m_lineSpacingSlider->setValue(static_cast<int>(m_lineSpacing * 10));
    connect(m_lineSpacingSlider, &QSlider::valueChanged, this, &FontSettingsDialog::onLineSpacingSliderChanged);
    gridLayout->addWidget(m_lineSpacingSlider, 0, 1);
    m_lineSpacingValueLabel = new QLabel(QString::number(m_lineSpacing, 'f', 1) + "x", this);
    m_lineSpacingValueLabel->setFixedWidth(36);
    gridLayout->addWidget(m_lineSpacingValueLabel, 0, 2);

    gridLayout->addWidget(new QLabel(tr("Margins:"), this), 1, 0);
    m_marginsSlider = new QSlider(Qt::Horizontal, this);
    m_marginsSlider->setRange(12, 64);
    m_marginsSlider->setValue(m_paragraphMargins);
    connect(m_marginsSlider, &QSlider::valueChanged, this, &FontSettingsDialog::onMarginsSliderChanged);
    gridLayout->addWidget(m_marginsSlider, 1, 1);
    m_marginsValueLabel = new QLabel(QString::number(m_paragraphMargins) + "px", this);
    m_marginsValueLabel->setFixedWidth(36);
    gridLayout->addWidget(m_marginsValueLabel, 1, 2);

    gridLayout->addWidget(new QLabel(tr("First-line Indent:"), this), 2, 0);
    m_indentSlider = new QSlider(Qt::Horizontal, this);
    m_indentSlider->setRange(0, 48);
    m_indentSlider->setValue(m_paragraphIndent);
    connect(m_indentSlider, &QSlider::valueChanged, this, &FontSettingsDialog::onIndentSliderChanged);
    gridLayout->addWidget(m_indentSlider, 2, 1);
    m_indentValueLabel = new QLabel(QString::number(m_paragraphIndent) + "px", this);
    m_indentValueLabel->setFixedWidth(36);
    gridLayout->addWidget(m_indentValueLabel, 2, 2);

    m_dropCapsCheck = new QCheckBox(tr("Drop Caps (Ornate Chapter Opening)"), this);
    m_dropCapsCheck->setChecked(m_dropCaps);
    connect(m_dropCapsCheck, &QCheckBox::toggled, this, &FontSettingsDialog::onDropCapsToggled);
    gridLayout->addWidget(m_dropCapsCheck, 3, 0, 1, 3);

    mainLayout->addLayout(gridLayout);

    // 3. Text Alignment Section
    auto* alignHeader = new QLabel(tr("ALIGNMENT"), this);
    alignHeader->setStyleSheet("font-weight: bold; color: #64748B; font-size: 8pt;");
    mainLayout->addWidget(alignHeader);

    auto* alignRow = new QHBoxLayout();
    m_alignmentGroup = new QButtonGroup(this);

    m_alignJustifyBtn = new QPushButton(tr("Justified"), this);
    m_alignJustifyBtn->setCheckable(true);
    m_alignLeftBtn = new QPushButton(tr("Left"), this);
    m_alignLeftBtn->setCheckable(true);
    m_alignCenterBtn = new QPushButton(tr("Center"), this);
    m_alignCenterBtn->setCheckable(true);

    m_alignmentGroup->addButton(m_alignJustifyBtn, static_cast<int>(Qt::AlignJustify));
    m_alignmentGroup->addButton(m_alignLeftBtn, static_cast<int>(Qt::AlignLeft));
    m_alignmentGroup->addButton(m_alignCenterBtn, static_cast<int>(Qt::AlignHCenter));
    connect(m_alignmentGroup, &QButtonGroup::idClicked, this, &FontSettingsDialog::onAlignmentButtonClicked);

    alignRow->addWidget(m_alignJustifyBtn);
    alignRow->addWidget(m_alignLeftBtn);
    alignRow->addWidget(m_alignCenterBtn);
    mainLayout->addLayout(alignRow);

    // 3b. Page Turning Effect (Moon+ Reader Pro Signature Modes)
    auto* transHeader = new QLabel(tr("PAGE TURNING EFFECT"), this);
    transHeader->setStyleSheet("font-weight: bold; color: #64748B; font-size: 8pt;");
    mainLayout->addWidget(transHeader);

    auto* transRow = new QHBoxLayout();
    m_transitionGroup = new QButtonGroup(this);

    m_transNoneBtn = new QPushButton(tr("None (0ms)"), this);
    m_transNoneBtn->setCheckable(true);
    m_transSlideBtn = new QPushButton(tr("↔ Slide"), this);
    m_transSlideBtn->setCheckable(true);
    m_transFadeBtn = new QPushButton(tr("✨ Fade"), this);
    m_transFadeBtn->setCheckable(true);
    m_transCurlBtn = new QPushButton(tr("📖 3D Curl"), this);
    m_transCurlBtn->setCheckable(true);

    m_transitionGroup->addButton(m_transNoneBtn, static_cast<int>(PageTransitionEffect::None));
    m_transitionGroup->addButton(m_transSlideBtn, static_cast<int>(PageTransitionEffect::Slide));
    m_transitionGroup->addButton(m_transFadeBtn, static_cast<int>(PageTransitionEffect::Fade));
    m_transitionGroup->addButton(m_transCurlBtn, static_cast<int>(PageTransitionEffect::PageCurl));
    connect(m_transitionGroup, &QButtonGroup::idClicked, this, &FontSettingsDialog::onTransitionButtonClicked);

    transRow->addWidget(m_transNoneBtn);
    transRow->addWidget(m_transSlideBtn);
    transRow->addWidget(m_transFadeBtn);
    transRow->addWidget(m_transCurlBtn);
    mainLayout->addLayout(transRow);

    // 4. Color Palettes & Themes
    auto* themeHeader = new QLabel(tr("THEME PRESETS"), this);
    themeHeader->setStyleSheet("font-weight: bold; color: #64748B; font-size: 8pt;");
    mainLayout->addWidget(themeHeader);

    auto* themeRow = new QHBoxLayout();
    m_themeGroup = new QButtonGroup(this);

    const QVector<ReaderThemeMode> modes = {
        ReaderThemeMode::Parchment,
        ReaderThemeMode::OledBlack,
        ReaderThemeMode::Sepia,
        ReaderThemeMode::Night,
        ReaderThemeMode::ModernLight
    };

    for (int i = 0; i < modes.size(); ++i) {
        auto mode = modes[i];
        auto tokens = ReaderThemeTokens::fromMode(mode);
        auto* swatch = new ThemeSwatchButton(mode, tokens, this);
        m_swatchButtons.append(swatch);
        m_themeGroup->addButton(swatch, static_cast<int>(mode));
        themeRow->addWidget(swatch);

        connect(swatch, &QPushButton::clicked, this, [this, mode]() {
            onThemeSwatchClicked(mode);
        });
    }
    themeRow->addStretch(1);
    mainLayout->addLayout(themeRow);

    // 5. Live Preview Card
    m_previewCard = new QFrame(this);
    m_previewCard->setFrameShape(QFrame::StyledPanel);
    m_previewCard->setFixedHeight(110);
    auto* prevLayout = new QVBoxLayout(m_previewCard);
    m_previewTextLabel = new QLabel(
        tr("The Moon+ Reader desktop engine renders typography with subpixel precision, "
           "custom line heights, real paper textures, and responsive layout reflow."), m_previewCard);
    m_previewTextLabel->setWordWrap(true);
    prevLayout->addWidget(m_previewTextLabel);
    mainLayout->addWidget(m_previewCard);

    // 6. Action buttons
    auto* btnRow = new QHBoxLayout();
    m_resetBtn = new QPushButton(tr("Reset Defaults"), this);
    connect(m_resetBtn, &QPushButton::clicked, this, &FontSettingsDialog::resetToDefaults);

    m_doneBtn = new QPushButton(tr("Apply & Close"), this);
    m_doneBtn->setDefault(true);
    connect(m_doneBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnRow->addWidget(m_resetBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(m_doneBtn);
    mainLayout->addLayout(btnRow);
}

void FontSettingsDialog::populateFontFamilies() {
    m_fontFamilyCombo->clear();
    const auto custom = CustomFontManager::instance().customFontFamilies();
    for (const auto& fam : custom) {
        m_fontFamilyCombo->addItem(QStringLiteral("⭐ ") + fam, fam);
    }

    QStringList preferred = {"Roboto", "Georgia", "Merriweather", "Palatino", "Open Sans", "Courier New"};
    for (const auto& fam : preferred) {
        if (!custom.contains(fam)) {
            m_fontFamilyCombo->addItem(fam, fam);
        }
    }
    const auto families = QFontDatabase::families();
    for (const auto& f : families) {
        if (!preferred.contains(f) && !custom.contains(f)) {
            m_fontFamilyCombo->addItem(f, f);
        }
    }
}

void FontSettingsDialog::onLoadCustomFontClicked() {
    QString path = QFileDialog::getOpenFileName(this, tr("Load Custom Font"), QString(), tr("Font Files (*.ttf *.otf *.woff *.woff2);;All Files (*.*)"));
    if (!path.isEmpty()) {
        if (CustomFontManager::instance().loadFontFile(path)) {
            populateFontFamilies();
            const auto customList = CustomFontManager::instance().customFontFamilies();
            if (!customList.isEmpty()) {
                setFontFamily(customList.last());
            }
        }
    }
}

QFont FontSettingsDialog::currentFont() const {
    return m_currentFont;
}

void FontSettingsDialog::setCurrentFont(const QFont& font) {
    m_currentFont = font;
    updateUiFromState();
}

QString FontSettingsDialog::fontFamily() const {
    return m_currentFont.family();
}

void FontSettingsDialog::setFontFamily(const QString& family) {
    m_currentFont.setFamily(family);
    updateUiFromState();
}

int FontSettingsDialog::fontSize() const {
    return m_currentFont.pointSize();
}

void FontSettingsDialog::setFontSize(int pointSize) {
    m_currentFont.setPointSize(pointSize);
    updateUiFromState();
}

qreal FontSettingsDialog::lineSpacing() const {
    return m_lineSpacing;
}

void FontSettingsDialog::setLineSpacing(qreal multiplier) {
    m_lineSpacing = multiplier;
    updateUiFromState();
}

int FontSettingsDialog::paragraphMargins() const {
    return m_paragraphMargins;
}

void FontSettingsDialog::setParagraphMargins(int marginPx) {
    m_paragraphMargins = marginPx;
    updateUiFromState();
}

int FontSettingsDialog::paragraphIndent() const {
    return m_paragraphIndent;
}

void FontSettingsDialog::setParagraphIndent(int indentPx) {
    m_paragraphIndent = indentPx;
    updateUiFromState();
}

bool FontSettingsDialog::dropCapsEnabled() const {
    return m_dropCaps;
}

void FontSettingsDialog::setDropCapsEnabled(bool enabled) {
    m_dropCaps = enabled;
    updateUiFromState();
}

Qt::Alignment FontSettingsDialog::alignment() const {
    return m_alignment;
}

void FontSettingsDialog::setAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
    updateUiFromState();
}

PageTransitionEffect FontSettingsDialog::pageTransitionEffect() const {
    return m_transitionEffect;
}

void FontSettingsDialog::setPageTransitionEffect(PageTransitionEffect effect) {
    m_transitionEffect = effect;
    updateUiFromState();
}

ReaderThemeMode FontSettingsDialog::themeMode() const {
    return m_themeMode;
}

void FontSettingsDialog::setThemeMode(ReaderThemeMode mode) {
    m_themeMode = mode;
    updateUiFromState();
}

void FontSettingsDialog::resetToDefaults() {
    m_currentFont = QFont("Noto Serif", 14);
    m_lineSpacing = 1.6;
    m_paragraphMargins = 36;
    m_paragraphIndent = 24;
    m_dropCaps = true;
    m_alignment = Qt::AlignJustify;
    m_transitionEffect = PageTransitionEffect::None;
    m_themeMode = ReaderThemeMode::Parchment;
    updateUiFromState();

    emit fontChanged(m_currentFont);
    emit lineHeightChanged(m_lineSpacing);
    emit marginsChanged(m_paragraphMargins, m_paragraphMargins);
    emit paragraphIndentChanged(m_paragraphIndent);
    emit dropCapsChanged(m_dropCaps);
    emit alignmentChanged(m_alignment);
    emit pageTransitionEffectChanged(m_transitionEffect);
    emit themeChanged(m_themeMode);
}

void FontSettingsDialog::onFamilyComboChanged(int index) {
    if (m_updatingUi || index < 0) return;
    QString family = m_fontFamilyCombo->itemData(index).toString();
    if (family.isEmpty()) family = m_fontFamilyCombo->currentText();
    m_currentFont.setFamily(family);
    updatePreview();
    emit fontChanged(m_currentFont);
}

void FontSettingsDialog::onFontSizeSliderChanged(int value) {
    if (m_updatingUi) return;
    m_currentFont.setPointSize(value);
    m_fontSizeValueLabel->setText(QString::number(value) + "pt");
    updatePreview();
    emit fontChanged(m_currentFont);
}

void FontSettingsDialog::onDecreaseFontSizeClicked() {
    int cur = m_fontSizeSlider->value();
    if (cur > m_fontSizeSlider->minimum()) {
        m_fontSizeSlider->setValue(cur - 1);
    }
}

void FontSettingsDialog::onIncreaseFontSizeClicked() {
    int cur = m_fontSizeSlider->value();
    if (cur < m_fontSizeSlider->maximum()) {
        m_fontSizeSlider->setValue(cur + 1);
    }
}

void FontSettingsDialog::onLineSpacingSliderChanged(int value) {
    if (m_updatingUi) return;
    m_lineSpacing = value / 10.0;
    m_lineSpacingValueLabel->setText(QString::number(m_lineSpacing, 'f', 1) + "x");
    updatePreview();
    emit lineHeightChanged(m_lineSpacing);
}

void FontSettingsDialog::onMarginsSliderChanged(int value) {
    if (m_updatingUi) return;
    m_paragraphMargins = value;
    m_marginsValueLabel->setText(QString::number(m_paragraphMargins) + "px");
    emit marginsChanged(m_paragraphMargins, m_paragraphMargins);
}

void FontSettingsDialog::onIndentSliderChanged(int value) {
    if (m_updatingUi) return;
    m_paragraphIndent = value;
    m_indentValueLabel->setText(QString::number(m_paragraphIndent) + "px");
    emit paragraphIndentChanged(m_paragraphIndent);
}

void FontSettingsDialog::onDropCapsToggled(bool checked) {
    if (m_updatingUi) return;
    m_dropCaps = checked;
    emit dropCapsChanged(m_dropCaps);
}

void FontSettingsDialog::onAlignmentButtonClicked(int id) {
    if (m_updatingUi) return;
    m_alignment = static_cast<Qt::Alignment>(id);
    updatePreview();
    emit alignmentChanged(m_alignment);
}

void FontSettingsDialog::onTransitionButtonClicked(int id) {
    if (m_updatingUi) return;
    m_transitionEffect = static_cast<PageTransitionEffect>(id);
    emit pageTransitionEffectChanged(m_transitionEffect);
}

void FontSettingsDialog::onThemeSwatchClicked(ReaderThemeMode mode) {
    m_themeMode = mode;
    updatePreview();
    emit themeChanged(m_themeMode);
}

void FontSettingsDialog::updateUiFromState() {
    m_updatingUi = true;

    int idx = m_fontFamilyCombo->findText(m_currentFont.family());
    if (idx >= 0) m_fontFamilyCombo->setCurrentIndex(idx);

    m_fontSizeSlider->setValue(m_currentFont.pointSize());
    m_fontSizeValueLabel->setText(QString::number(m_currentFont.pointSize()) + "pt");

    m_lineSpacingSlider->setValue(static_cast<int>(m_lineSpacing * 10));
    m_lineSpacingValueLabel->setText(QString::number(m_lineSpacing, 'f', 1) + "x");

    m_marginsSlider->setValue(m_paragraphMargins);
    m_marginsValueLabel->setText(QString::number(m_paragraphMargins) + "px");

    if (m_indentSlider) m_indentSlider->setValue(m_paragraphIndent);
    if (m_indentValueLabel) m_indentValueLabel->setText(QString::number(m_paragraphIndent) + "px");
    if (m_dropCapsCheck) m_dropCapsCheck->setChecked(m_dropCaps);

    if (auto* btn = m_alignmentGroup->button(static_cast<int>(m_alignment))) {
        btn->setChecked(true);
    }

    if (m_transitionGroup) {
        if (auto* btn = m_transitionGroup->button(static_cast<int>(m_transitionEffect))) {
            btn->setChecked(true);
        }
    }

    for (auto* swatch : m_swatchButtons) {
        swatch->setChecked(swatch->mode() == m_themeMode);
    }

    m_updatingUi = false;
    updatePreview();
}

void FontSettingsDialog::updatePreview() {
    if (!m_previewCard || !m_previewTextLabel) return;

    auto tokens = ReaderThemeTokens::fromMode(m_themeMode);
    m_previewCard->setStyleSheet(QStringLiteral(
        "QFrame { background-color: %1; border-radius: 8px; border: 1px solid %2; padding: 12px; }"
    ).arg(tokens.backgroundColor.name(), tokens.dividerColor.name()));

    m_previewTextLabel->setFont(m_currentFont);
    m_previewTextLabel->setAlignment(m_alignment | Qt::AlignVCenter);
    m_previewTextLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(tokens.textColor.name()));
}

} // namespace LunaRead
