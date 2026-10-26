#include "DesktopShortcuts.h"

namespace LunaRead {

DesktopShortcuts::DesktopShortcuts(QObject* parent) : QObject(parent) {}

bool DesktopShortcuts::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        int key = keyEvent->key();
        Qt::KeyboardModifiers mods = keyEvent->modifiers();

        if (key == Qt::Key_Space || key == Qt::Key_PageDown || key == Qt::Key_Right || key == Qt::Key_J) {
            emit nextPageRequested();
            return true;
        } else if (key == Qt::Key_PageUp || key == Qt::Key_Left || key == Qt::Key_K) {
            emit prevPageRequested();
            return true;
        } else if (key == Qt::Key_B && mods == Qt::NoModifier) {
            emit toggleBookmarkRequested();
            return true;
        } else if (key == Qt::Key_N && mods == Qt::NoModifier) {
            emit toggleThemeRequested();
            return true;
        } else if (key == Qt::Key_T && mods == Qt::NoModifier) {
            emit toggleTtsRequested();
            return true;
        } else if ((key == Qt::Key_F && (mods & Qt::ControlModifier)) || (key == Qt::Key_Slash && mods == Qt::NoModifier)) {
            emit searchRequested();
            return true;
        } else if ((key == Qt::Key_C && mods == Qt::NoModifier) || (key == Qt::Key_T && (mods & Qt::ControlModifier)) || (key == Qt::Key_Tab && mods == Qt::NoModifier)) {
            emit openTocRequested();
            return true;
        } else if ((key == Qt::Key_A && mods == Qt::NoModifier) || (key == Qt::Key_S && mods == Qt::NoModifier)) {
            emit fontSettingsRequested();
            return true;
        } else if ((key == Qt::Key_Plus || key == Qt::Key_Equal) && (mods & Qt::ControlModifier)) {
            emit fontScaleRequested(1);
            return true;
        } else if (key == Qt::Key_Minus && (mods & Qt::ControlModifier)) {
            emit fontScaleRequested(-1);
            return true;
        } else if (key == Qt::Key_F11) {
            emit fullscreenToggleRequested();
            return true;
        } else if (key == Qt::Key_Escape) {
            emit escapeRequested();
            return true;
        }
    } else if (event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            int delta = wheelEvent->angleDelta().y();
            emit fontScaleRequested(delta > 0 ? 1 : -1);
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

} // namespace LunaRead
