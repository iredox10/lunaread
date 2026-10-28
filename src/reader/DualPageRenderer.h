#pragma once

#include <QWidget>
#include <optional>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class Typesetter;

class DualPageRenderer : public QWidget {
    Q_OBJECT
public:
    explicit DualPageRenderer(QWidget* parent = nullptr);

    void setTypesetter(Typesetter* typesetter);
    void setBookInfo(const QString& bookTitle, const QString& chapterTitle);
    void setActiveSentence(const QString& sentence);
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
    Typesetter* m_typesetter = nullptr;
    QString m_bookTitle;
    QString m_chapterTitle;
    ParsedPage m_leftPage;
    std::optional<ParsedPage> m_rightPage;
    ReaderThemeTokens m_tokens;
    QString m_activeSentence;
    bool m_rightToLeft = false;
};

} // namespace LunaRead
