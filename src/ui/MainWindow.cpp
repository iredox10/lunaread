#include "MainWindow.h"
#include "../parsers/ParserFactory.h"
#include "../db/DatabaseManager.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QFileInfo>
#include <QDateTime>
#include <QShortcut>

namespace LunaRead {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("LunaRead - The Next-Gen Moon+ Reader for Desktop"));
    resize(1200, 800);
    setAcceptDrops(true);

    setupMenuBar();

    m_stackedWidget = new QStackedWidget(this);
    setCentralWidget(m_stackedWidget);

    m_bookshelf = new BookshelfView(this);
    connect(m_bookshelf, &BookshelfView::bookSelected, this, &MainWindow::openBook);
    m_stackedWidget->addWidget(m_bookshelf);

    m_reader = new ReaderViewport(this);
    connect(m_reader, &ReaderViewport::closeRequested, this, &MainWindow::closeReader);
    m_stackedWidget->addWidget(m_reader);

    m_stackedWidget->setCurrentWidget(m_bookshelf);

    // Initialize System Tray
    m_trayManager = new SystemTrayManager(this, this);
    m_trayManager->show();
    connect(m_trayManager, &SystemTrayManager::openBookRequested, this, &MainWindow::openBook);

    setupShortcuts();
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* importAction = fileMenu->addAction(tr("&Import Book..."), this, &MainWindow::openFileImport);
    importAction->setShortcut(QKeySequence::Open);

    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* fullScreenAction = viewMenu->addAction(tr("&Full Screen"), this, [this]() {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    });
    fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));
}

void MainWindow::setupShortcuts() {
    // Quick open file
    auto* openShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_O), this);
    connect(openShortcut, &QShortcut::activated, this, &MainWindow::openFileImport);

    // Return to bookshelf / Escape
    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, [this]() {
        if (isFullScreen()) {
            showNormal();
        } else if (m_stackedWidget->currentWidget() == m_reader) {
            closeReader();
        }
    });
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto& url : urls) {
            if (url.isLocalFile() && ParserFactory::isSupported(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    for (const auto& url : urls) {
        if (url.isLocalFile()) {
            importFile(url.toLocalFile());
        }
    }
    event->acceptProposedAction();
}

void MainWindow::openFileImport() {
    QString filter = tr("All Supported E-books (*.epub *.pdf *.djvu *.mobi *.azw *.azw3 *.prc *.fb2 *.docx *.odt *.rtf *.txt *.md *.cbz *.cbr *.html *.htm *.xhtml *.chm *.umd);;All Files (*.*)");
    QString path = QFileDialog::getOpenFileName(this, tr("Import E-book into LunaRead"), QString(), filter);
    if (!path.isEmpty()) {
        importFile(path);
    }
}

void MainWindow::importFile(const QString& path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) return;

    auto doc = ParserFactory::parse(path);
    if (!doc.has_value()) {
        QMessageBox::warning(this, tr("Import Failed"), tr("Could not parse e-book file or format is unsupported: %1").arg(path));
        return;
    }

    QFileInfo info(path);
    BookItem item;
    item.id = doc->id;
    item.title = doc->title.isEmpty() ? info.baseName() : doc->title;
    item.author = doc->author.isEmpty() ? tr("Unknown Author") : doc->author;
    item.filePath = path;
    item.fileSize = info.size();
    item.fileHash = doc->id;
    item.format = doc->format;
    item.totalChapters = doc->totalChapters;
    item.dateAdded = QDateTime::currentMSecsSinceEpoch();
    item.lastReadTime = item.dateAdded;

    DatabaseManager::instance().insertBook(item);
    m_bookshelf->refreshBooks();
    m_trayManager->updateRecentBooksMenu();

    // Directly open the imported book
    m_stackedWidget->setCurrentWidget(m_reader);
    m_reader->loadDocument(*doc);
}

void MainWindow::openBook(const QString& bookId) {
    auto item = DatabaseManager::instance().getBookById(bookId);
    if (!item.has_value()) return;

    auto doc = ParserFactory::parse(item->filePath);
    if (doc.has_value()) {
        m_stackedWidget->setCurrentWidget(m_reader);
        m_reader->loadDocument(*doc);
        if (item->currentChapterIndex > 0) {
            m_reader->jumpToChapter(item->currentChapterIndex);
        }
        if (item->currentPageIndex > 0) {
            m_reader->jumpToPage(item->currentPageIndex);
        }
    } else {
        QMessageBox::warning(this, tr("Failed to Open"), tr("Could not read book file: %1").arg(item->filePath));
    }
}

void MainWindow::closeReader() {
    m_stackedWidget->setCurrentWidget(m_bookshelf);
    m_bookshelf->refreshBooks();
    m_trayManager->updateRecentBooksMenu();
}

} // namespace LunaRead
