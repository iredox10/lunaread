#pragma once

#include <QObject>
#include <QPointF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <array>
#include "../core/Types.h"

namespace LunaRead {

/**
 * @brief Actions mapped to the 9-zone tap/click navigation matrix.
 */
enum class TapZoneAction {
    None,
    NextPage,
    PrevPage,
    ToggleMenu,
    ToggleBookmark,
    ToggleNightFilter,
    ToggleTts,
    ToggleAutoScroll,
    OpenToc,
    OpenQuickNav,
    ToggleFullscreen
};

/**
 * @brief Controller for handling 9-zone tap gestures and mouse drags in the viewport.
 */
class GestureController : public QObject {
    Q_OBJECT

public:
    explicit GestureController(QObject* parent = nullptr);
    ~GestureController() override = default;

    // Feature 5 core methods
    [[nodiscard]] TapZoneAction getActionForPoint(const QPointF& pos, const QSize& viewportSize) const;
    [[nodiscard]] TapZoneAction getActionForPoint(const QPointF& pos, const QSizeF& viewportSize) const;

    void loadConfig();
    void saveConfig() const;

    void setZoneAction(int zoneIndex, TapZoneAction action);
    [[nodiscard]] TapZoneAction zoneAction(int zoneIndex) const;
    [[nodiscard]] TapZoneAction getZoneAction(int zoneIndex) const { return zoneAction(zoneIndex); }

    void setZoneAction(int row, int col, TapZoneAction action);
    [[nodiscard]] TapZoneAction tapZoneAction(int row, int col) const;

    // Legacy TapAction support & backward compatibility
    void setZoneAction(int row, int col, TapAction action);
    [[nodiscard]] TapAction zoneAction(int row, int col) const;
    TapAction resolveTap(const QPointF& pos, const QSizeF& viewportSize);
    bool handleMouseMove(const QPointF& pos, const QSizeF& viewportSize, qreal deltaY);

    [[nodiscard]] qreal brightness() const noexcept { return m_brightness; }
    void setBrightness(qreal brightness) noexcept { m_brightness = brightness; }

    // Presets
    static std::array<TapZoneAction, 9> presetMoonDefault();
    static std::array<TapZoneAction, 9> presetLeftHanded();
    static std::array<TapZoneAction, 9> presetMinimal();

    void applyPresetMoonDefault();
    void applyPresetLeftHanded();
    void applyPresetMinimal();
    void resetToDefault();

    // Static metadata & string helpers
    static QString actionToString(TapZoneAction action);
    static TapZoneAction stringToAction(const QString& str);
    static QString actionDisplayName(TapZoneAction action);
    static QString actionDescription(TapZoneAction action);
    static QString actionIcon(TapZoneAction action);
    static QString zoneName(int zoneIndex);
    static QString zoneShortName(int zoneIndex);
    static QString zoneDirectionSymbol(int zoneIndex);

    static TapAction toLegacyTapAction(TapZoneAction action);
    static TapZoneAction fromLegacyTapAction(TapAction action);

signals:
    void zoneActionTriggered(TapZoneAction action);
    void actionTriggered(TapAction action);
    void brightnessChanged(qreal brightness);
    void fontSizeScaleRequested(qreal delta);
    void configChanged();

private:
    qreal m_brightness = 1.0;
    std::array<TapZoneAction, 9> m_zones{};
};

} // namespace LunaRead
