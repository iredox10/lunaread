#include <QApplication>
#include <QTest>
#include <QSignalSpy>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>

#include "ui/AutoScrollOverlayWidget.h"
#include "core/ThemeTokens.h"

using namespace LunaRead;

class TestAutoScrollOverlay : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testDefaultValues();
    void testSpeedAdjustmentPixelMode();
    void testSpeedAdjustmentLineMode();
    void testModeToggling();
    void testPlayPauseStop();
    void testScrollTimerEmitsSignals();
    void testLineByLineScrollTicks();
    void testTapOnScreenPausesAndResumes();
    void testKeyboardShortcuts();
    void testMouseWheelSpeedControl();
    void testRepositionAndDraggability();
    void testThemeUpdating();
    void testShowAndStartDismiss();
    void testPageByPageAutoScroll();
};

void TestAutoScrollOverlay::initTestCase() {
}

void TestAutoScrollOverlay::cleanupTestCase() {
}

void TestAutoScrollOverlay::testDefaultValues() {
    AutoScrollOverlayWidget overlay;

    QCOMPARE(overlay.isActive(), false);
    QCOMPARE(overlay.isScrolling(), false);
    QCOMPARE(overlay.mode(), AutoScrollMode::PixelByPixel);
    QCOMPARE(overlay.pixelSpeed(), AutoScrollOverlayWidget::DefaultPixelSpeed);
    QCOMPARE(overlay.lineSpeed(), AutoScrollOverlayWidget::DefaultLineSpeed);
    QCOMPARE(overlay.currentSpeed(), 30);
    QCOMPARE(overlay.lineHeight(), AutoScrollOverlayWidget::DefaultLineHeight);
    QCOMPARE(overlay.isTapToPauseEnabled(), true);
    QCOMPARE(overlay.isTapToResumeEnabled(), true);
    QCOMPARE(overlay.isShortcutsEnabled(), true);
    QCOMPARE(overlay.startOnShow(), false);

    auto* speedLabel = overlay.findChild<QLabel*>("autoScrollSpeedLabel");
    QVERIFY(speedLabel != nullptr);
    QCOMPARE(speedLabel->text(), "30 px/s");

    auto* modeBtn = overlay.findChild<QPushButton*>("autoScrollModeBtn");
    QVERIFY(modeBtn != nullptr);
    QCOMPARE(modeBtn->text(), "PX");
}

void TestAutoScrollOverlay::testSpeedAdjustmentPixelMode() {
    AutoScrollOverlayWidget overlay;
    QSignalSpy speedSpy(&overlay, &AutoScrollOverlayWidget::speedChanged);

    // Initial speed is 30 px/s
    overlay.increaseSpeed();
    QCOMPARE(overlay.pixelSpeed(), 35);
    QCOMPARE(overlay.currentSpeed(), 35);
    QCOMPARE(speedSpy.count(), 1);
    QCOMPARE(speedSpy.takeFirst().at(0).toInt(), 35);

    auto* speedLabel = overlay.findChild<QLabel*>("autoScrollSpeedLabel");
    QVERIFY(speedLabel != nullptr);
    QCOMPARE(speedLabel->text(), "35 px/s");

    overlay.decreaseSpeed();
    QCOMPARE(overlay.pixelSpeed(), 30);
    QCOMPARE(speedSpy.count(), 1);
    QCOMPARE(speedSpy.takeFirst().at(0).toInt(), 30);

    // Clamping to max speed (200 px/s)
    overlay.setPixelSpeed(300);
    QCOMPARE(overlay.pixelSpeed(), AutoScrollOverlayWidget::MaxPixelSpeed);
    QCOMPARE(speedLabel->text(), "200 px/s");

    // Clamping to min speed (5 px/s)
    overlay.setPixelSpeed(1);
    QCOMPARE(overlay.pixelSpeed(), AutoScrollOverlayWidget::MinPixelSpeed);
    QCOMPARE(speedLabel->text(), "5 px/s");

    // setSpeed should route to pixel speed in PixelByPixel mode
    overlay.setSpeed(60);
    QCOMPARE(overlay.pixelSpeed(), 60);
    QCOMPARE(overlay.currentSpeed(), 60);
}

void TestAutoScrollOverlay::testSpeedAdjustmentLineMode() {
    AutoScrollOverlayWidget overlay;
    overlay.setMode(AutoScrollMode::LineByLine);
    QCOMPARE(overlay.mode(), AutoScrollMode::LineByLine);

    auto* speedLabel = overlay.findChild<QLabel*>("autoScrollSpeedLabel");
    QVERIFY(speedLabel != nullptr);
    QCOMPARE(speedLabel->text(), "2 lines/s");

    QSignalSpy speedSpy(&overlay, &AutoScrollOverlayWidget::speedChanged);

    // Increase line speed
    overlay.increaseSpeed();
    QCOMPARE(overlay.lineSpeed(), 3);
    QCOMPARE(overlay.currentSpeed(), 3);
    QCOMPARE(speedSpy.count(), 1);
    QCOMPARE(speedLabel->text(), "3 lines/s");

    // Decrease line speed to 1 line/s
    overlay.decreaseSpeed();
    overlay.decreaseSpeed();
    QCOMPARE(overlay.lineSpeed(), 1);
    QCOMPARE(speedLabel->text(), "1 line/s");

    // Clamp to min (1)
    overlay.decreaseSpeed();
    QCOMPARE(overlay.lineSpeed(), 1);

    // Clamp to max (15)
    overlay.setLineSpeed(50);
    QCOMPARE(overlay.lineSpeed(), AutoScrollOverlayWidget::MaxLineSpeed);
    QCOMPARE(speedLabel->text(), "15 lines/s");

    // Custom line height
    overlay.setLineHeight(36);
    QCOMPARE(overlay.lineHeight(), 36);
}

void TestAutoScrollOverlay::testModeToggling() {
    AutoScrollOverlayWidget overlay;
    QSignalSpy modeSpy(&overlay, &AutoScrollOverlayWidget::modeChanged);

    QCOMPARE(overlay.mode(), AutoScrollMode::PixelByPixel);

    overlay.toggleMode();
    QCOMPARE(overlay.mode(), AutoScrollMode::LineByLine);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(modeSpy.takeFirst().at(0).value<AutoScrollMode>(), AutoScrollMode::LineByLine);

    auto* modeBtn = overlay.findChild<QPushButton*>("autoScrollModeBtn");
    QVERIFY(modeBtn != nullptr);
    QCOMPARE(modeBtn->text(), "LN");

    overlay.toggleMode();
    QCOMPARE(overlay.mode(), AutoScrollMode::PageByPage);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(modeSpy.takeFirst().at(0).value<AutoScrollMode>(), AutoScrollMode::PageByPage);
    QCOMPARE(modeBtn->text(), "PG");

    overlay.toggleMode();
    QCOMPARE(overlay.mode(), AutoScrollMode::PixelByPixel);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(modeSpy.takeFirst().at(0).value<AutoScrollMode>(), AutoScrollMode::PixelByPixel);
    QCOMPARE(modeBtn->text(), "PX");
}

void TestAutoScrollOverlay::testPlayPauseStop() {
    AutoScrollOverlayWidget overlay;
    QSignalSpy toggleSpy(&overlay, &AutoScrollOverlayWidget::autoScrollToggled);

    QCOMPARE(overlay.isActive(), false);

    overlay.play();
    QCOMPARE(overlay.isActive(), true);
    QCOMPARE(toggleSpy.count(), 1);
    QCOMPARE(toggleSpy.takeFirst().at(0).toBool(), true);

    // Idempotent play call
    overlay.play();
    QCOMPARE(toggleSpy.count(), 0);

    overlay.pause();
    QCOMPARE(overlay.isActive(), false);
    QCOMPARE(toggleSpy.count(), 1);
    QCOMPARE(toggleSpy.takeFirst().at(0).toBool(), false);

    // Toggle
    overlay.togglePlayPause();
    QCOMPARE(overlay.isActive(), true);
    overlay.togglePlayPause();
    QCOMPARE(overlay.isActive(), false);

    // Stop resets active state
    overlay.play();
    overlay.stop();
    QCOMPARE(overlay.isActive(), false);
}

void TestAutoScrollOverlay::testScrollTimerEmitsSignals() {
    AutoScrollOverlayWidget overlay;
    overlay.setPixelSpeed(100);

    QSignalSpy scrollSpy(&overlay, &AutoScrollOverlayWidget::scrollStepRequested);

    overlay.play();
    // Wait for timer ticks to fire (~60 ticks/s)
    QTest::qWait(120);
    overlay.pause();

    QVERIFY(scrollSpy.count() > 0);

    int totalDelta = 0;
    for (int i = 0; i < scrollSpy.count(); ++i) {
        int delta = scrollSpy.at(i).at(0).toInt();
        QVERIFY(delta > 0);
        totalDelta += delta;
    }
    // At 100 px/s for ~120ms, expected distance is ~12 pixels (with tolerance)
    QVERIFY(totalDelta >= 6 && totalDelta <= 25);

    // Once paused, no further scroll ticks should occur
    int countAtPause = scrollSpy.count();
    QTest::qWait(60);
    QCOMPARE(scrollSpy.count(), countAtPause);
}

void TestAutoScrollOverlay::testLineByLineScrollTicks() {
    AutoScrollOverlayWidget overlay;
    overlay.setMode(AutoScrollMode::LineByLine);
    overlay.setLineSpeed(10); // 10 lines/s -> 1 line every 100ms
    overlay.setLineHeight(24);

    QSignalSpy scrollSpy(&overlay, &AutoScrollOverlayWidget::scrollStepRequested);

    overlay.play();
    QTest::qWait(250);
    overlay.pause();

    QVERIFY(scrollSpy.count() > 0);
    for (int i = 0; i < scrollSpy.count(); ++i) {
        int delta = scrollSpy.at(i).at(0).toInt();
        QVERIFY(delta % 24 == 0); // Each tick must be a multiple of line height
    }
}

void TestAutoScrollOverlay::testTapOnScreenPausesAndResumes() {
    QWidget viewport;
    viewport.resize(800, 600);
    viewport.show();

    AutoScrollOverlayWidget overlay(&viewport);
    overlay.show();
    overlay.reposition(viewport.size());

    // Initially start playing
    overlay.play();
    QCOMPARE(overlay.isActive(), true);

    // Simulate clicking viewport outside overlay HUD (e.g., at 100, 100)
    QMouseEvent pressOutside(QEvent::MouseButtonPress, QPointF(100, 100), QPointF(100, 100),
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    bool filtered = overlay.eventFilter(&viewport, &pressOutside);
    QCOMPARE(filtered, true); // Event intercepted
    QCOMPARE(overlay.isActive(), false); // Auto-scroll paused!

    // Simulate clicking viewport again while paused -> resumes!
    filtered = overlay.eventFilter(&viewport, &pressOutside);
    QCOMPARE(filtered, true);
    QCOMPARE(overlay.isActive(), true); // Auto-scroll resumed!

    // Disable tap-to-pause
    overlay.setTapToPauseEnabled(false);
    filtered = overlay.eventFilter(&viewport, &pressOutside);
    QCOMPARE(filtered, false);
    QCOMPARE(overlay.isActive(), true); // Remains active
}

void TestAutoScrollOverlay::testKeyboardShortcuts() {
    QWidget viewport;
    viewport.resize(800, 600);
    viewport.show();

    AutoScrollOverlayWidget overlay(&viewport);
    overlay.show();

    // 1. Space toggles play/pause
    QKeyEvent spaceEvent(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
    bool filtered = overlay.eventFilter(&viewport, &spaceEvent);
    QCOMPARE(filtered, true);
    QCOMPARE(overlay.isActive(), true);

    filtered = overlay.eventFilter(&viewport, &spaceEvent);
    QCOMPARE(filtered, true);
    QCOMPARE(overlay.isActive(), false);

    // 2. Up arrow increases speed
    int initialSpeed = overlay.pixelSpeed();
    QKeyEvent upEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
    filtered = overlay.eventFilter(&viewport, &upEvent);
    QCOMPARE(filtered, true);
    QCOMPARE(overlay.pixelSpeed(), initialSpeed + AutoScrollOverlayWidget::PixelSpeedStep);

    // 3. Down arrow decreases speed
    QKeyEvent downEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    filtered = overlay.eventFilter(&viewport, &downEvent);
    QCOMPARE(filtered, true);
    QCOMPARE(overlay.pixelSpeed(), initialSpeed);

    // 4. M toggles mode
    QKeyEvent mEvent(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    filtered = overlay.eventFilter(&viewport, &mEvent);
    QCOMPARE(filtered, true);
    QCOMPARE(overlay.mode(), AutoScrollMode::LineByLine);

    // 5. Escape dismisses
    QSignalSpy closeSpy(&overlay, &AutoScrollOverlayWidget::closeRequested);
    QKeyEvent escEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    filtered = overlay.eventFilter(&viewport, &escEvent);
    QCOMPARE(filtered, true);
    QCOMPARE(closeSpy.count(), 1);
    QCOMPARE(overlay.isVisible(), false);
}

void TestAutoScrollOverlay::testMouseWheelSpeedControl() {
    AutoScrollOverlayWidget overlay;
    int initialSpeed = overlay.pixelSpeed();

    // Scroll up increases speed
    QWheelEvent wheelUp(QPointF(20, 20), QPointF(20, 20), QPoint(0, 0), QPoint(0, 120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&overlay, &wheelUp);
    QCOMPARE(overlay.pixelSpeed(), initialSpeed + AutoScrollOverlayWidget::PixelSpeedStep);

    // Scroll down decreases speed
    QWheelEvent wheelDown(QPointF(20, 20), QPointF(20, 20), QPoint(0, 0), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&overlay, &wheelDown);
    QCOMPARE(overlay.pixelSpeed(), initialSpeed);
}

void TestAutoScrollOverlay::testRepositionAndDraggability() {
    QWidget viewport;
    viewport.resize(800, 600);

    AutoScrollOverlayWidget overlay(&viewport);
    overlay.reposition(viewport.size(), true);

    const int expectedX = 800 - overlay.width() - 24;
    const int expectedY = 600 - overlay.height() - 24;
    QCOMPARE(overlay.x(), expectedX);
    QCOMPARE(overlay.y(), expectedY);

    // Simulate resizing parent viewport
    viewport.resize(1024, 768);
    overlay.reposition(viewport.size());
    QCOMPARE(overlay.x(), 1024 - overlay.width() - 24);
    QCOMPARE(overlay.y(), 768 - overlay.height() - 24);

    // Drag overlay
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&overlay, &press);

    QMouseEvent move(QEvent::MouseMove, QPointF(50, 50), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&overlay, &move);

    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(50, 50), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&overlay, &release);

    // Reset back to bottom right
    overlay.resetToDefaultPosition();
    QCOMPARE(overlay.x(), 1024 - overlay.width() - 24);
    QCOMPARE(overlay.y(), 768 - overlay.height() - 24);
}

void TestAutoScrollOverlay::testThemeUpdating() {
    AutoScrollOverlayWidget overlay;

    // Apply various reader theme presets
    overlay.updateTheme(ReaderThemeTokens::night());
    overlay.updateTheme(ReaderThemeTokens::sepia());
    overlay.updateTheme(ReaderThemeTokens::parchment());
    overlay.updateTheme(ReaderThemeTokens::oledBlack());
    overlay.updateTheme(ReaderThemeTokens::modernLight());

    // Still functional after theme switches
    QCOMPARE(overlay.isActive(), false);
}

void TestAutoScrollOverlay::testShowAndStartDismiss() {
    AutoScrollOverlayWidget overlay;
    QSignalSpy closeSpy(&overlay, &AutoScrollOverlayWidget::closeRequested);

    overlay.showAndStart();
    QCOMPARE(overlay.isVisible(), true);
    QCOMPARE(overlay.isActive(), true);

    overlay.dismiss();
    QCOMPARE(overlay.isVisible(), false);
    QCOMPARE(overlay.isActive(), false);
    QCOMPARE(closeSpy.count(), 1);
}

void TestAutoScrollOverlay::testPageByPageAutoScroll() {
    AutoScrollOverlayWidget overlay;
    overlay.setMode(AutoScrollMode::PageByPage);
    QCOMPARE(overlay.mode(), AutoScrollMode::PageByPage);
    QCOMPARE(overlay.pageIntervalSec(), AutoScrollOverlayWidget::DefaultPageIntervalSec);

    auto* speedLabel = overlay.findChild<QLabel*>("autoScrollSpeedLabel");
    QVERIFY(speedLabel != nullptr);
    QCOMPARE(speedLabel->text(), "15s / pg");

    overlay.setPageIntervalSec(10);
    QCOMPARE(overlay.pageIntervalSec(), 10);
    QCOMPARE(speedLabel->text(), "10s / pg");

    // Clamping limits
    overlay.setPageIntervalSec(2);
    QCOMPARE(overlay.pageIntervalSec(), AutoScrollOverlayWidget::MinPageIntervalSec);
    overlay.setPageIntervalSec(200);
    QCOMPARE(overlay.pageIntervalSec(), AutoScrollOverlayWidget::MaxPageIntervalSec);

    // Increase speed makes interval smaller (faster page turns)
    overlay.setPageIntervalSec(20);
    overlay.increaseSpeed();
    QCOMPARE(overlay.pageIntervalSec(), 18);

    // Decrease speed makes interval larger (slower page turns)
    overlay.decreaseSpeed();
    QCOMPARE(overlay.pageIntervalSec(), 20);
}

int runAutoScrollTests() {
    TestAutoScrollOverlay tc;
    return QTest::qExec(&tc);
}

#include "test_auto_scroll.moc"
