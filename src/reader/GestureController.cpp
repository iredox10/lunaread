#include "GestureController.h"
#include <algorithm>

namespace LunaRead {

GestureController::GestureController(QObject* parent) : QObject(parent) {
    // Standard Moon+ Reader 9-zone default configuration:
    m_zones[0][0] = TapAction::OpenToc;           // Top-Left: Table of Contents drawer
    m_zones[0][1] = TapAction::OpenMenu;          // Top-Center: Toolbar HUD
    m_zones[0][2] = TapAction::ToggleTheme;       // Top-Right: Quick Theme switch
    m_zones[1][0] = TapAction::PrevPage;          // Middle-Left: Previous Page
    m_zones[1][1] = TapAction::OpenMenu;          // Center: Toolbar HUD
    m_zones[1][2] = TapAction::NextPage;          // Middle-Right: Next Page
    m_zones[2][0] = TapAction::ToggleBookmark;    // Bottom-Left: Bookmark
    m_zones[2][1] = TapAction::ToggleTts;         // Bottom-Center: Floating TTS Control Bar HUD
    m_zones[2][2] = TapAction::NextPage;          // Bottom-Right: Next Page
}

void GestureController::setZoneAction(int row, int col, TapAction action) {
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
        m_zones[row][col] = action;
    }
}

TapAction GestureController::zoneAction(int row, int col) const {
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
        return m_zones[row][col];
    }
    return TapAction::None;
}

TapAction GestureController::resolveTap(const QPointF& pos, const QSizeF& size) {
    if (size.width() <= 0 || size.height() <= 0) return TapAction::None;

    qreal normX = pos.x() / size.width();
    qreal normY = pos.y() / size.height();

    // 3x3 Grid: Top 20% Y, Bottom 20% Y, Middle 60% Y
    // Left 30% X, Center 40% X, Right 30% X
    int row = (normY < 0.20) ? 0 : ((normY > 0.80) ? 2 : 1);
    int col = (normX < 0.30) ? 0 : ((normX > 0.70) ? 2 : 1);

    TapAction action = m_zones[row][col];
    emit actionTriggered(action);
    return action;
}

bool GestureController::handleMouseMove(const QPointF& pos, const QSizeF& size, qreal deltaY) {
    if (size.width() <= 0) return false;
    qreal normX = pos.x() / size.width();

    if (normX <= 0.05) {
        // Left edge: brightness drag
        m_brightness = std::clamp(m_brightness - (deltaY / 200.0), 0.1, 1.0);
        emit brightnessChanged(m_brightness);
        return true;
    } else if (normX >= 0.95) {
        // Right edge: font size scale
        emit fontSizeScaleRequested(-deltaY / 50.0);
        return true;
    }
    return false;
}

} // namespace LunaRead
