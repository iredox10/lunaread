#pragma once

#include <QWidget>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class DualPageRenderer : public QWidget {
    Q_OBJECT
public:
    explicit DualPageRenderer(QWidget* parent = nullptr);

    void setPages(const ParsedPage& left, const std::optional<ParsedPage>& right, const ReaderThemeTokens& tokens);
    void updateTheme(const ReaderThemeTokens& tokens);
    void setRightToLeft(bool rtl);
    [[nodiscard]] bool isRightToLeft() const noexcept { return m_rightToLeft; }

signals:
    void pageTurnRequested(int delta);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    ParsedPage m_leftPage;
    std::optional<ParsedPage> m_rightPage;
    ReaderThemeTokens m_tokens;
    bool m_rightToLeft = false;
};

} // namespace LunaRead
