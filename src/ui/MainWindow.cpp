#include "MainWindow.h"
#include "../parsers/ParserFactory.h"
#include "../db/DatabaseManager.h"
#include "OpdsBrowserDialog.h"
#include <QInputDialog>
#include "AnnotationExportDialog.h"
#include "KeyboardShortcutsDialog.h"
#include "WebDavSyncDialog.h"
#include "ReadingStatsDialog.h"
#include "ThemeCustomizerDialog.h"
#include "BackupRestoreDialog.h"
#include "LocalFilesBrowserDialog.h"
#include "TextReplacementDialog.h"
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
    connect(m_reader, &ReaderViewport::fullscreenRequested, this, [this]() {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    });
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

    auto* localFilesAction = fileMenu->addAction(tr("📁 &My Files (Local Browser)..."), this, &MainWindow::openLocalFilesBrowser);
    localFilesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    auto* opdsAction = fileMenu->addAction(tr("&Online Catalogs (OPDS)..."), this, &MainWindow::openOpdsBrowser);
    opdsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    auto* backupAction = fileMenu->addAction(tr("&Backup & Restore Library..."), this, &MainWindow::openBackupRestore);

    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto* toolsMenu = menuBar()->addMenu(tr("&Tools"));

    auto* statsAction = toolsMenu->addAction(tr("📊 &Reading Statistics Dashboard..."), this, &MainWindow::openReadingStats);
    statsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));

    auto* syncAction = toolsMenu->addAction(tr("☁️ &WebDAV Cloud Sync..."), this, &MainWindow::openWebDavSync);
    syncAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));

    auto* filterAction = toolsMenu->addAction(tr("📝 &Text Replacement & Filter Rules..."), this, [this]() {
        auto* dlg = new TextReplacementDialog(QString(), this);
        dlg->show();
    });
    filterAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));

    auto* themeAction = toolsMenu->addAction(tr("🎨 &Theme Customizer..."), this, &MainWindow::openThemeCustomizer);
    auto* exportAction = toolsMenu->addAction(tr("📤 &Export Book Annotations..."), this, [this]() {
        if (m_stackedWidget->currentWidget() == m_reader) {
            m_reader->exportAnnotations();
        } else {
            auto books = DatabaseManager::instance().getAllBooks();
            if (books.isEmpty()) {
                QMessageBox::information(this, tr("Export Annotations"), tr("No books found in library."));
                return;
            }
            QStringList titles;
            for (const auto& b : books) titles.append(b.title);
            bool ok = false;
            QString chosen = QInputDialog::getItem(this, tr("Export Annotations"), tr("Select book to export:"), titles, 0, false, &ok);
            if (ok && !chosen.isEmpty()) {
                for (const auto& b : books) {
                    if (b.title == chosen) {
                        auto anns = DatabaseManager::instance().getAnnotationsForBook(b.id);
                        auto* dlg = new AnnotationExportDialog(b.title, anns, this);
                        dlg->show();
                        break;
                    }
                }
            }
        }
    });

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* fullScreenAction = viewMenu->addAction(tr("&Full Screen"), this, [this]() {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    });
    fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* shortcutsAction = helpMenu->addAction(tr("⌨️ &Keyboard Shortcuts Reference..."), this, [this]() {
        auto* dlg = new KeyboardShortcutsDialog(this);
        dlg->show();
    });
    shortcutsAction->setShortcut(QKeySequence(Qt::Key_F1));
    helpMenu->addSeparator();
    auto* aboutAction = helpMenu->addAction(tr("ℹ️ &About LunaRead"), this, [this]() {
        QMessageBox::about(this, tr("About LunaRead"),
            tr("<h3>LunaRead Desktop</h3>"
               "<p><b>Version 1.0 (Qt 6.11 / C++20)</b></p>"
               "<p>A feature-complete desktop e-book reading environment inspired by Moon+ Reader Pro.</p>"
               "<ul>"
               "<li>Smooth 3D page-curl, continuous vertical scroll, and dual-page reading</li>"
               "<li>14 e-book formats supported (EPUB, PDF, MOBI, FB2, CBZ, DJVU, TXT, MD, HTML, CHM, UMD, RTF, DOCX, ODT)</li>"
               "<li>Eye-care 20-20-20 timer and hardware/software blue light filter</li>"
               "<li>Full WebDAV and OPDS catalog synchronization</li>"
               "<li>Multi-language lexicon dictionary and spoken TTS narration</li>"
               "</ul>"));
    });
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
        doc->id = item->id;
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

void MainWindow::openOpdsBrowser() {
    auto* dlg = new OpdsBrowserDialog(this);
    connect(dlg, &OpdsBrowserDialog::bookImported, this, [this](const QString& filePath) {
        importFile(filePath);
    });
    dlg->show();
}

void MainWindow::openWebDavSync() {
    auto* dlg = new WebDavSyncDialog(this);
    dlg->show();
}

void MainWindow::openReadingStats() {
    auto* dlg = new ReadingStatsDialog(this);
    dlg->show();
}

void MainWindow::openThemeCustomizer() {
    auto* dlg = new ThemeCustomizerDialog(this);
    connect(dlg, &ThemeCustomizerDialog::themeCreated, this, [this](const ReaderThemeTokens& tokens) {
        if (m_reader) {
            m_reader->setThemeMode(ReaderThemeMode::ModernLight);
            Q_UNUSED(tokens);
        }
    });
    dlg->show();
}

void MainWindow::openBackupRestore() {
    auto* dlg = new BackupRestoreDialog(this);
    dlg->exec();
    m_bookshelf->refreshBooks();
    m_trayManager->updateRecentBooksMenu();
}

void MainWindow::openLocalFilesBrowser() {
    auto* dlg = new LocalFilesBrowserDialog(QDir::homePath(), this);
    connect(dlg, &LocalFilesBrowserDialog::importRequested, this, [this](const QStringList& filePaths) {
        for (const auto& path : filePaths) {
            importFile(path);
        }
    });
    connect(dlg, &LocalFilesBrowserDialog::openDirectlyRequested, this, [this](const QString& filePath) {
        importFile(filePath);
    });
    dlg->show();
}

} // namespace LunaRead
