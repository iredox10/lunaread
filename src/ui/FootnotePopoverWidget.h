#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../core/ThemeTokens.h"

namespace LunaRead {

class FootnotePopoverWidget : public QWidget {
    Q_OBJECT

public:
    explicit FootnotePopoverWidget(QWidget* parent = nullptr);
    ~FootnotePopoverWidget() override = default;

    void setFootnote(const QString& referenceId, const QString& contentHtml, int targetChapter = -1, int targetPage = -1);
    void applyTheme(const ReaderThemeTokens& tokens);
    void reposition(const QSize& viewportSize, const QPointF& clickPos);

signals:
    void jumpRequested(int chapterIndex, int pageIndex);
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_titleLabel = nullptr;
    QTextBrowser* m_browser = nullptr;
    QPushButton* m_jumpBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    int m_targetChapter = -1;
    int m_targetPage = -1;
    ReaderThemeTokens m_tokens;
};

} // namespace LunaRead
