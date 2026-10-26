#include <QCoreApplication>
#include <QTest>
#include <iostream>

int runDbTests();

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "=== Running LunaRead-Qt Database Test Suite ===" << std::endl;
    int status = runDbTests();
    if (status == 0) {
        std::cout << "=== ALL DATABASE TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    } else {
        std::cerr << "=== DATABASE TESTS FAILED! ===" << std::endl;
    }
    return status;
}
