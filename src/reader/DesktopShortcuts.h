#pragma once

#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include <QWheelEvent>

namespace LunaRead {

class DesktopShortcuts : public QObject {
    Q_OBJECT
public:
    explicit DesktopShortcuts(QObject* parent = nullptr);

signals:
    void nextPageRequested();
    void prevPageRequested();
    void toggleBookmarkRequested();
    void toggleThemeRequested();
    void toggleTtsRequested();
    void searchRequested();
    void fullscreenToggleRequested();
    void escapeRequested();
    void fontScaleRequested(int delta);
    void openTocRequested();
    void fontSettingsRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};

} // namespace LunaRead
