#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QElapsedTimer>
#include <QPoint>

#include "../core/ThemeTokens.h"

namespace LunaRead {

/**
 * @brief Auto-scroll modes mimicking Moon+ Reader's iconic hands-free reading options.
 */
enum class AutoScrollMode {
    PixelByPixel,   ///< Continuous smooth roll (speed in pixels/sec)
    LineByLine,     ///< Discrete line-by-line roll (speed in lines/sec)
    PageByPage      ///< Timed automatic page turn (speed in seconds/page)
};

/**
 * @brief Floating semi-transparent HUD overlay for Moon+ Reader style Hands-Free Auto-Scroll.
 *
 * Positioned near the bottom/right of the reader viewport:
 * - Play / Pause button with responsive visual icon toggle
 * - Speed controls: '-' and '+' buttons, mouse wheel support, and speed readout
 *   (default 30 px/s, adjustable 5 to 200 px/s in pixel mode, 1 to 15 lines/s in line mode)
 * - Mode toggle: Continuous Pixel-by-pixel roll vs Line-by-line roll
 * - High-precision internal QTimer + QElapsedTimer driving smooth scrolling ticks
 * - Tap on reader screen to pause/resume
 * - Keyboard shortcuts (Space to toggle, Up/Down for speed, M for mode, Esc to dismiss)
 * - Draggable HUD overlay within viewport bounds
 * - Frosted-glass translucent styling conforming to dark and light reader theme tokens
 *
 * Signals:
 * - scrollStepRequested(int pixelDelta): Emits pixel delta to be scrolled by viewport
 * - autoScrollToggled(bool active): Emitted when auto-scrolling starts or pauses
 * - speedChanged(int speed): Emitted when speed value changes
 * - modeChanged(AutoScrollMode mode): Emitted when scrolling mode is toggled
 * - closeRequested(): Emitted when HUD overlay is dismissed
 */
class AutoScrollOverlayWidget : public QWidget {
    Q_OBJECT

public:
    // Pixel-by-pixel mode speed bounds and steps
    static constexpr int DefaultPixelSpeed = 30; // 30 px/s
    static constexpr int MinPixelSpeed = 5;      // 5 px/s
    static constexpr int MaxPixelSpeed = 200;    // 200 px/s
    static constexpr int PixelSpeedStep = 5;     // 5 px/s per adjustment

    // Line-by-line mode speed bounds and steps
    static constexpr int DefaultLineSpeed = 2;   // 2 lines/s
    static constexpr int MinLineSpeed = 1;       // 1 line/s
    static constexpr int MaxLineSpeed = 15;      // 15 lines/s
    static constexpr int LineSpeedStep = 1;      // 1 line/s per adjustment

    // Page-by-page timed turn bounds and steps
    static constexpr int DefaultPageIntervalSec = 15; // 15 seconds / page
    static constexpr int MinPageIntervalSec = 3;       // 3 seconds / page
    static constexpr int MaxPageIntervalSec = 90;      // 90 seconds / page
    static constexpr int PageIntervalStep = 2;         // 2 seconds per adjustment

    static constexpr int DefaultLineHeight = 28; // Standard line step height in pixels

    explicit AutoScrollOverlayWidget(QWidget* parent = nullptr);
    ~AutoScrollOverlayWidget() override;

    // State queries
    [[nodiscard]] bool isActive() const noexcept { return m_isActive; }
    [[nodiscard]] bool isScrolling() const noexcept { return m_isActive; }
    [[nodiscard]] AutoScrollMode mode() const noexcept { return m_mode; }
    [[nodiscard]] int pixelSpeed() const noexcept { return m_pixelSpeed; }
    [[nodiscard]] int lineSpeed() const noexcept { return m_lineSpeed; }
    [[nodiscard]] int pageIntervalSec() const noexcept { return m_pageIntervalSec; }
    [[nodiscard]] int currentSpeed() const noexcept;
    [[nodiscard]] int lineHeight() const noexcept { return m_lineHeight; }
    [[nodiscard]] bool isTapToPauseEnabled() const noexcept { return m_tapToPauseEnabled; }
    [[nodiscard]] bool isTapToResumeEnabled() const noexcept { return m_tapToResumeEnabled; }
    [[nodiscard]] bool isShortcutsEnabled() const noexcept { return m_shortcutsEnabled; }
    [[nodiscard]] bool startOnShow() const noexcept { return m_startOnShow; }

    // Configuration setters
    void setMode(AutoScrollMode mode);
    void setPixelSpeed(int pxPerSec);
    void setLineSpeed(int linesPerSec);
    void setPageIntervalSec(int sec);
    void setSpeed(int speed);
    void setLineHeight(int height);
    void setTapToPauseEnabled(bool enabled) noexcept { m_tapToPauseEnabled = enabled; }
    void setTapToResumeEnabled(bool enabled) noexcept { m_tapToResumeEnabled = enabled; }
    void setShortcutsEnabled(bool enabled) noexcept { m_shortcutsEnabled = enabled; }
    void setStartOnShow(bool start) noexcept { m_startOnShow = start; }

    // Theme integration
    void updateTheme(const ReaderThemeTokens& tokens);

    // Viewport positioning
    void reposition(const QSize& parentSize, bool forceReset = false);
    void resetToDefaultPosition();

public slots:
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void showAndStart();
    void increaseSpeed();
    void decreaseSpeed();
    void toggleMode();
    void dismiss();
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void scrollStepRequested(int pixelDelta);
    void pageTurnRequested(int delta);
    void autoScrollToggled(bool active);
    void speedChanged(int speed);
    void modeChanged(AutoScrollMode mode);
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onScrollTick();

private:
    void setupUi();
    void setupStyle();
    void updatePlayPauseUi();
    void updateSpeedLabel();
    void updateModeButtonUi();

    // Vector icon generators
    [[nodiscard]] QIcon drawPlayIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawPauseIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawMinusIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawPlusIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawModeIcon(AutoScrollMode mode, const QColor& color) const;
    [[nodiscard]] QIcon drawCloseIcon(const QColor& color) const;

    // Sub-widgets
    QPushButton* m_playPauseBtn = nullptr;
    QPushButton* m_modeBtn = nullptr;
    QPushButton* m_speedMinusBtn = nullptr;
    QLabel* m_speedLabel = nullptr;
    QPushButton* m_speedPlusBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // Auto-scroll parameters
    bool m_isActive = false;
    AutoScrollMode m_mode = AutoScrollMode::PixelByPixel;
    int m_pixelSpeed = DefaultPixelSpeed;
    int m_lineSpeed = DefaultLineSpeed;
    int m_pageIntervalSec = DefaultPageIntervalSec;
    int m_lineHeight = DefaultLineHeight;

    // Precision scroll timer & accumulators
    QTimer* m_scrollTimer = nullptr;
    QElapsedTimer m_elapsedTimer;
    double m_subPixelAccumulator = 0.0;
    double m_lineAccumulator = 0.0;
    double m_pageSecAccumulator = 0.0;

    // Interaction preferences
    bool m_tapToPauseEnabled = true;
    bool m_tapToResumeEnabled = true;
    bool m_shortcutsEnabled = true;
    bool m_startOnShow = false;

    // Overlay dragging state
    bool m_isDragging = false;
    bool m_hasCustomPosition = false;
    QPoint m_dragClickOffset;

    // Theme tokens
    ReaderThemeTokens m_tokens;
};

} // namespace LunaRead
