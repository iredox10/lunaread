#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "BookshelfView.h"
#include "SystemTrayManager.h"
#include "../reader/ReaderViewport.h"

namespace LunaRead {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

public slots:
    void openFileImport();
    void openBook(const QString& bookId);
    void closeReader();
    void importFile(const QString& filePath);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupMenuBar();
    void setupShortcuts();

    QStackedWidget* m_stackedWidget = nullptr;
    BookshelfView* m_bookshelf = nullptr;
    ReaderViewport* m_reader = nullptr;
    SystemTrayManager* m_trayManager = nullptr;
};

} // namespace LunaRead
