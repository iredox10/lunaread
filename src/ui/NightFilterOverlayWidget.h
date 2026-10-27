#pragma once

#include <QWidget>
#include <QColor>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

namespace LunaRead {

/**
 * @brief Eye-care Night Dimmer and Blue Light Filter Overlay for LunaRead-Qt.
 *
 * Implements Moon+ Reader's iconic blue light reduction and extra-low brightness filter:
 * - Color temperature warming (reduces harsh blue spectrum with soothing amber tint)
 * - Software brightness dimming (dims display below OS hardware minimums for pitch-black reading)
 * - Draggable mini-HUD control panel with quick reset and toggle buttons.
 */
class NightFilterOverlayWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness)
    Q_PROPERTY(int warmness READ warmness WRITE setWarmness)

public:
    explicit NightFilterOverlayWidget(QWidget* parent = nullptr);
    ~NightFilterOverlayWidget() override = default;

    [[nodiscard]] bool isFilterEnabled() const noexcept { return m_enabled; }
    void setFilterEnabled(bool enabled);
    void toggleFilter();

    [[nodiscard]] int brightness() const noexcept { return m_brightness; }
    void setBrightness(int percentage); // 10 to 100

    [[nodiscard]] int warmness() const noexcept { return m_warmness; }
    void setWarmness(int percentage); // 0 to 100

    void showControls();
    void hideControls();
    void toggleControls();

signals:
    void filterChanged(bool enabled, int brightness, int warmness);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void setupHud();
    void updateHudPosition();

    bool m_enabled = false;
    int m_brightness = 100; // 100% = normal, 20% = very dim
    int m_warmness = 35;     // 35% default warm amber tint

    // Draggable Control Card
    QFrame* m_hudCard = nullptr;
    QSlider* m_brightnessSlider = nullptr;
    QLabel* m_brightnessValLabel = nullptr;
    QSlider* m_warmnessSlider = nullptr;
    QLabel* m_warmnessValLabel = nullptr;
    QPushButton* m_toggleBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    bool m_draggingHud = false;
    QPoint m_dragStartPos;
};

} // namespace LunaRead
