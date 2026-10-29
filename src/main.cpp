#include <QApplication>
#include <QTimer>
#include "ui/MainWindow.h"
#include "db/DatabaseManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LunaRead");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LunaRead");

    // Initialize cross-platform SQLite database
    if (!LunaRead::DatabaseManager::instance().initialize()) {
        qWarning("Database initialization failed, continuing with in-memory storage.");
    }

    LunaRead::MainWindow window;
    window.show();

    if (argc > 1) {
        QString argPath = QString::fromLocal8Bit(argv[1]);
        QTimer::singleShot(50, &window, [&window, argPath]() {
            window.importFile(argPath);
        });
    }

    return app.exec();
}
