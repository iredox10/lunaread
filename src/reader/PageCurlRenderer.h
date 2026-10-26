#pragma once

#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <optional>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class PageCurlRenderer {
public:
    static void paint(
        QPainter* painter,
        const QRectF& bounds,
        const ParsedPage& currentPage,
        const std::optional<ParsedPage>& nextPage,
        qreal progress, // 0.0 = flat, 1.0 = turned
        const QPointF& origin,
        const QPointF& dragPos,
        const ReaderThemeTokens& tokens,
        bool isDualPage = false,
        const QString& highlightedSentence = QString()
    );

private:
    static void renderPageSurface(
        QPainter* painter,
        const QRectF& rect,
        const ParsedPage& page,
        const ReaderThemeTokens& tokens,
        const QString& highlightedSentence = QString()
    );
};

} // namespace LunaRead
