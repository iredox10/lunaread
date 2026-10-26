#include <QCoreApplication>
#include <QTest>
#include <iostream>

// Forward declarations of test classes
int runUtilsTests();
int runParsersTests();
int runDbTests();
int runOpdsTests();
int runSyncTests();

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    int status = 0;
    std::cout << "=== Running LunaRead-Qt Desktop Test Suite ===" << std::endl;

    status |= runUtilsTests();
    status |= runParsersTests();
    status |= runDbTests();
    status |= runOpdsTests();
    status |= runSyncTests();

    if (status == 0) {
        std::cout << "=== ALL TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    } else {
        std::cerr << "=== SOME TESTS FAILED! ===" << std::endl;
    }

    return status;
}
