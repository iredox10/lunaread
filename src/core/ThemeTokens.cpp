#include "ThemeTokens.h"

namespace LunaRead {

ReaderThemeTokens ReaderThemeTokens::parchment() {
    ReaderThemeTokens t;
    t.id = "parchment";
    t.name = "Parchment";
    t.backgroundColor = QColor(0xFB, 0xF0, 0xD9);
    t.controlBgColor = QColor(0xF0, 0xE4, 0xCA);
    t.textColor = QColor(0x3A, 0x2E, 0x2B);
    t.accentColor = QColor(0x8D, 0x6E, 0x63);
    t.headerFooterColor = QColor(0x7D, 0x6E, 0x63);
    t.selectionColor = QColor(0xC4, 0x97, 0x5C, 120);
    t.dividerColor = QColor(0xE0, 0xD3, 0xB8);
    t.isDark = false;
    return t;
}

ReaderThemeTokens ReaderThemeTokens::oledBlack() {
    ReaderThemeTokens t;
    t.id = "oledBlack";
    t.name = "OLED Pure Black";
    t.backgroundColor = QColor(0x00, 0x00, 0x00);
    t.controlBgColor = QColor(0x18, 0x18, 0x1B);
    t.textColor = QColor(0xD4, 0xD4, 0xD8);
    t.accentColor = QColor(0x60, 0xA5, 0xFA);
    t.headerFooterColor = QColor(0x71, 0x71, 0x7A);
    t.selectionColor = QColor(0x3B, 0x82, 0xF6, 120);
    t.dividerColor = QColor(0x27, 0x27, 0x2A);
    t.isDark = true;
    return t;
}

ReaderThemeTokens ReaderThemeTokens::sepia() {
    ReaderThemeTokens t;
    t.id = "sepia";
    t.name = "Warm Sepia";
    t.backgroundColor = QColor(0xEF, 0xE6, 0xD5);
    t.controlBgColor = QColor(0xE5, 0xDA, 0xC5);
    t.textColor = QColor(0x43, 0x34, 0x22);
    t.accentColor = QColor(0xB0, 0x89, 0x68);
    t.headerFooterColor = QColor(0x8C, 0x7B, 0x68);
    t.selectionColor = QColor(0xD2, 0xB4, 0x8C, 120);
    t.dividerColor = QColor(0xDD, 0xD2, 0xBF);
    t.isDark = false;
    return t;
}

ReaderThemeTokens ReaderThemeTokens::night() {
    ReaderThemeTokens t;
    t.id = "night";
    t.name = "Night Charcoal";
    t.backgroundColor = QColor(0x18, 0x18, 0x1B);
    t.controlBgColor = QColor(0x27, 0x27, 0x2A);
    t.textColor = QColor(0xE4, 0xE4, 0xE7);
    t.accentColor = QColor(0xA1, 0xA1, 0xAA);
    t.headerFooterColor = QColor(0x71, 0x71, 0x7A);
    t.selectionColor = QColor(0x52, 0x52, 0x5B, 120);
    t.dividerColor = QColor(0x27, 0x27, 0x2A);
    t.isDark = true;
    return t;
}

ReaderThemeTokens ReaderThemeTokens::modernLight() {
    ReaderThemeTokens t;
    t.id = "modernLight";
    t.name = "Modern Light";
    t.backgroundColor = QColor(0xFF, 0xFF, 0xFF);
    t.controlBgColor = QColor(0xF3, 0xF4, 0xF6);
    t.textColor = QColor(0x1F, 0x29, 0x37);
    t.accentColor = QColor(0x25, 0x63, 0xEB);
    t.headerFooterColor = QColor(0x6B, 0x72, 0x80);
    t.selectionColor = QColor(0x93, 0xC5, 0xFD, 120);
    t.dividerColor = QColor(0xE5, 0xE7, 0xEB);
    t.isDark = false;
    return t;
}

ReaderThemeTokens ReaderThemeTokens::fromMode(ReaderThemeMode mode) {
    switch (mode) {
        case ReaderThemeMode::Parchment:   return parchment();
        case ReaderThemeMode::OledBlack:   return oledBlack();
        case ReaderThemeMode::Sepia:       return sepia();
        case ReaderThemeMode::Night:       return night();
        case ReaderThemeMode::ModernLight: return modernLight();
    }
    return parchment();
}

} // namespace LunaRead
