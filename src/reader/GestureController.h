#pragma once

#include <QObject>
#include <QPointF>
#include <QSizeF>
#include "../core/Types.h"

namespace LunaRead {

class GestureController : public QObject {
    Q_OBJECT
public:
    explicit GestureController(QObject* parent = nullptr);

    TapAction resolveTap(const QPointF& pos, const QSizeF& viewportSize);
    bool handleMouseMove(const QPointF& pos, const QSizeF& viewportSize, qreal deltaY);

    void setZoneAction(int row, int col, TapAction action);
    [[nodiscard]] TapAction zoneAction(int row, int col) const;

signals:
    void actionTriggered(TapAction action);
    void brightnessChanged(qreal brightness);
    void fontSizeScaleRequested(qreal delta);

private:
    qreal m_brightness = 1.0;
    TapAction m_zones[3][3];
};

} // namespace LunaRead
