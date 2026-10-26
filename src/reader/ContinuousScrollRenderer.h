#pragma once

#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

class ContinuousScrollRenderer : public QScrollArea {
    Q_OBJECT
public:
    explicit ContinuousScrollRenderer(QWidget* parent = nullptr);

    void setDocument(const BookDocument& doc, const ReaderThemeTokens& tokens);
    void updateTheme(const ReaderThemeTokens& tokens);

signals:
    void scrollProgressChanged(double percentage);

protected:
    void scrollContentsBy(int dx, int dy) override;

private:
    QWidget* m_container = nullptr;
    QVBoxLayout* m_layout = nullptr;
    ReaderThemeTokens m_tokens;
};

} // namespace LunaRead
