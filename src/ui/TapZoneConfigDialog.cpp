#include "TapZoneConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>

namespace LunaRead {

TapZoneConfigDialog::TapZoneConfigDialog(GestureController* controller, QWidget* parent)
    : QDialog(parent), m_controller(controller) {
    setWindowTitle(tr("9-Zone Touch & Click Navigation Setup"));
    resize(740, 580);

    if (m_controller) {
        for (int i = 0; i < 9; ++i) {
            m_tempZones[i] = m_controller->zoneAction(i);
        }
        m_tempWheelAction = m_controller->mouseWheelAction();
        m_tempRightClickAction = m_controller->rightClickAction();
        m_tempMiddleClickAction = m_controller->middleClickAction();
    } else {
        m_tempZones = GestureController::presetMoonDefault();
    }

    setupUi();
}

void TapZoneConfigDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    auto* headerLabel = new QLabel(
        tr("<b>Customize screen click & tap navigation zones</b><br>"
           "<span style='color: #64748B; font-size: 9.5pt;'>Configure what action happens when you click or tap in each area of the reading screen.</span>"), this);
    mainLayout->addWidget(headerLabel);

    // 3x3 Visual Grid
    auto* gridFrame = new QFrame(this);
    gridFrame->setStyleSheet(
        "QFrame {"
        "  background: #0F172A;"
        "  border: 1px solid #334155;"
        "  border-radius: 12px;"
        "}"
    );
    auto* gridLayout = new QGridLayout(gridFrame);
    gridLayout->setSpacing(10);
    gridLayout->setContentsMargins(14, 14, 14, 14);

    const QStringList actionNames = {
        tr("🚫 None (Do Nothing)"),
        tr("📖 Next Page"),
        tr("⏮ Previous Page"),
        tr("☰ Toggle Menu / Toolbars"),
        tr("🔖 Toggle Bookmark"),
        tr("🌙 Toggle Night Mode / Dimmer"),
        tr("🗣️ Toggle Spoken TTS"),
        tr("📜 Toggle Hands-Free Auto-Scroll"),
        tr("📑 Open Table of Contents"),
        tr("⏱️ Open Quick Seek & Nav Bar"),
        tr("⛶ Toggle Full Screen")
    };

    const std::array<TapZoneAction, 11> actionValues = {
        TapZoneAction::None,
        TapZoneAction::NextPage,
        TapZoneAction::PrevPage,
        TapZoneAction::ToggleMenu,
        TapZoneAction::ToggleBookmark,
        TapZoneAction::ToggleNightFilter,
        TapZoneAction::ToggleTts,
        TapZoneAction::ToggleAutoScroll,
        TapZoneAction::OpenToc,
        TapZoneAction::OpenQuickNav,
        TapZoneAction::ToggleFullscreen
    };

    for (int i = 0; i < 9; ++i) {
        int row = i / 3;
        int col = i % 3;

        auto* tile = new QFrame(gridFrame);
        tile->setStyleSheet(
            "QFrame {"
            "  background: #1E293B;"
            "  border: 1px solid #475569;"
            "  border-radius: 8px;"
            "}"
        );
        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(8, 8, 8, 8);
        tileLayout->setSpacing(4);

        auto* nameLabel = new QLabel(GestureController::zoneName(i), tile);
        nameLabel->setStyleSheet("color: #94A3B8; font-size: 8.5pt; font-weight: bold; border: none; background: transparent;");
        tileLayout->addWidget(nameLabel);

        auto* combo = new QComboBox(tile);
        combo->setStyleSheet(
            "QComboBox {"
            "  background: #0F172A;"
            "  color: #F8FAFC;"
            "  border: 1px solid #64748B;"
            "  border-radius: 6px;"
            "  padding: 4px;"
            "  font-size: 9pt;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background: #0F172A;"
            "  color: #F8FAFC;"
            "  selection-background-color: #2563EB;"
            "}"
        );

        for (int a = 0; a < actionNames.size(); ++a) {
            combo->addItem(actionNames[a], static_cast<int>(actionValues[a]));
        }

        // Set active index
        int activeIdx = 0;
        for (int a = 0; a < actionValues.size(); ++a) {
            if (actionValues[a] == m_tempZones[i]) {
                activeIdx = a;
                break;
            }
        }
        combo->setCurrentIndex(activeIdx);

        connect(combo, &QComboBox::currentIndexChanged, this, [this, i, combo, actionValues]() {
            int selectedIdx = combo->currentIndex();
            if (selectedIdx >= 0 && selectedIdx < static_cast<int>(actionValues.size())) {
                m_tempZones[i] = actionValues[selectedIdx];
            }
        });

        tileLayout->addWidget(combo);
        m_zoneCombos[i] = combo;

        gridLayout->addWidget(tile, row, col);
    }

    mainLayout->addWidget(gridFrame, 1);

    // Presets Bar
    auto* presetGroup = new QHBoxLayout();
    presetGroup->setSpacing(8);

    presetGroup->addWidget(new QLabel(tr("<b>Presets:</b>"), this));

    auto* defaultBtn = new QPushButton(tr("Moon+ Reader Default"), this);
    connect(defaultBtn, &QPushButton::clicked, this, &TapZoneConfigDialog::applyPresetDefault);
    presetGroup->addWidget(defaultBtn);

    auto* leftBtn = new QPushButton(tr("Left-Handed Mode"), this);
    connect(leftBtn, &QPushButton::clicked, this, &TapZoneConfigDialog::applyPresetLeftHanded);
    presetGroup->addWidget(leftBtn);

    auto* minimalBtn = new QPushButton(tr("Minimal (Sides Only)"), this);
    connect(minimalBtn, &QPushButton::clicked, this, &TapZoneConfigDialog::applyPresetMinimal);
    presetGroup->addWidget(minimalBtn);

    presetGroup->addStretch();
    mainLayout->addLayout(presetGroup);

    // Mouse Hardware Actions Bar
    auto* mouseFrame = new QFrame(this);
    mouseFrame->setStyleSheet(
        "QFrame {"
        "  background: #0F172A;"
        "  border: 1px solid #334155;"
        "  border-radius: 8px;"
        "  padding: 6px;"
        "}"
    );
    auto* mouseLayout = new QHBoxLayout(mouseFrame);
    mouseLayout->setSpacing(12);

    // Mouse Wheel
    auto* wheelLayout = new QVBoxLayout();
    wheelLayout->addWidget(new QLabel(tr("<b>🖱️ Mouse Wheel Scroll:</b>"), mouseFrame));
    m_wheelCombo = new QComboBox(mouseFrame);
    m_wheelCombo->addItem(tr("📖 Turn Page (Next / Prev)"), static_cast<int>(MouseWheelAction::TurnPage));
    m_wheelCombo->addItem(tr("Aa Adjust Font Size (+/- 1pt)"), static_cast<int>(MouseWheelAction::AdjustFontSize));
    m_wheelCombo->addItem(tr("🌙 Adjust Night Dimmer"), static_cast<int>(MouseWheelAction::AdjustNightFilterBrightness));
    m_wheelCombo->addItem(tr("⏳ Toggle Auto-Scroll"), static_cast<int>(MouseWheelAction::ToggleAutoScroll));
    for (int idx = 0; idx < m_wheelCombo->count(); ++idx) {
        if (m_wheelCombo->itemData(idx).toInt() == static_cast<int>(m_tempWheelAction)) {
            m_wheelCombo->setCurrentIndex(idx);
            break;
        }
    }
    wheelLayout->addWidget(m_wheelCombo);
    mouseLayout->addLayout(wheelLayout);

    // Right Click
    auto* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(new QLabel(tr("<b>🖱️ Right Click:</b>"), mouseFrame));
    m_rightClickCombo = new QComboBox(mouseFrame);
    m_rightClickCombo->addItem(tr("☰ Toggle Menu / Toolbars"), static_cast<int>(MouseButtonAction::DefaultMenu));
    m_rightClickCombo->addItem(tr("⏮ Previous Page"), static_cast<int>(MouseButtonAction::PreviousPage));
    m_rightClickCombo->addItem(tr("📖 Next Page"), static_cast<int>(MouseButtonAction::NextPage));
    m_rightClickCombo->addItem(tr("🖥️ Toggle Fullscreen"), static_cast<int>(MouseButtonAction::ToggleFullscreen));
    m_rightClickCombo->addItem(tr("🔖 Toggle Bookmark"), static_cast<int>(MouseButtonAction::ToggleBookmark));
    m_rightClickCombo->addItem(tr("⏳ Toggle Auto-Scroll"), static_cast<int>(MouseButtonAction::ToggleAutoScroll));
    for (int idx = 0; idx < m_rightClickCombo->count(); ++idx) {
        if (m_rightClickCombo->itemData(idx).toInt() == static_cast<int>(m_tempRightClickAction)) {
            m_rightClickCombo->setCurrentIndex(idx);
            break;
        }
    }
    rightLayout->addWidget(m_rightClickCombo);
    mouseLayout->addLayout(rightLayout);

    // Middle Click
    auto* midLayout = new QVBoxLayout();
    midLayout->addWidget(new QLabel(tr("<b>🖱️ Middle Click:</b>"), mouseFrame));
    m_middleClickCombo = new QComboBox(mouseFrame);
    m_middleClickCombo->addItem(tr("⏳ Toggle Auto-Scroll"), static_cast<int>(MouseButtonAction::ToggleAutoScroll));
    m_middleClickCombo->addItem(tr("🧭 Open QuickNav Popover"), static_cast<int>(MouseButtonAction::OpenQuickNav));
    m_middleClickCombo->addItem(tr("🔖 Toggle Bookmark"), static_cast<int>(MouseButtonAction::ToggleBookmark));
    m_middleClickCombo->addItem(tr("🖥️ Toggle Fullscreen"), static_cast<int>(MouseButtonAction::ToggleFullscreen));
    for (int idx = 0; idx < m_middleClickCombo->count(); ++idx) {
        if (m_middleClickCombo->itemData(idx).toInt() == static_cast<int>(m_tempMiddleClickAction)) {
            m_middleClickCombo->setCurrentIndex(idx);
            break;
        }
    }
    midLayout->addWidget(m_middleClickCombo);
    mouseLayout->addLayout(midLayout);

    mainLayout->addWidget(mouseFrame);

    // Bottom Action Buttons
    auto* botLayout = new QHBoxLayout();
    botLayout->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    botLayout->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton(tr("Save & Apply"), this);
    saveBtn->setStyleSheet("background-color: #2563EB; color: white; font-weight: bold; padding: 8px 18px; border-radius: 6px;");
    connect(saveBtn, &QPushButton::clicked, this, &TapZoneConfigDialog::saveAndAccept);
    botLayout->addWidget(saveBtn);

    mainLayout->addLayout(botLayout);
}

void TapZoneConfigDialog::updateGridUi() {
    for (int i = 0; i < 9; ++i) {
        if (!m_zoneCombos[i]) continue;
        for (int a = 0; a < m_zoneCombos[i]->count(); ++a) {
            if (m_zoneCombos[i]->itemData(a).toInt() == static_cast<int>(m_tempZones[i])) {
                m_zoneCombos[i]->setCurrentIndex(a);
                break;
            }
        }
    }
}

void TapZoneConfigDialog::applyPresetDefault() {
    m_tempZones = GestureController::presetMoonDefault();
    updateGridUi();
}

void TapZoneConfigDialog::applyPresetLeftHanded() {
    m_tempZones = GestureController::presetLeftHanded();
    updateGridUi();
}

void TapZoneConfigDialog::applyPresetMinimal() {
    m_tempZones = GestureController::presetMinimal();
    updateGridUi();
}

void TapZoneConfigDialog::saveAndAccept() {
    if (m_controller) {
        for (int i = 0; i < 9; ++i) {
            m_controller->setZoneAction(i, m_tempZones[i]);
        }
        if (m_wheelCombo) {
            m_controller->setMouseWheelAction(static_cast<MouseWheelAction>(m_wheelCombo->currentData().toInt()));
        }
        if (m_rightClickCombo) {
            m_controller->setRightClickAction(static_cast<MouseButtonAction>(m_rightClickCombo->currentData().toInt()));
        }
        if (m_middleClickCombo) {
            m_controller->setMiddleClickAction(static_cast<MouseButtonAction>(m_middleClickCombo->currentData().toInt()));
        }
        m_controller->saveConfig();
    }
    accept();
}

} // namespace LunaRead
