#pragma once

#include <QDialog>
#include <QPushButton>
#include <QWidget>
#include <QColor>
#include <QString>
#include <QJsonObject>
#include <QList>
#include <optional>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QSlider;
class QLabel;
class QLineEdit;
class QGroupBox;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Interactive button displaying a color swatch, hex code, and opening QColorDialog on click.
 */
class ColorPickerButton : public QPushButton {
    Q_OBJECT

public:
    explicit ColorPickerButton(const QColor& initialColor = Qt::white, QWidget* parent = nullptr);
    ~ColorPickerButton() override = default;

    [[nodiscard]] QColor color() const;
    void setColor(const QColor& color);

signals:
    void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void pickColor();

private:
    void updateAppearance();
    QColor m_color;
};

/**
 * @brief Live e-reader page snippet widget rendering book typography with selected theme tokens and dimmer.
 */
class ThemePreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit ThemePreviewWidget(QWidget* parent = nullptr);
    ~ThemePreviewWidget() override = default;

    void setTokens(const ReaderThemeTokens& tokens);
    [[nodiscard]] const ReaderThemeTokens& tokens() const;

    void setBrightness(int brightness);
    [[nodiscard]] int brightness() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ReaderThemeTokens m_tokens;
    int m_brightness = 100;
};

/**
 * @brief Modal dialog for creating, customizing, saving, importing, and exporting LunaRead reading themes.
 */
class ThemeCustomizerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ThemeCustomizerDialog(QWidget* parent = nullptr);
    explicit ThemeCustomizerDialog(const ReaderThemeTokens& initialTokens, QWidget* parent = nullptr);
    ~ThemeCustomizerDialog() override;

    [[nodiscard]] ReaderThemeTokens currentTokens() const;
    [[nodiscard]] ReaderThemeTokens effectiveTokens() const;
    void setTokens(const ReaderThemeTokens& tokens);

    [[nodiscard]] int brightness() const;
    void setBrightness(int percent);

    // Color pickers getters and setters
    [[nodiscard]] QColor backgroundColor() const;
    [[nodiscard]] QColor textColor() const;
    [[nodiscard]] QColor accentColor() const;
    [[nodiscard]] QColor dividerColor() const;

    void setBackgroundColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setAccentColor(const QColor& color);
    void setDividerColor(const QColor& color);

    // Serialization & Storage Helpers
    static QJsonObject tokensToJson(const ReaderThemeTokens& tokens, int brightness = 100);
    static ReaderThemeTokens tokensFromJson(const QJsonObject& json, int* outBrightness = nullptr);
    static bool exportThemeToFile(const ReaderThemeTokens& tokens, int brightness, const QString& filePath);
    static std::optional<ReaderThemeTokens> importThemeFromFile(const QString& filePath, int* outBrightness = nullptr);
    static QList<ReaderThemeTokens> loadCustomThemes();
    static bool saveCustomTheme(const ReaderThemeTokens& tokens, int brightness = 100);
    static bool deleteCustomTheme(const QString& themeId);

signals:
    void themeCreated(const ReaderThemeTokens& tokens);
    void themeSaved(const ReaderThemeTokens& tokens);
    void themeDeleted(const QString& themeId);

public slots:
    void applyTheme();
    void saveCurrentTheme();
    void deleteSelectedCustomTheme();
    void importThemeJson();
    void exportThemeJson();
    void resetToDefault();

private slots:
    void onPresetComboChanged(int index);
    void onColorPickerChanged();
    void onBrightnessSliderChanged(int value);
    void toggleAdvancedColors();

private:
    void setupUi();
    void populatePresets();
    void updateUiFromState();
    void updatePreview();
    void syncTokensFromPickers();

    // UI Widgets
    QComboBox* m_presetCombo = nullptr;
    QPushButton* m_deleteThemeBtn = nullptr;

    ThemePreviewWidget* m_previewWidget = nullptr;

    QSlider* m_brightnessSlider = nullptr;
    QLabel* m_brightnessValueLabel = nullptr;

    // Primary Color Pickers
    ColorPickerButton* m_bgColorBtn = nullptr;
    ColorPickerButton* m_textColorBtn = nullptr;
    ColorPickerButton* m_accentColorBtn = nullptr;
    ColorPickerButton* m_dividerColorBtn = nullptr;

    // Advanced Color Pickers
    ColorPickerButton* m_controlBgBtn = nullptr;
    ColorPickerButton* m_headerFooterBtn = nullptr;
    ColorPickerButton* m_selectionColorBtn = nullptr;
    QWidget* m_advancedColorsContainer = nullptr;
    QPushButton* m_toggleAdvancedBtn = nullptr;

    // Theme Name & Custom Actions
    QLineEdit* m_themeNameEdit = nullptr;
    QPushButton* m_saveThemeBtn = nullptr;

    // Import / Export
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;

    // Dialog Buttons
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    // State
    ReaderThemeTokens m_tokens;
    int m_brightness = 100;
    bool m_updatingUi = false;
    bool m_advancedColorsCustomized = false;
};

} // namespace LunaRead
