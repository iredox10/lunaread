#pragma once

#include <QDialog>
#include <QFont>
#include <QMarginsF>
#include <QVector>
#include <QStringList>
#include <QPushButton>
#include <QPainter>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QSlider;
class QLabel;
class QButtonGroup;
class QFrame;
class QVBoxLayout;
class QHBoxLayout;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Circular color swatch button representing a reader theme palette.
 */
class ThemeSwatchButton : public QPushButton {
    Q_OBJECT

public:
    ThemeSwatchButton(ReaderThemeMode mode, const ReaderThemeTokens& tokens, QWidget* parent = nullptr)
        : QPushButton(parent), m_mode(mode), m_tokens(tokens) {
        setFixedSize(38, 38);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tokens.name);
        setFocusPolicy(Qt::StrongFocus);
        setStyleSheet("QPushButton { border: none; background: transparent; }");
    }

    [[nodiscard]] ReaderThemeMode mode() const { return m_mode; }
    [[nodiscard]] const ReaderThemeTokens& tokens() const { return m_tokens; }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QPointF center = rect().center();
        const qreal radius = 13.0;

        // Outer focus/active ring
        if (isChecked()) {
            QPen ringPen(m_tokens.accentColor, 2.5);
            p.setPen(ringPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(center, radius + 3.5, radius + 3.5);
        }

        // Swatch circle with border
        p.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
        p.setBrush(m_tokens.backgroundColor);
        p.drawEllipse(center, radius, radius);

        // Center dot when checked
        if (isChecked()) {
            p.setPen(Qt::NoPen);
            p.setBrush(m_tokens.textColor);
            p.drawEllipse(center, 4.0, 4.0);
        }
    }

private:
    ReaderThemeMode m_mode;
    ReaderThemeTokens m_tokens;
};

/**
 * @brief Visual popover modal dialog for reading personalization in LunaRead-Qt.
 *
 * Implements Moon+ Reader's iconic reading settings:
 * - Font Family Picker: Curated & system serif / sans-serif fonts (Roboto, Georgia, Merriweather, Open Sans, Palatino, Courier)
 * - Font Size Slider: 10pt to 72pt with live preview label and +/- stepper buttons
 * - Line Spacing Slider: 1.0x to 3.0x (increments of 0.1)
 * - Paragraph Margins Slider: 12px to 64px
 * - Alignment Selector: Justify, Left, Center
 * - Theme Palette Selector: 5 circular swatches (Parchment, OLED Black, Sepia, Night, Modern Light)
 * - Live Typography Preview Card
 */
class FontSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit FontSettingsDialog(QWidget* parent = nullptr);
    ~FontSettingsDialog() override;

    // Font Family & Size
    [[nodiscard]] QFont currentFont() const;
    void setCurrentFont(const QFont& font);

    [[nodiscard]] QString fontFamily() const;
    void setFontFamily(const QString& family);

    [[nodiscard]] int fontSize() const;
    void setFontSize(int pointSize);

    // Line Spacing & Margins
    [[nodiscard]] qreal lineSpacing() const;
    void setLineSpacing(qreal multiplier);

    [[nodiscard]] int paragraphMargins() const;
    void setParagraphMargins(int marginPx);

    // Alignment
    [[nodiscard]] Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    // Theme Mode
    [[nodiscard]] ReaderThemeMode themeMode() const;
    void setThemeMode(ReaderThemeMode mode);

    // Reset to Moon+ Reader defaults
    void resetToDefaults();

signals:
    void fontChanged(const QFont& font);
    void lineHeightChanged(qreal height);
    void marginsChanged(qreal horizontal, qreal vertical);
    void themeChanged(ReaderThemeMode mode);
    void alignmentChanged(Qt::Alignment alignment);

private slots:
    void onFamilyComboChanged(int index);
    void onFontSizeSliderChanged(int value);
    void onDecreaseFontSizeClicked();
    void onIncreaseFontSizeClicked();
    void onLineSpacingSliderChanged(int value);
    void onMarginsSliderChanged(int value);
    void onAlignmentButtonClicked(int id);
    void onThemeSwatchClicked(ReaderThemeMode mode);

private:
    void setupUi();
    void populateFontFamilies();
    void updatePreview();
    void updateUiFromState();

    // UI Widgets
    QComboBox* m_fontFamilyCombo = nullptr;

    QSlider* m_fontSizeSlider = nullptr;
    QLabel* m_fontSizeValueLabel = nullptr;
    QPushButton* m_fontSizeDecreaseBtn = nullptr;
    QPushButton* m_fontSizeIncreaseBtn = nullptr;

    QSlider* m_lineSpacingSlider = nullptr;
    QLabel* m_lineSpacingValueLabel = nullptr;

    QSlider* m_marginsSlider = nullptr;
    QLabel* m_marginsValueLabel = nullptr;

    QButtonGroup* m_alignmentGroup = nullptr;
    QPushButton* m_alignJustifyBtn = nullptr;
    QPushButton* m_alignLeftBtn = nullptr;
    QPushButton* m_alignCenterBtn = nullptr;

    QButtonGroup* m_themeGroup = nullptr;
    QVector<ThemeSwatchButton*> m_swatchButtons;

    QFrame* m_previewCard = nullptr;
    QLabel* m_previewTextLabel = nullptr;

    QPushButton* m_resetBtn = nullptr;
    QPushButton* m_doneBtn = nullptr;

    // State
    QFont m_currentFont{"Roboto", 14};
    qreal m_lineSpacing = 1.5;
    int m_paragraphMargins = 24;
    Qt::Alignment m_alignment = Qt::AlignJustify;
    ReaderThemeMode m_themeMode = ReaderThemeMode::Parchment;

    bool m_updatingUi = false;
};

} // namespace LunaRead
