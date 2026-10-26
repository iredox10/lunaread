#pragma once

#include <QString>
#include <QColor>
#include "Types.h"

namespace LunaRead {

/// Visual style tokens for reader themes
struct ReaderThemeTokens {
    QString id;
    QString name;
    QColor backgroundColor;
    QColor controlBgColor;
    QColor textColor;
    QColor accentColor;
    QColor headerFooterColor;
    QColor selectionColor;
    QColor dividerColor;
    bool isDark = false;

    static ReaderThemeTokens parchment();
    static ReaderThemeTokens oledBlack();
    static ReaderThemeTokens sepia();
    static ReaderThemeTokens night();
    static ReaderThemeTokens modernLight();

    static ReaderThemeTokens fromMode(ReaderThemeMode mode);
    static ReaderThemeTokens getTheme(ReaderThemeMode mode) { return fromMode(mode); }
};

using ThemePreset = ReaderThemeTokens;

} // namespace LunaRead
