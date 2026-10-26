#include <QTest>
#include <QSignalSpy>
#include <QImage>
#include <QPainter>
#include "reader/PageCurlOpenGLWidget.h"
#include "core/Types.h"
#include "core/ThemeTokens.h"

using namespace LunaRead;

class TestPageCurlOpenGL : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testDefaultState();
    void testSetPages();
    void testCurlProgressClampingAndSignals();
    void testDragPositionCalculatesAngle();
    void testDualPageMode();
    void testShadingAndGeometryProperties();
    void testRenderPageToImageHelper();
    void testAnimationTrigger();
    void testMousePeelingCalculations();
};

void TestPageCurlOpenGL::initTestCase() {
}

void TestPageCurlOpenGL::cleanupTestCase() {
}

void TestPageCurlOpenGL::testDefaultState() {
    PageCurlOpenGLWidget widget;
    QCOMPARE(widget.curlProgress(), 0.0);
    QCOMPARE(widget.curlAngle(), 0.0);
    QCOMPARE(widget.isDualPage(), false);
    QCOMPARE(widget.cylinderRadius(), 0.0);
    QVERIFY(widget.shadowStrength() > 0.0 && widget.shadowStrength() <= 1.0);
    QVERIFY(widget.specularStrength() > 0.0 && widget.specularStrength() <= 1.0);
    QVERIFY(widget.spineShadowWidth() > 0.0);
    QVERIFY(widget.spineShadowDepth() > 0.0 && widget.spineShadowDepth() <= 1.0);
    QVERIFY(!widget.isAnimating());
}

void TestPageCurlOpenGL::testSetPages() {
    PageCurlOpenGLWidget widget;

    // Test with valid rendered images
    QImage front(600, 800, QImage::Format_RGBA8888);
    front.fill(Qt::white);
    QImage back(600, 800, QImage::Format_RGBA8888);
    back.fill(Qt::lightGray);
    QImage next(600, 800, QImage::Format_RGBA8888);
    next.fill(Qt::yellow);

    widget.setPages(front, back, next);

    // Test with null images to verify robustness of fallback generation
    widget.setPages(QImage(), QImage(), QImage());
}

void TestPageCurlOpenGL::testCurlProgressClampingAndSignals() {
    PageCurlOpenGLWidget widget;
    QSignalSpy progressSpy(&widget, &PageCurlOpenGLWidget::curlProgressChanged);

    // Test normal progress
    widget.setCurlProgress(0.45);
    QCOMPARE(widget.curlProgress(), 0.45);
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.takeFirst().at(0).toReal(), 0.45);

    // Test negative clamping
    widget.setCurlProgress(-0.5);
    QCOMPARE(widget.curlProgress(), 0.0);

    // Test excess clamping
    widget.setCurlProgress(1.75);
    QCOMPARE(widget.curlProgress(), 1.0);
}

void TestPageCurlOpenGL::testDragPositionCalculatesAngle() {
    PageCurlOpenGLWidget widget;
    widget.resize(800, 1000);

    QSignalSpy angleSpy(&widget, &PageCurlOpenGLWidget::curlAngleChanged);

    // Drag from bottom-right corner upwards
    QPointF dragBottom(600.0, 900.0); // y > 500
    widget.setCurlProgress(0.2, dragBottom);
    QVERIFY(widget.curlAngle() > 0.0); // angle positive for bottom lift

    // Drag from top-right corner downwards
    QPointF dragTop(600.0, 100.0); // y < 500
    widget.setCurlProgress(0.3, dragTop);
    QVERIFY(widget.curlAngle() < 0.0); // angle negative for top lift

    // Angle limits should be respected
    widget.setCurlAngle(1.5); // exceed max 0.55
    QVERIFY(widget.curlAngle() <= 0.55);
}

void TestPageCurlOpenGL::testDualPageMode() {
    PageCurlOpenGLWidget widget;
    QSignalSpy dualSpy(&widget, &PageCurlOpenGLWidget::dualPageChanged);

    widget.setDualPage(true);
    QCOMPARE(widget.isDualPage(), true);
    QCOMPARE(dualSpy.count(), 1);

    // Test setDualPages convenience
    QImage left(400, 600, QImage::Format_RGBA8888);
    left.fill(Qt::cyan);
    QImage rightFront(400, 600, QImage::Format_RGBA8888);
    rightFront.fill(Qt::white);
    QImage rightBack(400, 600, QImage::Format_RGBA8888);
    rightBack.fill(Qt::lightGray);
    QImage nextRight(400, 600, QImage::Format_RGBA8888);
    nextRight.fill(Qt::yellow);

    widget.setDualPages(left, rightFront, rightBack, nextRight);
    QCOMPARE(widget.isDualPage(), true);
}

void TestPageCurlOpenGL::testShadingAndGeometryProperties() {
    PageCurlOpenGLWidget widget;

    widget.setCylinderRadius(35.5);
    QCOMPARE(widget.cylinderRadius(), 35.5);

    widget.setShadowStrength(0.85);
    QCOMPARE(widget.shadowStrength(), 0.85);

    widget.setSpecularStrength(0.55);
    QCOMPARE(widget.specularStrength(), 0.55);

    widget.setSpineShadowWidth(45.0);
    QCOMPARE(widget.spineShadowWidth(), 45.0);

    widget.setSpineShadowDepth(0.70);
    QCOMPARE(widget.spineShadowDepth(), 0.70);
}

void TestPageCurlOpenGL::testRenderPageToImageHelper() {
    ParsedPage page;
    page.text = "Chapter 1: The Adventure Begins.\n\nIt was a dark and stormy night.";
    page.pageIndexInChapter = 2;
    page.isImagePage = false;

    ReaderThemeTokens tokens = ReaderThemeTokens::fromMode(ReaderThemeMode::Sepia);
    QSize renderSize(600, 900);

    QImage rendered = PageCurlOpenGLWidget::renderPageToImage(page, renderSize, tokens, 3);
    QCOMPARE(rendered.size(), renderSize);
    QCOMPARE(rendered.format(), QImage::Format_RGBA8888);

    // Verify background color is applied
    QColor sampled = rendered.pixelColor(10, 10);
    QCOMPARE(sampled.rgb(), tokens.backgroundColor.rgb());
}

void TestPageCurlOpenGL::testAnimationTrigger() {
    PageCurlOpenGLWidget widget;
    widget.setCurlProgress(0.0);

    widget.startCurlAnimation(1.0, 100);
    QVERIFY(widget.isAnimating());

    widget.cancelAnimation();
    QVERIFY(!widget.isAnimating());
}

void TestPageCurlOpenGL::testMousePeelingCalculations() {
    PageCurlOpenGLWidget widget;
    widget.setInteractiveDragEnabled(true);
    QVERIFY(widget.isInteractiveDragEnabled());

    widget.setInteractiveDragEnabled(false);
    QVERIFY(!widget.isInteractiveDragEnabled());
}

int runPageCurlOpenGLTests() {
    TestPageCurlOpenGL test;
    return QTest::qExec(&test);
}

#include "test_page_curl_opengl.moc"
