#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <array>
#include "../reader/GestureController.h"

namespace LunaRead {

/**
 * @brief Interactive configuration dialog for Moon+ Reader 9-zone touch/click navigation.
 *
 * Displays a 3x3 visual screen layout where each tile can be mapped to any reader action
 * (Next Page, Prev Page, Toggle Menu, Bookmarks, Night Filter, Quick Nav, TTS, etc.)
 * with instant presets for default, left-handed, and minimal reading.
 */
class TapZoneConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit TapZoneConfigDialog(GestureController* controller, QWidget* parent = nullptr);
    ~TapZoneConfigDialog() override = default;

private slots:
    void applyPresetDefault();
    void applyPresetLeftHanded();
    void applyPresetMinimal();
    void saveAndAccept();

private:
    void setupUi();
    void updateGridUi();

    GestureController* m_controller = nullptr;
    std::array<TapZoneAction, 9> m_tempZones{};
    std::array<QComboBox*, 9> m_zoneCombos{};
    QComboBox* m_wheelCombo = nullptr;
    QComboBox* m_rightClickCombo = nullptr;
    QComboBox* m_middleClickCombo = nullptr;
    MouseWheelAction m_tempWheelAction = MouseWheelAction::TurnPage;
    MouseButtonAction m_tempRightClickAction = MouseButtonAction::DefaultMenu;
    MouseButtonAction m_tempMiddleClickAction = MouseButtonAction::ToggleAutoScroll;
};

} // namespace LunaRead
