#pragma once

#include <QWidget>
#include <QColor>
#include <QTimer>
#include <QRectF>

namespace LunaRead {

/**
 * @brief Color preset options for the Reading Ruler / Focus Guide.
 */
enum class RulerColor {
    GoldenYellow, ///< Warm golden amber tone (classic Moon+ Reader focus yellow)
    SoftCyan,     ///< Calming cool cyan / sky blue tone
    SubtleGrey,   ///< Neutral reading grey tone for minimal distraction
    Custom        ///< User-defined custom color
};

/**
 * @brief Operating modes for ruler positioning.
 */
enum class RulerMode {
    FollowMouse,    ///< Mode 1: Smoothly follows cursor Y position
    FixedOrKeyboard ///< Mode 2: Fixed position or keyboard / drag controlled
};

/**
 * @brief Reading Ruler / Focus Guide overlay widget for LunaRead.
 *
 * Implements Moon+ Reader's Reading Ruler & Focus Guide:
 * - Semi-transparent overlay widget placed across the reading area to highlight
 *   the active line while slightly dimming upper and lower regions.
 * - Adjustable ruler height (24px to 80px, clamped [16, 300]).
 * - Adjustable opacity (10% to 50%, clamped [0.05, 0.85]).
 * - Color presets: Golden yellow, Soft cyan, Subtle grey, or Custom.
 * - Mode 1: Follow mouse cursor Y position smoothly (60 FPS smooth interpolation).
 * - Mode 2: Fixed position or keyboard / drag controlled.
 * - Toggleable on/off via slot toggleRuler().
 * - setAttribute(Qt::WA_TransparentForMouseEvents, false) while passing unhandled
 *   mouse clicks and wheel events directly to the parent reader / underlying widgets.
 */
class ReadingRulerWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal rulerY READ rulerY WRITE setRulerY NOTIFY rulerYChanged)
    Q_PROPERTY(int rulerHeight READ rulerHeight WRITE setRulerHeight NOTIFY rulerHeightChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(RulerColor rulerColor READ rulerColor WRITE setRulerColor NOTIFY rulerColorChanged)
    Q_PROPERTY(QColor customColor READ customColor WRITE setCustomColor NOTIFY customColorChanged)
    Q_PROPERTY(RulerMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool dimSurroundings READ dimSurroundings WRITE setDimSurroundings NOTIFY dimSurroundingsChanged)
    Q_PROPERTY(qreal dimOpacity READ dimOpacity WRITE setDimOpacity NOTIFY dimOpacityChanged)
    Q_PROPERTY(bool smoothMovement READ smoothMovement WRITE setSmoothMovement NOTIFY smoothMovementChanged)
    Q_PROPERTY(int guideLineThickness READ guideLineThickness WRITE setGuideLineThickness NOTIFY guideLineThicknessChanged)
    Q_PROPERTY(bool passClicksThrough READ passClicksThrough WRITE setPassClicksThrough NOTIFY passClicksThroughChanged)

public:
    static constexpr int MinRecommendedHeight = 24;
    static constexpr int MaxRecommendedHeight = 80;
    static constexpr int MinHeight = 16;
    static constexpr int MaxHeight = 300;
    static constexpr int DefaultHeight = 42;

    static constexpr qreal MinRecommendedOpacity = 0.10;
    static constexpr qreal MaxRecommendedOpacity = 0.50;
    static constexpr qreal MinOpacity = 0.05;
    static constexpr qreal MaxOpacity = 0.85;
    static constexpr qreal DefaultOpacity = 0.25;

    static constexpr qreal DefaultDimOpacity = 0.25;
    static constexpr int DefaultStepSize = 28;

    explicit ReadingRulerWidget(QWidget* parent = nullptr);
    ~ReadingRulerWidget() override = default;

    // --- Geometry & Position ---
    [[nodiscard]] qreal rulerY() const noexcept { return m_currentY; }
    void setRulerY(qreal y);

    [[nodiscard]] int rulerHeight() const noexcept { return m_rulerHeight; }
    void setRulerHeight(int height);

    [[nodiscard]] QRectF rulerRect() const;

    // --- Opacity ---
    [[nodiscard]] qreal opacity() const noexcept { return m_opacity; }
    void setOpacity(qreal opacity);

    [[nodiscard]] int opacityPercent() const noexcept { return static_cast<int>(m_opacity * 100.0 + 0.5); }
    void setOpacityPercent(int percent);

    // --- Color ---
    [[nodiscard]] RulerColor rulerColor() const noexcept { return m_rulerColor; }
    void setRulerColor(RulerColor color);

    [[nodiscard]] QColor customColor() const noexcept { return m_customColor; }
    void setCustomColor(const QColor& color);

    [[nodiscard]] QColor effectiveColor() const;
    [[nodiscard]] QColor effectiveBorderColor() const;
    static QColor presetColor(RulerColor color);

    // --- Mode ---
    [[nodiscard]] RulerMode mode() const noexcept { return m_mode; }
    void setMode(RulerMode mode);

    // --- Dimming ---
    [[nodiscard]] bool dimSurroundings() const noexcept { return m_dimSurroundings; }
    void setDimSurroundings(bool dim);

    [[nodiscard]] qreal dimOpacity() const noexcept { return m_dimOpacity; }
    void setDimOpacity(qreal opacity);

    // --- Smooth Movement ---
    [[nodiscard]] bool smoothMovement() const noexcept { return m_smoothMovement; }
    void setSmoothMovement(bool smooth);

    [[nodiscard]] qreal smoothDamping() const noexcept { return m_smoothDamping; }
    void setSmoothDamping(qreal damping);

    // --- Line Thickness & Step Size ---
    [[nodiscard]] int guideLineThickness() const noexcept { return m_guideLineThickness; }
    void setGuideLineThickness(int thickness);

    [[nodiscard]] int stepSize() const noexcept { return m_stepSize; }
    void setStepSize(int step);

    // --- Mouse Pass-through Control ---
    [[nodiscard]] bool passClicksThrough() const noexcept { return m_passClicksThrough; }
    void setPassClicksThrough(bool pass);

    [[nodiscard]] bool isDragging() const noexcept { return m_isDragging; }

public slots:
    void toggleRuler();
    void showRuler();
    void hideRuler();
    void setRulerVisible(bool visible);

    void stepUp();
    void stepDown();
    void moveRuler(qreal deltaY);
    void jumpToTop();
    void jumpToBottom();

    void cycleColor();
    void cycleMode();

    void increaseHeight(int delta = 4);
    void decreaseHeight(int delta = 4);
    void increaseOpacity(qreal delta = 0.05);
    void decreaseOpacity(qreal delta = 0.05);

signals:
    void rulerToggled(bool visible);
    void rulerYChanged(qreal y);
    void rulerHeightChanged(int height);
    void opacityChanged(qreal opacity);
    void rulerColorChanged(RulerColor color);
    void customColorChanged(const QColor& color);
    void modeChanged(RulerMode mode);
    void dimSurroundingsChanged(bool dim);
    void dimOpacityChanged(qreal opacity);
    void smoothMovementChanged(bool smooth);
    void guideLineThicknessChanged(int thickness);
    void passClicksThroughChanged(bool pass);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onSmoothTimerTick();

private:
    void setTargetY(qreal targetY);
    void clampRulerPosition();
    void forwardMouseEventToUnderlying(QMouseEvent* event);
    void forwardWheelEventToUnderlying(QWheelEvent* event);
    void drawGripHandles(class QPainter& painter, const QRectF& rect);

    // Ruler properties
    qreal m_currentY = 120.0;
    qreal m_targetY = 120.0;
    int m_rulerHeight = DefaultHeight;
    qreal m_opacity = DefaultOpacity;
    RulerColor m_rulerColor = RulerColor::GoldenYellow;
    QColor m_customColor = QColor(250, 204, 21);
    RulerMode m_mode = RulerMode::FollowMouse;

    // Visual options
    bool m_dimSurroundings = true;
    qreal m_dimOpacity = DefaultDimOpacity;
    int m_guideLineThickness = 2;
    int m_stepSize = DefaultStepSize;

    // Smooth movement
    bool m_smoothMovement = true;
    qreal m_smoothDamping = 0.30;
    QTimer* m_smoothTimer = nullptr;

    // Interaction states
    bool m_passClicksThrough = true;
    bool m_isDragging = false;
    qreal m_dragOffsetY = 0.0;
    bool m_isForwardingEvent = false;
};

} // namespace LunaRead
