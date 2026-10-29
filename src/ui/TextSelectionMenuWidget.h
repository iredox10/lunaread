#pragma once

#include <QWidget>
#include <QPushButton>
#include <QFrame>
#include <QColor>
#include <QString>
#include <QVector>
#include <QRect>
#include <QPoint>
#include <QList>

#include "../core/ThemeTokens.h"

namespace LunaRead {

/**
 * @brief Circular color button representing a highlight swatch.
 *
 * Renders an antialiased circular color badge with interactive hover halo
 * and pressed state animations.
 */
class HighlighterCircleButton : public QPushButton {
    Q_OBJECT

public:
    explicit HighlighterCircleButton(const QColor& color, const QString& colorName, QWidget* parent = nullptr);
    ~HighlighterCircleButton() override = default;

    [[nodiscard]] QColor color() const noexcept { return m_color; }
    [[nodiscard]] QString colorName() const { return m_colorName; }
    void setColor(const QColor& color);
    void setActive(bool active);
    [[nodiscard]] bool isActive() const noexcept { return m_active; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_color;
    QString m_colorName;
    bool m_active = false;
};

/**
 * @brief Floating, rounded, drop-shadowed contextual popup bar for text selection in reader.
 *
 * Appears dynamically above selected text (or below if near top of viewport)
 * similar to Moon+ Reader and iOS/Android reader popups.
 *
 * Features:
 * - 5 Highlighter color circle buttons:
 *     Yellow  (#FFE082)
 *     Green   (#A5D6A7)
 *     Cyan    (#80DEEA)
 *     Pink    (#F48FB1)
 *     Purple  (#CE93D8)
 * - Underline button (emits underlineRequested)
 * - Strikethrough button (emits strikethroughRequested)
 * - Add Note button (emits addNoteRequested)
 * - Copy button (copies to QClipboard and emits copyRequested)
 * - Dictionary / Define button (emits defineRequested)
 * - Translate button (emits translateRequested)
 * - Speak / TTS button (emits speakRequested)
 * - Search in Book button (emits searchRequested)
 * - Delete button, shown only when managing an existing highlight (emits deleteRequested)
 * - Close button (emits dismissed)
 */
class TextSelectionMenuWidget : public QWidget {
    Q_OBJECT

public:
    struct HighlightColorOption {
        QString name;
        QColor color;
    };

    /// Returns the 5 Moon+ Reader signature pastel highlighter colors
    static const QVector<HighlightColorOption>& defaultHighlightColors();

    explicit TextSelectionMenuWidget(QWidget* parent = nullptr);
    ~TextSelectionMenuWidget() override;

    // Selected text payload
    void setSelectedText(const QString& text);
    [[nodiscard]] QString selectedText() const;

    // Positioning and display
    void showAboveSelection(const QRect& selectionRect, const QPoint& globalOffset = QPoint());
    void showAt(const QPoint& targetPos, const QRect& bounds = QRect());
    void dismiss();

    // Theme integration
    void updateTheme(const ReaderThemeTokens& tokens);
    [[nodiscard]] const ReaderThemeTokens& themeTokens() const noexcept { return m_tokens; }

    // Configuration
    void setDismissOnAction(bool dismiss) noexcept { m_dismissOnAction = dismiss; }
    [[nodiscard]] bool dismissOnAction() const noexcept { return m_dismissOnAction; }

    // Managing an existing highlight (Moon+ Reader: tap a highlight to recolor/delete)
    void setEditingAnnotation(const QString& annotationId, const QColor& currentColor);
    void clearEditingAnnotation();
    [[nodiscard]] bool isEditingAnnotation() const noexcept { return !m_editingAnnotationId.isEmpty(); }
    [[nodiscard]] QString editingAnnotationId() const { return m_editingAnnotationId; }

    void setCompactMode(bool compact);
    [[nodiscard]] bool isCompactMode() const noexcept { return m_compactMode; }

    // Accessors for buttons
    [[nodiscard]] QList<HighlighterCircleButton*> highlightButtons() const { return m_colorButtons; }
    [[nodiscard]] QPushButton* underlineButton() const noexcept { return m_underlineBtn; }
    [[nodiscard]] QPushButton* strikethroughButton() const noexcept { return m_strikethroughBtn; }
    [[nodiscard]] QPushButton* addNoteButton() const noexcept { return m_addNoteBtn; }
    [[nodiscard]] QPushButton* copyButton() const noexcept { return m_copyBtn; }
    [[nodiscard]] QPushButton* defineButton() const noexcept { return m_defineBtn; }
    [[nodiscard]] QPushButton* translateButton() const noexcept { return m_translateBtn; }
    [[nodiscard]] QPushButton* speakButton() const noexcept { return m_speakBtn; }
    [[nodiscard]] QPushButton* searchButton() const noexcept { return m_searchBtn; }
    [[nodiscard]] QPushButton* closeButton() const noexcept { return m_closeBtn; }

signals:
    // Highlight signals
    void highlightRequested(const QString& text, const QColor& color);
    void highlightColorRequested(const QColor& color);
    void deleteRequested(const QString& annotationId);

    // Text formatting signals
    void underlineRequested(const QString& text);
    void strikethroughRequested(const QString& text);

    // Action signals
    void addNoteRequested(const QString& text);
    void copyRequested(const QString& text);
    void defineRequested(const QString& text);
    void translateRequested(const QString& text);
    void speakRequested(const QString& text);
    void searchRequested(const QString& text);

    // Menu lifecycle signal
    void dismissed();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void setupStyle();

    // Custom vector icon generators
    [[nodiscard]] QIcon drawUnderlineIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawStrikethroughIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawNoteIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawCopyIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawDefineIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawTranslateIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawSpeakIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawSearchIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawDeleteIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawCloseIcon(const QColor& color) const;

    static constexpr int ShadowMargin = 10;

    QString m_selectedText;
    ReaderThemeTokens m_tokens;
    bool m_dismissOnAction = true;
    bool m_compactMode = false;
    QString m_editingAnnotationId;

    // Controls
    QList<HighlighterCircleButton*> m_colorButtons;
    QPushButton* m_underlineBtn = nullptr;
    QPushButton* m_strikethroughBtn = nullptr;
    QPushButton* m_addNoteBtn = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_defineBtn = nullptr;
    QPushButton* m_translateBtn = nullptr;
    QPushButton* m_speakBtn = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QFrame* m_hDivider = nullptr;
    QFrame* m_vDivider = nullptr;
};

} // namespace LunaRead
