#pragma once

#include <QFont>
#include <QMarginsF>
#include <QSizeF>
#include <QVector>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class Typesetter {
public:
    Typesetter();

    void setFont(const QFont& font);
    void setLineHeight(qreal multiplier);
    void setParagraphSpacing(qreal spacing);
    void setMargins(const QMarginsF& margins);
    void setParagraphIndent(qreal indentPx);
    void setAlignment(Qt::Alignment alignment);

    [[nodiscard]] const QFont& font() const noexcept { return m_font; }
    [[nodiscard]] qreal lineHeight() const noexcept { return m_lineHeight; }
    [[nodiscard]] qreal paragraphSpacing() const noexcept { return m_paragraphSpacing; }
    [[nodiscard]] const QMarginsF& margins() const noexcept { return m_margins; }
    [[nodiscard]] qreal paragraphIndent() const noexcept { return m_paragraphIndent; }
    [[nodiscard]] Qt::Alignment alignment() const noexcept { return m_alignment; }

    /// Paginates chapter into viewport-constrained pages without word-clipping.
    QVector<ParsedPage> paginate(const ChapterContent& chapter, const QSizeF& viewportSize, int startGlobalPageIndex = 0);

private:
    QFont m_font;
    qreal m_lineHeight = 1.5;
    qreal m_paragraphSpacing = 12.0;
    QMarginsF m_margins{24.0, 28.0, 24.0, 28.0};
    qreal m_paragraphIndent = 0.0;
    Qt::Alignment m_alignment = Qt::AlignJustify;
};

} // namespace LunaRead
