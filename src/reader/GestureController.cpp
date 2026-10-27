#include "GestureController.h"
#include <algorithm>
#include <cmath>
#include <QSettings>

namespace LunaRead {

GestureController::GestureController(QObject* parent) : QObject(parent) {
    loadConfig();
}

std::array<TapZoneAction, 9> GestureController::presetMoonDefault() {
    return {
        TapZoneAction::OpenToc,        // Zone 0: Top-Left
        TapZoneAction::ToggleMenu,     // Zone 1: Top-Center
        TapZoneAction::ToggleBookmark, // Zone 2: Top-Right
        TapZoneAction::PrevPage,       // Zone 3: Mid-Left
        TapZoneAction::ToggleMenu,     // Zone 4: Mid-Center
        TapZoneAction::NextPage,       // Zone 5: Mid-Right
        TapZoneAction::OpenQuickNav,   // Zone 6: Bot-Left
        TapZoneAction::ToggleTts,      // Zone 7: Bot-Center
        TapZoneAction::NextPage        // Zone 8: Bot-Right
    };
}

std::array<TapZoneAction, 9> GestureController::presetLeftHanded() {
    return {
        TapZoneAction::ToggleBookmark, // Zone 0: Top-Left
        TapZoneAction::ToggleMenu,     // Zone 1: Top-Center
        TapZoneAction::OpenToc,        // Zone 2: Top-Right
        TapZoneAction::NextPage,       // Zone 3: Mid-Left (Easy reach for left thumb)
        TapZoneAction::ToggleMenu,     // Zone 4: Mid-Center
        TapZoneAction::PrevPage,       // Zone 5: Mid-Right
        TapZoneAction::NextPage,       // Zone 6: Bot-Left (Easy reach for left thumb)
        TapZoneAction::ToggleTts,      // Zone 7: Bot-Center
        TapZoneAction::PrevPage        // Zone 8: Bot-Right
    };
}

std::array<TapZoneAction, 9> GestureController::presetMinimal() {
    return {
        TapZoneAction::PrevPage,       // Zone 0: Top-Left
        TapZoneAction::ToggleMenu,     // Zone 1: Top-Center
        TapZoneAction::NextPage,       // Zone 2: Top-Right
        TapZoneAction::PrevPage,       // Zone 3: Mid-Left
        TapZoneAction::ToggleMenu,     // Zone 4: Mid-Center
        TapZoneAction::NextPage,       // Zone 5: Mid-Right
        TapZoneAction::PrevPage,       // Zone 6: Bot-Left
        TapZoneAction::ToggleMenu,     // Zone 7: Bot-Center
        TapZoneAction::NextPage        // Zone 8: Bot-Right
    };
}

void GestureController::applyPresetMoonDefault() {
    m_zones = presetMoonDefault();
    emit configChanged();
}

void GestureController::applyPresetLeftHanded() {
    m_zones = presetLeftHanded();
    emit configChanged();
}

void GestureController::applyPresetMinimal() {
    m_zones = presetMinimal();
    emit configChanged();
}

void GestureController::resetToDefault() {
    applyPresetMoonDefault();
}

void GestureController::loadConfig() {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    const auto defaults = presetMoonDefault();

    for (int i = 0; i < 9; ++i) {
        const QString key = QStringLiteral("tap_zones/zone_%1").arg(i);
        if (settings.contains(key)) {
            QVariant val = settings.value(key);
            if (val.typeId() == QMetaType::Int || val.typeId() == QMetaType::LongLong) {
                int intVal = val.toInt();
                if (intVal >= 0 && intVal <= static_cast<int>(TapZoneAction::ToggleFullscreen)) {
                    m_zones[i] = static_cast<TapZoneAction>(intVal);
                } else {
                    m_zones[i] = defaults[i];
                }
            } else {
                m_zones[i] = stringToAction(val.toString());
            }
        } else {
            m_zones[i] = defaults[i];
        }
    }
}

void GestureController::saveConfig() const {
    QSettings settings(QStringLiteral("LunaRead"), QStringLiteral("LunaRead"));
    for (int i = 0; i < 9; ++i) {
        const QString key = QStringLiteral("tap_zones/zone_%1").arg(i);
        settings.setValue(key, actionToString(m_zones[i]));
    }
    settings.sync();
}

void GestureController::setZoneAction(int zoneIndex, TapZoneAction action) {
    if (zoneIndex >= 0 && zoneIndex < 9) {
        if (m_zones[zoneIndex] != action) {
            m_zones[zoneIndex] = action;
            emit configChanged();
        }
    }
}

TapZoneAction GestureController::zoneAction(int zoneIndex) const {
    if (zoneIndex >= 0 && zoneIndex < 9) {
        return m_zones[zoneIndex];
    }
    return TapZoneAction::None;
}

void GestureController::setZoneAction(int row, int col, TapZoneAction action) {
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
        setZoneAction(row * 3 + col, action);
    }
}

TapZoneAction GestureController::tapZoneAction(int row, int col) const {
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
        return zoneAction(row * 3 + col);
    }
    return TapZoneAction::None;
}

void GestureController::setZoneAction(int row, int col, TapAction action) {
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
        setZoneAction(row * 3 + col, fromLegacyTapAction(action));
    }
}

TapAction GestureController::zoneAction(int row, int col) const {
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
        return toLegacyTapAction(m_zones[row * 3 + col]);
    }
    return TapAction::None;
}

TapZoneAction GestureController::getActionForPoint(const QPointF& pos, const QSize& viewportSize) const {
    if (viewportSize.width() <= 0 || viewportSize.height() <= 0) {
        return TapZoneAction::None;
    }
    if (std::isnan(pos.x()) || std::isnan(pos.y())) {
        return TapZoneAction::None;
    }

    qreal normX = pos.x() / static_cast<qreal>(viewportSize.width());
    qreal normY = pos.y() / static_cast<qreal>(viewportSize.height());

    int col = static_cast<int>(normX * 3.0);
    int row = static_cast<int>(normY * 3.0);

    col = std::clamp(col, 0, 2);
    row = std::clamp(row, 0, 2);

    int index = row * 3 + col;
    if (index >= 0 && index < 9) {
        return m_zones[index];
    }
    return TapZoneAction::None;
}

TapZoneAction GestureController::getActionForPoint(const QPointF& pos, const QSizeF& viewportSize) const {
    if (viewportSize.width() <= 0.0 || viewportSize.height() <= 0.0) {
        return TapZoneAction::None;
    }
    if (std::isnan(pos.x()) || std::isnan(pos.y())) {
        return TapZoneAction::None;
    }

    qreal normX = pos.x() / viewportSize.width();
    qreal normY = pos.y() / viewportSize.height();

    int col = static_cast<int>(normX * 3.0);
    int row = static_cast<int>(normY * 3.0);

    col = std::clamp(col, 0, 2);
    row = std::clamp(row, 0, 2);

    int index = row * 3 + col;
    if (index >= 0 && index < 9) {
        return m_zones[index];
    }
    return TapZoneAction::None;
}

TapAction GestureController::resolveTap(const QPointF& pos, const QSizeF& viewportSize) {
    TapZoneAction action = getActionForPoint(pos, viewportSize);
    TapAction legacy = toLegacyTapAction(action);
    emit zoneActionTriggered(action);
    emit actionTriggered(legacy);
    return legacy;
}

bool GestureController::handleMouseMove(const QPointF& pos, const QSizeF& size, qreal deltaY) {
    if (size.width() <= 0.0) return false;
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

QString GestureController::actionToString(TapZoneAction action) {
    switch (action) {
        case TapZoneAction::NextPage: return QStringLiteral("NextPage");
        case TapZoneAction::PrevPage: return QStringLiteral("PrevPage");
        case TapZoneAction::ToggleMenu: return QStringLiteral("ToggleMenu");
        case TapZoneAction::ToggleBookmark: return QStringLiteral("ToggleBookmark");
        case TapZoneAction::ToggleNightFilter: return QStringLiteral("ToggleNightFilter");
        case TapZoneAction::ToggleTts: return QStringLiteral("ToggleTts");
        case TapZoneAction::ToggleAutoScroll: return QStringLiteral("ToggleAutoScroll");
        case TapZoneAction::OpenToc: return QStringLiteral("OpenToc");
        case TapZoneAction::OpenQuickNav: return QStringLiteral("OpenQuickNav");
        case TapZoneAction::ToggleFullscreen: return QStringLiteral("ToggleFullscreen");
        case TapZoneAction::None:
        default:
            return QStringLiteral("None");
    }
}

TapZoneAction GestureController::stringToAction(const QString& str) {
    const QString s = str.trimmed();
    bool ok = false;
    int intVal = s.toInt(&ok);
    if (ok && intVal >= 0 && intVal <= static_cast<int>(TapZoneAction::ToggleFullscreen)) {
        return static_cast<TapZoneAction>(intVal);
    }

    const QString lower = s.toLower();
    if (lower == QStringLiteral("nextpage") || lower == QStringLiteral("next_page") || lower == QStringLiteral("next")) {
        return TapZoneAction::NextPage;
    }
    if (lower == QStringLiteral("prevpage") || lower == QStringLiteral("previouspage") || lower == QStringLiteral("prev_page") || lower == QStringLiteral("prev")) {
        return TapZoneAction::PrevPage;
    }
    if (lower == QStringLiteral("togglemenu") || lower == QStringLiteral("openmenu") || lower == QStringLiteral("toggle_menu") || lower == QStringLiteral("menu")) {
        return TapZoneAction::ToggleMenu;
    }
    if (lower == QStringLiteral("togglebookmark") || lower == QStringLiteral("toggle_bookmark") || lower == QStringLiteral("bookmark")) {
        return TapZoneAction::ToggleBookmark;
    }
    if (lower == QStringLiteral("togglenightfilter") || lower == QStringLiteral("toggle_night_filter") || lower == QStringLiteral("nightfilter") || lower == QStringLiteral("night_filter") || lower == QStringLiteral("night")) {
        return TapZoneAction::ToggleNightFilter;
    }
    if (lower == QStringLiteral("toggletts") || lower == QStringLiteral("toggle_tts") || lower == QStringLiteral("tts")) {
        return TapZoneAction::ToggleTts;
    }
    if (lower == QStringLiteral("toggleautoscroll") || lower == QStringLiteral("toggle_auto_scroll") || lower == QStringLiteral("autoscroll") || lower == QStringLiteral("auto_scroll")) {
        return TapZoneAction::ToggleAutoScroll;
    }
    if (lower == QStringLiteral("opentoc") || lower == QStringLiteral("open_toc") || lower == QStringLiteral("toc")) {
        return TapZoneAction::OpenToc;
    }
    if (lower == QStringLiteral("openquicknav") || lower == QStringLiteral("open_quick_nav") || lower == QStringLiteral("quicknav") || lower == QStringLiteral("quick_nav")) {
        return TapZoneAction::OpenQuickNav;
    }
    if (lower == QStringLiteral("togglefullscreen") || lower == QStringLiteral("toggle_fullscreen") || lower == QStringLiteral("fullscreen")) {
        return TapZoneAction::ToggleFullscreen;
    }
    if (lower == QStringLiteral("none")) {
        return TapZoneAction::None;
    }

    return TapZoneAction::None;
}

QString GestureController::actionDisplayName(TapZoneAction action) {
    switch (action) {
        case TapZoneAction::NextPage: return tr("Next Page");
        case TapZoneAction::PrevPage: return tr("Previous Page");
        case TapZoneAction::ToggleMenu: return tr("Toggle Menu");
        case TapZoneAction::ToggleBookmark: return tr("Toggle Bookmark");
        case TapZoneAction::ToggleNightFilter: return tr("Night Filter");
        case TapZoneAction::ToggleTts: return tr("Text-to-Speech");
        case TapZoneAction::ToggleAutoScroll: return tr("Auto-Scroll");
        case TapZoneAction::OpenToc: return tr("Table of Contents");
        case TapZoneAction::OpenQuickNav: return tr("Quick Nav");
        case TapZoneAction::ToggleFullscreen: return tr("Toggle Fullscreen");
        case TapZoneAction::None:
        default:
            return tr("None");
    }
}

QString GestureController::actionDescription(TapZoneAction action) {
    switch (action) {
        case TapZoneAction::NextPage: return tr("Turn to next page");
        case TapZoneAction::PrevPage: return tr("Turn to previous page");
        case TapZoneAction::ToggleMenu: return tr("Show/Hide header/footer menu");
        case TapZoneAction::ToggleBookmark: return tr("Add/Remove dog-ear bookmark");
        case TapZoneAction::ToggleNightFilter: return tr("Toggle blue light / night dimmer");
        case TapZoneAction::ToggleTts: return tr("Start/Stop TTS");
        case TapZoneAction::ToggleAutoScroll: return tr("Start/Stop hands-free autoscroll");
        case TapZoneAction::OpenToc: return tr("Open table of contents");
        case TapZoneAction::OpenQuickNav: return tr("Open quick navigation seek bar");
        case TapZoneAction::ToggleFullscreen: return tr("Toggle full screen");
        case TapZoneAction::None:
        default:
            return tr("Do nothing");
    }
}

QString GestureController::actionIcon(TapZoneAction action) {
    switch (action) {
        case TapZoneAction::NextPage: return QStringLiteral("⏭️");
        case TapZoneAction::PrevPage: return QStringLiteral("⏮️");
        case TapZoneAction::ToggleMenu: return QStringLiteral("☰");
        case TapZoneAction::ToggleBookmark: return QStringLiteral("🔖");
        case TapZoneAction::ToggleNightFilter: return QStringLiteral("🌙");
        case TapZoneAction::ToggleTts: return QStringLiteral("🔊");
        case TapZoneAction::ToggleAutoScroll: return QStringLiteral("📜");
        case TapZoneAction::OpenToc: return QStringLiteral("📑");
        case TapZoneAction::OpenQuickNav: return QStringLiteral("🧭");
        case TapZoneAction::ToggleFullscreen: return QStringLiteral("⛶");
        case TapZoneAction::None:
        default:
            return QStringLiteral("🚫");
    }
}

QString GestureController::zoneName(int zoneIndex) {
    switch (zoneIndex) {
        case 0: return tr("Top-Left");
        case 1: return tr("Top-Center");
        case 2: return tr("Top-Right");
        case 3: return tr("Mid-Left");
        case 4: return tr("Mid-Center");
        case 5: return tr("Mid-Right");
        case 6: return tr("Bot-Left");
        case 7: return tr("Bot-Center");
        case 8: return tr("Bot-Right");
        default: return tr("Zone %1").arg(zoneIndex);
    }
}

QString GestureController::zoneShortName(int zoneIndex) {
    switch (zoneIndex) {
        case 0: return QStringLiteral("TL");
        case 1: return QStringLiteral("TC");
        case 2: return QStringLiteral("TR");
        case 3: return QStringLiteral("ML");
        case 4: return QStringLiteral("MC");
        case 5: return QStringLiteral("MR");
        case 6: return QStringLiteral("BL");
        case 7: return QStringLiteral("BC");
        case 8: return QStringLiteral("BR");
        default: return QStringLiteral("Z%1").arg(zoneIndex);
    }
}

QString GestureController::zoneDirectionSymbol(int zoneIndex) {
    switch (zoneIndex) {
        case 0: return QStringLiteral("↖");
        case 1: return QStringLiteral("↑");
        case 2: return QStringLiteral("↗");
        case 3: return QStringLiteral("←");
        case 4: return QStringLiteral("⊙");
        case 5: return QStringLiteral("→");
        case 6: return QStringLiteral("↙");
        case 7: return QStringLiteral("↓");
        case 8: return QStringLiteral("↘");
        default: return QStringLiteral("•");
    }
}

TapAction GestureController::toLegacyTapAction(TapZoneAction action) {
    switch (action) {
        case TapZoneAction::NextPage: return TapAction::NextPage;
        case TapZoneAction::PrevPage: return TapAction::PrevPage;
        case TapZoneAction::ToggleMenu: return TapAction::OpenMenu;
        case TapZoneAction::ToggleBookmark: return TapAction::ToggleBookmark;
        case TapZoneAction::ToggleNightFilter: return TapAction::ToggleTheme;
        case TapZoneAction::ToggleTts: return TapAction::ToggleTts;
        case TapZoneAction::OpenToc: return TapAction::OpenToc;
        case TapZoneAction::ToggleAutoScroll:
        case TapZoneAction::OpenQuickNav:
        case TapZoneAction::ToggleFullscreen:
        case TapZoneAction::None:
        default:
            return TapAction::None;
    }
}

TapZoneAction GestureController::fromLegacyTapAction(TapAction action) {
    switch (action) {
        case TapAction::NextPage: return TapZoneAction::NextPage;
        case TapAction::PrevPage: return TapZoneAction::PrevPage;
        case TapAction::OpenMenu: return TapZoneAction::ToggleMenu;
        case TapAction::ToggleBookmark: return TapZoneAction::ToggleBookmark;
        case TapAction::ToggleTheme: return TapZoneAction::ToggleNightFilter;
        case TapAction::ToggleTts: return TapZoneAction::ToggleTts;
        case TapAction::OpenToc: return TapZoneAction::OpenToc;
        case TapAction::ShowStats:
        case TapAction::OpenFontSettings:
        case TapAction::OpenSearch:
        case TapAction::None:
        default:
            return TapZoneAction::None;
    }
}

} // namespace LunaRead
