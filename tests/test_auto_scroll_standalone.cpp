#include <QApplication>
#include <QTest>
#include <iostream>

int runAutoScrollTests();

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "=== Running LunaRead AutoScrollOverlayWidget Test Suite ===" << std::endl;
    int status = runAutoScrollTests();

    if (status == 0) {
        std::cout << "=== ALL AUTO-SCROLL OVERLAY TESTS PASSED! ===" << std::endl;
    } else {
        std::cerr << "=== AUTO-SCROLL OVERLAY TESTS FAILED! ===" << std::endl;
    }

    return status;
}
