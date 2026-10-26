#include <QApplication>
#include <QTest>
#include <iostream>

int runPageCurlOpenGLTests();

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "=== Running LunaRead PageCurlOpenGLWidget Test Suite ===" << std::endl;
    int status = runPageCurlOpenGLTests();

    if (status == 0) {
        std::cout << "=== ALL PAGE CURL OPENGL TESTS PASSED! ===" << std::endl;
    } else {
        std::cerr << "=== PAGE CURL OPENGL TESTS FAILED! ===" << std::endl;
    }

    return status;
}
