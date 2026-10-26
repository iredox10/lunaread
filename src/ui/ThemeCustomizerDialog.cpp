#include "ThemeCustomizerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QScrollArea>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QSettings>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <algorithm>

namespace LunaRead {

// ============================================================================
// ColorPickerButton Implementation
// ============================================================================

ColorPickerButton::ColorPickerButton(const QColor& initialColor, QWidget* parent)
    : QPushButton(parent), m_color(initialColor) {
    setFixedHeight(34);
    setMinimumWidth(110);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    connect(this, &QPushButton::clicked, this, &ColorPickerButton::pickColor);
    updateAppearance();
}

QColor ColorPickerButton::color() const {
    return m_color;
}

void ColorPickerButton::setColor(const QColor& color) {
    if (m_color != color) {
        m_color = color;
        updateAppearance();
        emit colorChanged(m_color);
    }
}

void ColorPickerButton::pickColor() {
    QColor selected = QColorDialog::getColor(m_color, this, tr("Select Color"), QColorDialog::ShowAlphaChannel);
    if (selected.isValid()) {
        setColor(selected);
    }
}

void ColorPickerButton::updateAppearance() {
    QString hex = m_color.alpha() < 255 ? m_color.name(QColor::HexArgb).toUpper()
                                        : m_color.name(QColor::HexRgb).toUpper();
    setToolTip(QString("%1 (Click to change)").arg(hex));
    update();
}

void ColorPickerButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(1, 1, -1, -1);

    // Frame background and border
    QColor borderColor = isDown() ? QColor(0x25, 0x63, 0xEB)
                        : (hasFocus() ? QColor(0x3B, 0x82, 0xF6)
                                      : QColor(0xCB, 0xD5, 0xE1));
    p.setPen(QPen(borderColor, 1.2));
    p.setBrush(QColor(0xF8, 0xFA, 0xFC));
    p.drawRoundedRect(r, 6, 6);

    // Color Swatch
    const QRect swatchRect(r.left() + 6, r.top() + (r.height() - 20) / 2, 26, 20);
    if (m_color.alpha() < 255) {
        p.fillRect(swatchRect, Qt::lightGray);
    }
    p.fillRect(swatchRect, m_color);
    p.setPen(QPen(QColor(0, 0, 0, 50), 1.0));
    p.drawRect(swatchRect);

    // Color Hex Text
    p.setPen(QColor(0x1E, 0x29, 0x3B));
    QFont font("Monospace", 9, QFont::DemiBold);
    font.setStyleHint(QFont::Monospace);
    p.setFont(font);

    QString hex = m_color.alpha() < 255 ? m_color.name(QColor::HexArgb).toUpper()
                                        : m_color.name(QColor::HexRgb).toUpper();
    const QRect textRect(swatchRect.right() + 8, r.top(), r.width() - swatchRect.width() - 14, r.height());
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, hex);
}

// ============================================================================
// ThemePreviewWidget Implementation
// ============================================================================

ThemePreviewWidget::ThemePreviewWidget(QWidget* parent)
    : QWidget(parent), m_tokens(ReaderThemeTokens::parchment()), m_brightness(100) {
    setMinimumHeight(185);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ThemePreviewWidget::setTokens(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    update();
}

const ReaderThemeTokens& ThemePreviewWidget::tokens() const {
    return m_tokens;
}

void ThemePreviewWidget::setBrightness(int brightness) {
    m_brightness = std::clamp(brightness, 0, 100);
    update();
}

int ThemePreviewWidget::brightness() const {
    return m_brightness;
}

void ThemePreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect r = rect().adjusted(2, 2, -2, -2);

    // Outer frame & card background
    p.setPen(QPen(m_tokens.dividerColor, 1.5));
    p.setBrush(m_tokens.backgroundColor);
    p.drawRoundedRect(r, 8, 8);

    QPainterPath clipPath;
    clipPath.addRoundedRect(r, 8, 8);
    p.setClipPath(clipPath);

    // 1. Header Area
    const QRect headerRect(r.left() + 16, r.top() + 8, r.width() - 32, 20);
    p.setPen(m_tokens.headerFooterColor);
    QFont headerFont("Segoe UI", 8, QFont::Bold);
    p.setFont(headerFont);
    p.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, tr("CHAPTER 1 • DOWN THE RABBIT-HOLE"));
    p.drawText(headerRect, Qt::AlignRight | Qt::AlignVCenter, tr("Page 14 (12%)"));

    // Header divider line
    const int dividerY1 = r.top() + 32;
    p.setPen(QPen(m_tokens.dividerColor, 1.0));
    p.drawLine(r.left() + 14, dividerY1, r.right() - 14, dividerY1);

    // 2. Body Text Area
    QFont bodyFont("Georgia", 10);
    p.setFont(bodyFont);
    QFontMetrics fm(bodyFont);

    const int lineSpacing = fm.lineSpacing() + 3;
    int curY = dividerY1 + 14;

    // Body Line 1
    p.setPen(m_tokens.textColor);
    p.drawText(r.left() + 18, curY + fm.ascent(),
               tr("Alice was beginning to get very tired of sitting by her sister on the bank,"));

    // Body Line 2 with selection highlight
    curY += lineSpacing;
    const QString prefixText = tr("and of having nothing to do: once or twice she had ");
    const QString highlightText = tr("peeped into the book");
    const int prefixWidth = fm.horizontalAdvance(prefixText);
    const int highlightWidth = fm.horizontalAdvance(highlightText);

    p.drawText(r.left() + 18, curY + fm.ascent(), prefixText);

    const QRect highlightRect(r.left() + 18 + prefixWidth, curY, highlightWidth, fm.height());
    p.fillRect(highlightRect, m_tokens.selectionColor);
    p.drawText(r.left() + 18 + prefixWidth, curY + fm.ascent(), highlightText);

    // Body Line 3: Accent / Link Text
    curY += lineSpacing + 4;
    p.setPen(m_tokens.accentColor);
    QFont linkFont = bodyFont;
    linkFont.setUnderline(true);
    p.setFont(linkFont);
    p.drawText(r.left() + 18, curY + fm.ascent(),
               tr("‘and what is the use of a book without pictures or conversations?’ →"));

    // 3. Footer Divider Line
    const int dividerY2 = r.bottom() - 28;
    p.setPen(QPen(m_tokens.dividerColor, 1.0));
    p.drawLine(r.left() + 14, dividerY2, r.right() - 14, dividerY2);

    // 4. Footer Area
    const QRect footerRect(r.left() + 16, dividerY2 + 4, r.width() - 32, 20);
    p.setPen(m_tokens.headerFooterColor);
    p.setFont(headerFont);
    p.drawText(footerRect, Qt::AlignLeft | Qt::AlignVCenter, tr("LunaRead Desktop Engine"));
    p.drawText(footerRect, Qt::AlignRight | Qt::AlignVCenter, tr("Typographic Reflow • 240 WPM"));

    // 5. Brightness Dimmer Overlay
    if (m_brightness < 100) {
        int alpha = std::clamp(static_cast<int>((100 - m_brightness) * 2.55), 0, 255);
        p.fillRect(r, QColor(0, 0, 0, alpha));
    }
}

// ============================================================================
// ThemeCustomizerDialog Implementation
// ============================================================================

ThemeCustomizerDialog::ThemeCustomizerDialog(QWidget* parent)
    : ThemeCustomizerDialog(ReaderThemeTokens::parchment(), parent) {
}

ThemeCustomizerDialog::ThemeCustomizerDialog(const ReaderThemeTokens& initialTokens, QWidget* parent)
    : QDialog(parent), m_tokens(initialTokens) {
    setWindowTitle(tr("Reader Theme Customizer"));
    resize(620, 720);
    setMinimumSize(540, 600);

    setupUi();
    updateUiFromState();
}

ThemeCustomizerDialog::~ThemeCustomizerDialog() = default;

void ThemeCustomizerDialog::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 16, 16, 16);
    outerLayout->setSpacing(12);

    // Scroll area for flexible window sizes
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget(scrollArea);
    auto* mainLayout = new QVBoxLayout(scrollContent);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(14);

    const QString sectionHeaderStyle =
        "font-weight: bold; color: #64748B; font-size: 8pt; text-transform: uppercase;";

    // ------------------------------------------------------------------------
    // 1. Preset Selector Header
    // ------------------------------------------------------------------------
    auto* presetHeader = new QLabel(tr("THEME PRESETS & PROFILES"), this);
    presetHeader->setStyleSheet(sectionHeaderStyle);
    mainLayout->addWidget(presetHeader);

    auto* presetRow = new QHBoxLayout();
    m_presetCombo = new QComboBox(this);
    m_presetCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    populatePresets();
    connect(m_presetCombo, &QComboBox::currentIndexChanged, this, &ThemeCustomizerDialog::onPresetComboChanged);
    presetRow->addWidget(m_presetCombo, 1);

    m_deleteThemeBtn = new QPushButton(tr("Delete"), this);
    m_deleteThemeBtn->setToolTip(tr("Delete selected custom theme"));
    m_deleteThemeBtn->setEnabled(false);
    connect(m_deleteThemeBtn, &QPushButton::clicked, this, &ThemeCustomizerDialog::deleteSelectedCustomTheme);
    presetRow->addWidget(m_deleteThemeBtn);

    mainLayout->addLayout(presetRow);

    // ------------------------------------------------------------------------
    // 2. Live Preview
    // ------------------------------------------------------------------------
    auto* previewHeader = new QLabel(tr("LIVE READING PREVIEW"), this);
    previewHeader->setStyleSheet(sectionHeaderStyle);
    mainLayout->addWidget(previewHeader);

    m_previewWidget = new ThemePreviewWidget(this);
    mainLayout->addWidget(m_previewWidget);

    // ------------------------------------------------------------------------
    // 3. Brightness / Dimmer Slider
    // ------------------------------------------------------------------------
    auto* brightnessHeaderRow = new QHBoxLayout();
    auto* brightnessLabel = new QLabel(tr("BRIGHTNESS / DIMMER"), this);
    brightnessLabel->setStyleSheet(sectionHeaderStyle);
    m_brightnessValueLabel = new QLabel("100%", this);
    m_brightnessValueLabel->setStyleSheet("font-weight: bold; color: #1E293B;");
    brightnessHeaderRow->addWidget(brightnessLabel);
    brightnessHeaderRow->addStretch();
    brightnessHeaderRow->addWidget(m_brightnessValueLabel);
    mainLayout->addLayout(brightnessHeaderRow);

    m_brightnessSlider = new QSlider(Qt::Horizontal, this);
    m_brightnessSlider->setRange(0, 100);
    m_brightnessSlider->setValue(m_brightness);
    m_brightnessSlider->setTickPosition(QSlider::TicksBelow);
    m_brightnessSlider->setTickInterval(10);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &ThemeCustomizerDialog::onBrightnessSliderChanged);
    mainLayout->addWidget(m_brightnessSlider);

    // ------------------------------------------------------------------------
    // 4. Color Pickers (Primary Colors)
    // ------------------------------------------------------------------------
    auto* colorsHeader = new QLabel(tr("COLOR PALETTE"), this);
    colorsHeader->setStyleSheet(sectionHeaderStyle);
    mainLayout->addWidget(colorsHeader);

    auto* colorGrid = new QGridLayout();
    colorGrid->setSpacing(10);

    colorGrid->addWidget(new QLabel(tr("Background Color:"), this), 0, 0);
    m_bgColorBtn = new ColorPickerButton(m_tokens.backgroundColor, this);
    connect(m_bgColorBtn, &ColorPickerButton::colorChanged, this, &ThemeCustomizerDialog::onColorPickerChanged);
    colorGrid->addWidget(m_bgColorBtn, 0, 1);

    colorGrid->addWidget(new QLabel(tr("Text Color:"), this), 0, 2);
    m_textColorBtn = new ColorPickerButton(m_tokens.textColor, this);
    connect(m_textColorBtn, &ColorPickerButton::colorChanged, this, &ThemeCustomizerDialog::onColorPickerChanged);
    colorGrid->addWidget(m_textColorBtn, 0, 3);

    colorGrid->addWidget(new QLabel(tr("Accent & Links:"), this), 1, 0);
    m_accentColorBtn = new ColorPickerButton(m_tokens.accentColor, this);
    connect(m_accentColorBtn, &ColorPickerButton::colorChanged, this, &ThemeCustomizerDialog::onColorPickerChanged);
    colorGrid->addWidget(m_accentColorBtn, 1, 1);

    colorGrid->addWidget(new QLabel(tr("Divider & Border:"), this), 1, 2);
    m_dividerColorBtn = new ColorPickerButton(m_tokens.dividerColor, this);
    connect(m_dividerColorBtn, &ColorPickerButton::colorChanged, this, &ThemeCustomizerDialog::onColorPickerChanged);
    colorGrid->addWidget(m_dividerColorBtn, 1, 3);

    mainLayout->addLayout(colorGrid);

    // Advanced Colors Toggle
    m_toggleAdvancedBtn = new QPushButton(tr("▾ Show Advanced Colors (Surface, Selection, Headers)"), this);
    m_toggleAdvancedBtn->setFlat(true);
    m_toggleAdvancedBtn->setCursor(Qt::PointingHandCursor);
    m_toggleAdvancedBtn->setStyleSheet("text-align: left; color: #2563EB; font-weight: demi-bold; padding: 2px 0px;");
    connect(m_toggleAdvancedBtn, &QPushButton::clicked, this, &ThemeCustomizerDialog::toggleAdvancedColors);
    mainLayout->addWidget(m_toggleAdvancedBtn);

    // Advanced Colors Container
    m_advancedColorsContainer = new QWidget(this);
    auto* advGrid = new QGridLayout(m_advancedColorsContainer);
    advGrid->setContentsMargins(0, 4, 0, 4);
    advGrid->setSpacing(10);

    advGrid->addWidget(new QLabel(tr("Card / Control Surface:"), this), 0, 0);
    m_controlBgBtn = new ColorPickerButton(m_tokens.controlBgColor, this);
    connect(m_controlBgBtn, &ColorPickerButton::colorChanged, this, [this](const QColor&) {
        m_advancedColorsCustomized = true;
        onColorPickerChanged();
    });
    advGrid->addWidget(m_controlBgBtn, 0, 1);

    advGrid->addWidget(new QLabel(tr("Header / Footer Text:"), this), 0, 2);
    m_headerFooterBtn = new ColorPickerButton(m_tokens.headerFooterColor, this);
    connect(m_headerFooterBtn, &ColorPickerButton::colorChanged, this, [this](const QColor&) {
        m_advancedColorsCustomized = true;
        onColorPickerChanged();
    });
    advGrid->addWidget(m_headerFooterBtn, 0, 3);

    advGrid->addWidget(new QLabel(tr("Selection Highlight:"), this), 1, 0);
    m_selectionColorBtn = new ColorPickerButton(m_tokens.selectionColor, this);
    connect(m_selectionColorBtn, &ColorPickerButton::colorChanged, this, [this](const QColor&) {
        m_advancedColorsCustomized = true;
        onColorPickerChanged();
    });
    advGrid->addWidget(m_selectionColorBtn, 1, 1);

    m_advancedColorsContainer->hide();
    mainLayout->addWidget(m_advancedColorsContainer);

    // ------------------------------------------------------------------------
    // 5. Theme Name & Save Section
    // ------------------------------------------------------------------------
    auto* saveHeader = new QLabel(tr("SAVE AS CUSTOM THEME"), this);
    saveHeader->setStyleSheet(sectionHeaderStyle);
    mainLayout->addWidget(saveHeader);

    auto* saveRow = new QHBoxLayout();
    m_themeNameEdit = new QLineEdit(this);
    m_themeNameEdit->setPlaceholderText(tr("Theme Name (e.g. Midnight Sapphire)"));
    m_themeNameEdit->setText(m_tokens.name);
    saveRow->addWidget(m_themeNameEdit, 1);

    m_saveThemeBtn = new QPushButton(tr("💾 Save as Custom Theme"), this);
    connect(m_saveThemeBtn, &QPushButton::clicked, this, &ThemeCustomizerDialog::saveCurrentTheme);
    saveRow->addWidget(m_saveThemeBtn);
    mainLayout->addLayout(saveRow);

    // ------------------------------------------------------------------------
    // 6. JSON Import / Export & Reset
    // ------------------------------------------------------------------------
    auto* fileActionsRow = new QHBoxLayout();
    m_importBtn = new QPushButton(tr("📥 Import JSON..."), this);
    connect(m_importBtn, &QPushButton::clicked, this, &ThemeCustomizerDialog::importThemeJson);

    m_exportBtn = new QPushButton(tr("📤 Export JSON..."), this);
    connect(m_exportBtn, &QPushButton::clicked, this, &ThemeCustomizerDialog::exportThemeJson);

    m_resetBtn = new QPushButton(tr("↺ Reset Default"), this);
    connect(m_resetBtn, &QPushButton::clicked, this, &ThemeCustomizerDialog::resetToDefault);

    fileActionsRow->addWidget(m_importBtn);
    fileActionsRow->addWidget(m_exportBtn);
    fileActionsRow->addStretch();
    fileActionsRow->addWidget(m_resetBtn);
    mainLayout->addLayout(fileActionsRow);

    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea, 1);

    // ------------------------------------------------------------------------
    // 7. Bottom Dialog Action Buttons
    // ------------------------------------------------------------------------
    auto* bottomBtnRow = new QHBoxLayout();
    bottomBtnRow->setContentsMargins(0, 4, 0, 0);

    m_cancelBtn = new QPushButton(tr("Close"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_applyBtn = new QPushButton(tr("Apply Theme"), this);
    m_applyBtn->setDefault(true);
    m_applyBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 16px; }");
    connect(m_applyBtn, &QPushButton::clicked, this, &ThemeCustomizerDialog::applyTheme);

    bottomBtnRow->addStretch(1);
    bottomBtnRow->addWidget(m_cancelBtn);
    bottomBtnRow->addWidget(m_applyBtn);
    outerLayout->addLayout(bottomBtnRow);
}

void ThemeCustomizerDialog::populatePresets() {
    m_presetCombo->blockSignals(true);
    m_presetCombo->clear();

    // 1. Standard Built-in presets
    struct BuiltinItem {
        QString label;
        QString id;
    };
    const QVector<BuiltinItem> builtins = {
        { tr("Parchment"), "parchment" },
        { tr("OLED Pure Black"), "oledBlack" },
        { tr("Warm Sepia"), "sepia" },
        { tr("Night Charcoal"), "night" },
        { tr("Modern Light"), "modernLight" }
    };

    for (const auto& b : builtins) {
        m_presetCombo->addItem(b.label, QString("builtin:" + b.id));
    }

    // 2. Custom Presets from QSettings
    auto customThemes = loadCustomThemes();
    if (!customThemes.isEmpty()) {
        m_presetCombo->insertSeparator(m_presetCombo->count());
        for (const auto& ct : customThemes) {
            m_presetCombo->addItem(ct.name + tr(" (Custom)"), QString("custom:" + ct.id));
        }
    }

    m_presetCombo->blockSignals(false);
}

void ThemeCustomizerDialog::toggleAdvancedColors() {
    bool isVisible = m_advancedColorsContainer->isVisible();
    m_advancedColorsContainer->setVisible(!isVisible);
    m_toggleAdvancedBtn->setText(!isVisible
        ? tr("▴ Hide Advanced Colors (Surface, Selection, Headers)")
        : tr("▾ Show Advanced Colors (Surface, Selection, Headers)"));
}

void ThemeCustomizerDialog::onPresetComboChanged(int index) {
    if (m_updatingUi || index < 0) return;

    QString presetData = m_presetCombo->itemData(index).toString();
    if (presetData.startsWith("builtin:")) {
        QString id = presetData.mid(8);
        if (id == "parchment") m_tokens = ReaderThemeTokens::parchment();
        else if (id == "oledBlack") m_tokens = ReaderThemeTokens::oledBlack();
        else if (id == "sepia") m_tokens = ReaderThemeTokens::sepia();
        else if (id == "night") m_tokens = ReaderThemeTokens::night();
        else if (id == "modernLight") m_tokens = ReaderThemeTokens::modernLight();

        m_brightness = 100;
        m_advancedColorsCustomized = false;
        m_deleteThemeBtn->setEnabled(false);
        updateUiFromState();
    } else if (presetData.startsWith("custom:")) {
        QString id = presetData.mid(7);
        auto customList = loadCustomThemes();
        for (const auto& ct : customList) {
            if (ct.id == id) {
                m_tokens = ct;
                m_advancedColorsCustomized = true;
                m_deleteThemeBtn->setEnabled(true);
                updateUiFromState();
                break;
            }
        }
    }
}

void ThemeCustomizerDialog::onColorPickerChanged() {
    if (m_updatingUi) return;

    syncTokensFromPickers();
    updatePreview();

    // If currently on a built-in preset, hint that it's customized
    const int curIdx = m_presetCombo->currentIndex();
    if (curIdx >= 0 && curIdx < 5) {
        if (!m_themeNameEdit->text().endsWith(tr(" (Custom)"))) {
            m_themeNameEdit->setText(m_tokens.name + tr(" (Custom)"));
        }
    }
}

void ThemeCustomizerDialog::onBrightnessSliderChanged(int value) {
    if (m_updatingUi) return;
    m_brightness = value;
    m_brightnessValueLabel->setText(QString("%1%").arg(m_brightness));
    updatePreview();
}

void ThemeCustomizerDialog::syncTokensFromPickers() {
    m_tokens.backgroundColor = m_bgColorBtn->color();
    m_tokens.textColor = m_textColorBtn->color();
    m_tokens.accentColor = m_accentColorBtn->color();
    m_tokens.dividerColor = m_dividerColorBtn->color();

    // Determine dark / light theme based on background luminance
    qreal lum = 0.299 * m_tokens.backgroundColor.redF()
              + 0.587 * m_tokens.backgroundColor.greenF()
              + 0.114 * m_tokens.backgroundColor.blueF();
    m_tokens.isDark = (lum < 0.5);

    if (!m_advancedColorsCustomized) {
        m_tokens.controlBgColor = m_tokens.isDark
            ? m_tokens.backgroundColor.lighter(125)
            : m_tokens.backgroundColor.darker(106);

        m_tokens.headerFooterColor = QColor(
            static_cast<int>(m_tokens.textColor.red() * 0.65 + m_tokens.backgroundColor.red() * 0.35),
            static_cast<int>(m_tokens.textColor.green() * 0.65 + m_tokens.backgroundColor.green() * 0.35),
            static_cast<int>(m_tokens.textColor.blue() * 0.65 + m_tokens.backgroundColor.blue() * 0.35)
        );

        m_tokens.selectionColor = QColor(
            m_tokens.accentColor.red(),
            m_tokens.accentColor.green(),
            m_tokens.accentColor.blue(),
            120
        );

        m_updatingUi = true;
        m_controlBgBtn->setColor(m_tokens.controlBgColor);
        m_headerFooterBtn->setColor(m_tokens.headerFooterColor);
        m_selectionColorBtn->setColor(m_tokens.selectionColor);
        m_updatingUi = false;
    } else {
        m_tokens.controlBgColor = m_controlBgBtn->color();
        m_tokens.headerFooterColor = m_headerFooterBtn->color();
        m_tokens.selectionColor = m_selectionColorBtn->color();
    }
}

void ThemeCustomizerDialog::updateUiFromState() {
    m_updatingUi = true;

    m_themeNameEdit->setText(m_tokens.name);

    m_bgColorBtn->setColor(m_tokens.backgroundColor);
    m_textColorBtn->setColor(m_tokens.textColor);
    m_accentColorBtn->setColor(m_tokens.accentColor);
    m_dividerColorBtn->setColor(m_tokens.dividerColor);

    m_controlBgBtn->setColor(m_tokens.controlBgColor);
    m_headerFooterBtn->setColor(m_tokens.headerFooterColor);
    m_selectionColorBtn->setColor(m_tokens.selectionColor);

    m_brightnessSlider->setValue(m_brightness);
    m_brightnessValueLabel->setText(QString("%1%").arg(m_brightness));

    m_updatingUi = false;
    updatePreview();
}

void ThemeCustomizerDialog::updatePreview() {
    if (!m_previewWidget) return;
    m_previewWidget->setTokens(m_tokens);
    m_previewWidget->setBrightness(m_brightness);
}

ReaderThemeTokens ThemeCustomizerDialog::currentTokens() const {
    return m_tokens;
}

ReaderThemeTokens ThemeCustomizerDialog::effectiveTokens() const {
    ReaderThemeTokens t = m_tokens;
    if (m_brightness < 100) {
        auto dim = [this](const QColor& c) -> QColor {
            double factor = std::clamp(m_brightness, 0, 100) / 100.0;
            return QColor(
                std::clamp(static_cast<int>(c.red() * factor), 0, 255),
                std::clamp(static_cast<int>(c.green() * factor), 0, 255),
                std::clamp(static_cast<int>(c.blue() * factor), 0, 255),
                c.alpha()
            );
        };
        t.backgroundColor = dim(t.backgroundColor);
        t.controlBgColor = dim(t.controlBgColor);
        t.textColor = dim(t.textColor);
        t.accentColor = dim(t.accentColor);
        t.headerFooterColor = dim(t.headerFooterColor);
        t.dividerColor = dim(t.dividerColor);
        t.selectionColor = dim(t.selectionColor);

        qreal lum = 0.299 * t.backgroundColor.redF()
                  + 0.587 * t.backgroundColor.greenF()
                  + 0.114 * t.backgroundColor.blueF();
        t.isDark = (lum < 0.5);
    }
    return t;
}

void ThemeCustomizerDialog::setTokens(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    updateUiFromState();
}

int ThemeCustomizerDialog::brightness() const {
    return m_brightness;
}

void ThemeCustomizerDialog::setBrightness(int percent) {
    m_brightness = std::clamp(percent, 0, 100);
    if (m_brightnessSlider) {
        m_brightnessSlider->setValue(m_brightness);
    }
    if (m_brightnessValueLabel) {
        m_brightnessValueLabel->setText(QString("%1%").arg(m_brightness));
    }
    updatePreview();
}

QColor ThemeCustomizerDialog::backgroundColor() const {
    return m_bgColorBtn ? m_bgColorBtn->color() : m_tokens.backgroundColor;
}

QColor ThemeCustomizerDialog::textColor() const {
    return m_textColorBtn ? m_textColorBtn->color() : m_tokens.textColor;
}

QColor ThemeCustomizerDialog::accentColor() const {
    return m_accentColorBtn ? m_accentColorBtn->color() : m_tokens.accentColor;
}

QColor ThemeCustomizerDialog::dividerColor() const {
    return m_dividerColorBtn ? m_dividerColorBtn->color() : m_tokens.dividerColor;
}

void ThemeCustomizerDialog::setBackgroundColor(const QColor& color) {
    if (m_bgColorBtn) m_bgColorBtn->setColor(color);
}

void ThemeCustomizerDialog::setTextColor(const QColor& color) {
    if (m_textColorBtn) m_textColorBtn->setColor(color);
}

void ThemeCustomizerDialog::setAccentColor(const QColor& color) {
    if (m_accentColorBtn) m_accentColorBtn->setColor(color);
}

void ThemeCustomizerDialog::setDividerColor(const QColor& color) {
    if (m_dividerColorBtn) m_dividerColorBtn->setColor(color);
}

void ThemeCustomizerDialog::applyTheme() {
    syncTokensFromPickers();
    m_tokens.name = m_themeNameEdit->text().trimmed();
    if (m_tokens.name.isEmpty()) {
        m_tokens.name = tr("Custom Theme");
    }

    emit themeCreated(effectiveTokens());
    accept();
}

void ThemeCustomizerDialog::saveCurrentTheme() {
    QString name = m_themeNameEdit->text().trimmed();
    if (name.isEmpty()) {
        name = tr("Custom Theme %1").arg(QDateTime::currentDateTime().toString("MMM d, hh:mm"));
        m_themeNameEdit->setText(name);
    }

    syncTokensFromPickers();
    m_tokens.name = name;
    if (m_tokens.id.isEmpty() || !m_tokens.id.startsWith("custom_")) {
        m_tokens.id = "custom_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    }

    if (saveCustomTheme(m_tokens, m_brightness)) {
        emit themeSaved(m_tokens);
        populatePresets();

        QString targetData = "custom:" + m_tokens.id;
        for (int i = 0; i < m_presetCombo->count(); ++i) {
            if (m_presetCombo->itemData(i).toString() == targetData) {
                m_presetCombo->setCurrentIndex(i);
                m_deleteThemeBtn->setEnabled(true);
                break;
            }
        }

        QMessageBox::information(
            this,
            tr("Theme Saved"),
            tr("Theme '%1' has been saved successfully to your custom themes!").arg(name)
        );
    } else {
        QMessageBox::warning(
            this,
            tr("Save Failed"),
            tr("Could not save the theme to preferences.")
        );
    }
}

void ThemeCustomizerDialog::deleteSelectedCustomTheme() {
    int idx = m_presetCombo->currentIndex();
    if (idx < 0) return;

    QString data = m_presetCombo->itemData(idx).toString();
    if (!data.startsWith("custom:")) return;

    QString id = data.mid(7);
    QString name = m_presetCombo->currentText();

    auto reply = QMessageBox::question(
        this,
        tr("Delete Custom Theme"),
        tr("Are you sure you want to delete the theme '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (deleteCustomTheme(id)) {
            emit themeDeleted(id);
            populatePresets();
            m_presetCombo->setCurrentIndex(0);
        }
    }
}

void ThemeCustomizerDialog::importThemeJson() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import Theme (JSON)"),
        QString(),
        tr("JSON Theme Files (*.json);;All Files (*.*)")
    );
    if (filePath.isEmpty()) return;

    int importedBrightness = 100;
    auto opt = importThemeFromFile(filePath, &importedBrightness);
    if (!opt.has_value()) {
        QMessageBox::warning(
            this,
            tr("Import Failed"),
            tr("Failed to parse theme from '%1'. Ensure it is valid JSON.").arg(filePath)
        );
        return;
    }

    m_tokens = *opt;
    m_brightness = importedBrightness;
    m_advancedColorsCustomized = true;

    updateUiFromState();

    auto reply = QMessageBox::question(
        this,
        tr("Save Imported Theme?"),
        tr("Theme '%1' was successfully imported. Would you like to save it to your presets now?").arg(m_tokens.name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    if (reply == QMessageBox::Yes) {
        saveCurrentTheme();
    }
}

void ThemeCustomizerDialog::exportThemeJson() {
    syncTokensFromPickers();
    m_tokens.name = m_themeNameEdit->text().trimmed();
    if (m_tokens.name.isEmpty()) m_tokens.name = "Custom Theme";

    QString suggestedName = m_tokens.name.toLower().replace(' ', '_') + ".json";
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Theme (JSON)"),
        suggestedName,
        tr("JSON Theme Files (*.json);;All Files (*.*)")
    );
    if (filePath.isEmpty()) return;

    if (exportThemeToFile(m_tokens, m_brightness, filePath)) {
        QMessageBox::information(
            this,
            tr("Export Successful"),
            tr("Theme '%1' was exported to '%2'").arg(m_tokens.name, filePath)
        );
    } else {
        QMessageBox::warning(
            this,
            tr("Export Failed"),
            tr("Failed to export theme to '%1'").arg(filePath)
        );
    }
}

void ThemeCustomizerDialog::resetToDefault() {
    m_tokens = ReaderThemeTokens::parchment();
    m_brightness = 100;
    m_advancedColorsCustomized = false;
    updateUiFromState();

    for (int i = 0; i < m_presetCombo->count(); ++i) {
        if (m_presetCombo->itemData(i).toString() == "builtin:parchment") {
            m_presetCombo->setCurrentIndex(i);
            break;
        }
    }
}

// ============================================================================
// Static Helpers: Serialization, File I/O, QSettings
// ============================================================================

QJsonObject ThemeCustomizerDialog::tokensToJson(const ReaderThemeTokens& tokens, int brightness) {
    QJsonObject obj;
    obj["version"] = 1;
    obj["id"] = tokens.id;
    obj["name"] = tokens.name;
    obj["backgroundColor"] = tokens.backgroundColor.name(QColor::HexArgb);
    obj["textColor"] = tokens.textColor.name(QColor::HexArgb);
    obj["accentColor"] = tokens.accentColor.name(QColor::HexArgb);
    obj["dividerColor"] = tokens.dividerColor.name(QColor::HexArgb);
    obj["controlBgColor"] = tokens.controlBgColor.name(QColor::HexArgb);
    obj["headerFooterColor"] = tokens.headerFooterColor.name(QColor::HexArgb);
    obj["selectionColor"] = tokens.selectionColor.name(QColor::HexArgb);
    obj["isDark"] = tokens.isDark;
    obj["brightness"] = brightness;
    return obj;
}

ReaderThemeTokens ThemeCustomizerDialog::tokensFromJson(const QJsonObject& obj, int* outBrightness) {
    ReaderThemeTokens t;
    t.id = obj.value("id").toString();
    t.name = obj.value("name").toString();
    if (t.name.isEmpty()) t.name = "Custom Theme";
    if (t.id.isEmpty()) t.id = "custom_" + QString::number(QDateTime::currentMSecsSinceEpoch());

    t.backgroundColor = QColor(obj.value("backgroundColor").toString("#FBF0D9"));
    t.textColor = QColor(obj.value("textColor").toString("#3A2E2B"));
    t.accentColor = QColor(obj.value("accentColor").toString("#8D6E63"));
    t.dividerColor = QColor(obj.value("dividerColor").toString("#E0D3B8"));

    if (obj.contains("isDark")) {
        t.isDark = obj.value("isDark").toBool();
    } else {
        qreal lum = 0.299 * t.backgroundColor.redF()
                  + 0.587 * t.backgroundColor.greenF()
                  + 0.114 * t.backgroundColor.blueF();
        t.isDark = (lum < 0.5);
    }

    if (obj.contains("controlBgColor")) {
        t.controlBgColor = QColor(obj.value("controlBgColor").toString());
    } else {
        t.controlBgColor = t.isDark ? t.backgroundColor.lighter(125) : t.backgroundColor.darker(106);
    }

    if (obj.contains("headerFooterColor")) {
        t.headerFooterColor = QColor(obj.value("headerFooterColor").toString());
    } else {
        t.headerFooterColor = QColor(
            static_cast<int>(t.textColor.red() * 0.65 + t.backgroundColor.red() * 0.35),
            static_cast<int>(t.textColor.green() * 0.65 + t.backgroundColor.green() * 0.35),
            static_cast<int>(t.textColor.blue() * 0.65 + t.backgroundColor.blue() * 0.35)
        );
    }

    if (obj.contains("selectionColor")) {
        t.selectionColor = QColor(obj.value("selectionColor").toString());
    } else {
        t.selectionColor = QColor(t.accentColor.red(), t.accentColor.green(), t.accentColor.blue(), 120);
    }

    if (outBrightness) {
        *outBrightness = obj.value("brightness").toInt(100);
    }

    return t;
}

bool ThemeCustomizerDialog::exportThemeToFile(const ReaderThemeTokens& tokens, int brightness, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QJsonObject obj = tokensToJson(tokens, brightness);
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

std::optional<ReaderThemeTokens> ThemeCustomizerDialog::importThemeFromFile(const QString& filePath, int* outBrightness) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    return tokensFromJson(doc.object(), outBrightness);
}

QList<ReaderThemeTokens> ThemeCustomizerDialog::loadCustomThemes() {
    QSettings settings("LunaRead", "LunaRead");
    QByteArray data = settings.value("CustomThemes/themes_json").toByteArray();
    if (data.isEmpty()) return {};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return {};

    QList<ReaderThemeTokens> list;
    QJsonArray arr = doc.array();
    for (const auto& v : arr) {
        if (v.isObject()) {
            list.append(tokensFromJson(v.toObject()));
        }
    }
    return list;
}

bool ThemeCustomizerDialog::saveCustomTheme(const ReaderThemeTokens& tokens, int brightness) {
    if (tokens.id.isEmpty() && tokens.name.isEmpty()) return false;

    QSettings settings("LunaRead", "LunaRead");
    QByteArray data = settings.value("CustomThemes/themes_json").toByteArray();
    QJsonArray arr;
    if (!data.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
    }

    QJsonObject newObj = tokensToJson(tokens, brightness);
    bool replaced = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject existing = arr[i].toObject();
        if (existing.value("id").toString() == tokens.id || existing.value("name").toString() == tokens.name) {
            arr[i] = newObj;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        arr.append(newObj);
    }

    settings.setValue("CustomThemes/themes_json", QJsonDocument(arr).toJson(QJsonDocument::Compact));
    settings.sync();
    return true;
}

bool ThemeCustomizerDialog::deleteCustomTheme(const QString& themeId) {
    QSettings settings("LunaRead", "LunaRead");
    QByteArray data = settings.value("CustomThemes/themes_json").toByteArray();
    if (data.isEmpty()) return false;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return false;

    QJsonArray arr = doc.array();
    QJsonArray newArr;
    bool found = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject existing = arr[i].toObject();
        if (existing.value("id").toString() == themeId || existing.value("name").toString() == themeId) {
            found = true;
        } else {
            newArr.append(existing);
        }
    }
    if (!found) return false;

    settings.setValue("CustomThemes/themes_json", QJsonDocument(newArr).toJson(QJsonDocument::Compact));
    settings.sync();
    return true;
}

} // namespace LunaRead
