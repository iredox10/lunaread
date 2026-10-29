#include "../features/CoverService.h"
#include "../features/PdfPageService.h"
#include <QProgressDialog>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QApplication>
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
#include "PrivacyLockDialog.h"
#include "ReadingProfileDialog.h"
#include "AmbientSoundDialog.h"
#include "AllAnnotationsDialog.h"
#include "VocabularyNotebookDialog.h"
#include "../features/PrivacyLockService.h"
#include "../features/ImageCropService.h"
#include "../utils/HashUtils.h"
#include "../features/ReadingProfileManager.h"
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
        if (isFullScreen()) {
            showNormal();
            menuBar()->setVisible(true);
        } else {
            menuBar()->setVisible(false);
            showFullScreen();
        }
    });
    m_stackedWidget->addWidget(m_reader);
    connect(&ReadingProfileManager::instance(), &ReadingProfileManager::activeProfileChanged, this, [this](const ReadingProfile& p) {
        if (m_reader) {
            m_reader->setThemeMode(p.themeMode);
        }
    });

    connect(m_stackedWidget, &QStackedWidget::currentChanged, this, [this](int index) {
        const bool isReader = (m_stackedWidget->widget(index) == m_reader);
        menuBar()->setVisible(!isReader);
    });

    m_stackedWidget->setCurrentWidget(m_bookshelf);

    // Initialize System Tray
    m_trayManager = new SystemTrayManager(this, this);
    m_trayManager->show();
    connect(m_trayManager, &SystemTrayManager::openBookRequested, this, &MainWindow::openBook);

    setupShortcuts();

    if (PrivacyLockService::instance().requirePinOnStartup() && PrivacyLockService::instance().isLocked()) {
        QTimer::singleShot(100, this, [this]() {
            auto* dlg = new PrivacyLockDialog(PrivacyLockDialog::Mode::Unlock, this);
            if (dlg->exec() != QDialog::Accepted) {
                close();
            }
        });
    }
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

    auto* exportBookAction = fileMenu->addAction(tr("📤 &Export Book / Converter..."), this, [this]() {
        if (m_stackedWidget->currentWidget() == m_reader) {
            m_reader->openBookExporter();
        } else {
            QMessageBox::information(this, tr("Export Book"), tr("Please open a book to export its chapters."));
        }
    });

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

    auto* profileAction = toolsMenu->addAction(tr("🌗 &Reading Profiles (Day / Night)..."), this, [this]() {
        auto* dlg = new ReadingProfileDialog(this);
        connect(dlg, &ReadingProfileDialog::profileApplied, this, [this](const ReadingProfile& p) {
            if (m_reader) {
                m_reader->setThemeMode(p.themeMode);
            }
        });
        dlg->show();
    });

    auto* soundAction = toolsMenu->addAction(tr("🎵 &Ambient Sound Generator..."), this, [this]() {
        auto* dlg = new AmbientSoundDialog(this);
        dlg->show();
    });

    auto* privacyAction = toolsMenu->addAction(tr("🔒 &Privacy & App PIN Lock..."), this, [this]() {
        auto& lockService = PrivacyLockService::instance();
        auto mode = lockService.isPinSet() ? PrivacyLockDialog::Mode::ChangePin : PrivacyLockDialog::Mode::SetPin;
        auto* dlg = new PrivacyLockDialog(mode, this);
        dlg->exec();
    });

    auto* tapZoneAction = toolsMenu->addAction(tr("🎯 &Touch / Click Tap Zones Setup..."), this, [this]() {
        if (m_reader) {
            m_reader->openTapZoneConfig();
        }
    });
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

    auto* speedAction = toolsMenu->addAction(tr("⚡ &Speed Reader (RSVP Mode)..."), this, [this]() {
        if (m_stackedWidget->currentWidget() == m_reader) {
            m_reader->openSpeedReader();
        } else {
            QMessageBox::information(this, tr("Speed Reader"), tr("Please open a book to use Speed Reader."));
        }
    });
    speedAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));

    auto* allAnnsAction = toolsMenu->addAction(tr("🔖 &All Highlights & Notes Hub..."), this, [this]() {
        if (m_stackedWidget->currentWidget() == m_reader) {
            m_reader->openAllAnnotations();
        } else {
            auto* dlg = new AllAnnotationsDialog(QString(), tr("All Books"), this);
            dlg->exec();
        }
    });
    allAnnsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A));

    auto* vocabAction = toolsMenu->addAction(tr("📚 &Vocabulary Notebook & Flashcards..."), this, [this]() {
        auto* dlg = new VocabularyNotebookDialog(this);
        dlg->exec();
    });
    vocabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_V));

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* fullScreenAction = viewMenu->addAction(tr("&Full Screen"), this, [this]() {
        if (isFullScreen()) {
            showNormal();
            menuBar()->setVisible(true);
        } else {
            menuBar()->setVisible(false);
            showFullScreen();
        }
    });
    fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    viewMenu->addSeparator();

    auto* modeMenu = viewMenu->addMenu(tr("📖 &Reading Mode"));
    auto* curlAct = modeMenu->addAction(tr("✨ 3D Page Curl Simulation"), this, [this]() {
        m_reader->setReadingMode(ReadingMode::PageCurl);
    });
    curlAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));

    auto* slideAct = modeMenu->addAction(tr("↔️ Horizontal Slide Mode"), this, [this]() {
        m_reader->setReadingMode(ReadingMode::HorizontalSlide);
    });
    slideAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));

    auto* scrollAct = modeMenu->addAction(tr("📃 Continuous Vertical Scroll"), this, [this]() {
        m_reader->setReadingMode(ReadingMode::ContinuousScroll);
    });
    scrollAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));

    auto* dualAct = modeMenu->addAction(tr("📄 Dual-Page Book Spread (LTR)"), this, [this]() {
        m_reader->setReadingMode(ReadingMode::DualPage);
    });
    dualAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));

    auto* mangaAct = modeMenu->addAction(tr("🎌 Japanese Manga Mode (RTL Dual-Page)"), this, [this]() {
        m_reader->setReadingMode(ReadingMode::MangaRtl);
    });
    mangaAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));

    auto* webtoonAct = modeMenu->addAction(tr("📜 Seamless Webtoon Vertical Strip"), this, [this]() {
        m_reader->setReadingMode(ReadingMode::WebtoonVertical);
    });
    webtoonAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_6));

    viewMenu->addSeparator();
    auto* cropAction = viewMenu->addAction(tr("✂️ &Auto-Crop White Margins"), this, [this]() {
        if (m_reader) {
            m_reader->toggleAutoCropMargins();
        }
    });
    cropAction->setCheckable(true);
    cropAction->setChecked(ImageCropService::instance().isAutoCropEnabled());
    cropAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));

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

    // Fast check by file path first
    auto existingPath = DatabaseManager::instance().getBookByFilePath(path);
    if (existingPath.has_value()) {
        openBook(existingPath->id);
        return;
    }

    openFileDirectly(path);
}

void MainWindow::openFileDirectly(const QString& path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) return;

    // 1. Fast check if already imported in database
    auto existing = DatabaseManager::instance().getBookByFilePath(path);
    if (existing.has_value()) {
        openBook(existing->id);
        return;
    }

    QFileInfo info(path);
    QString fileName = info.fileName();

    // 2. Responsive progress dialog (shows only if parsing takes >350ms)
    auto* progress = new QProgressDialog(this);
    progress->setWindowTitle(tr("Opening Book"));
    progress->setLabelText(tr("Opening %1...\nPlease wait.").arg(fileName));
    progress->setRange(0, 0);
    progress->setCancelButton(nullptr);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(350);
    progress->setStyleSheet(
        "QProgressDialog { background-color: #1E293B; color: #FFFFFF; font-size: 13px; border-radius: 8px; }"
        "QLabel { color: #F1F5F9; font-weight: bold; padding: 10px; }"
    );

    // 3. Asynchronously parse book on background thread
    auto* watcher = new QFutureWatcher<std::optional<BookDocument>>(this);
    connect(watcher, &QFutureWatcher<std::optional<BookDocument>>::finished, this, [this, watcher, progress, path, info]() {
        progress->close();
        progress->deleteLater();
        auto docOpt = watcher->result();
        watcher->deleteLater();

        if (!docOpt.has_value()) {
            QMessageBox::warning(this, tr("Import Failed"), tr("Could not parse e-book file: %1").arg(path));
            return;
        }

        auto doc = std::move(*docOpt);
        BookItem item;
        item.id = doc.id;
        item.title = doc.title.isEmpty() ? info.completeBaseName() : doc.title;
        item.author = doc.author.isEmpty() ? tr("Unknown Author") : doc.author;
        item.filePath = path;
        item.fileSize = info.size();
        item.fileHash = doc.id;
        item.format = doc.format;
        item.totalChapters = doc.totalChapters;
        item.dateAdded = QDateTime::currentMSecsSinceEpoch();
        item.lastReadTime = item.dateAdded;

        if (!doc.coverImageBytes.isEmpty()) {
            item.coverImagePath = CoverService::instance().saveCoverBytes(item.id, doc.coverImageBytes);
        } else {
            CoverService::instance().requestCover(item);
        }

        DatabaseManager::instance().insertBook(item);
        m_bookshelf->refreshBooks();
        m_trayManager->updateRecentBooksMenu();

        m_stackedWidget->setCurrentWidget(m_reader);
        m_reader->loadDocument(doc);
        menuBar()->setVisible(false);
        showFullScreen();
    });

    watcher->setFuture(QtConcurrent::run([path]() {
        return ParserFactory::parse(path);
    }));
}

void MainWindow::openBook(const QString& bookId) {
    auto item = DatabaseManager::instance().getBookById(bookId);
    if (!item.has_value()) return;

    QString filePath = item->filePath;
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, tr("Failed to Open"), tr("Book file not found: %1").arg(filePath));
        return;
    }

    // Fast in-memory cache for ultra-instant book re-opening (<0.1ms)
    static QHash<QString, BookDocument> s_openedDocCache;
    if (auto it = s_openedDocCache.find(bookId); it != s_openedDocCache.end()) {
        m_stackedWidget->setCurrentWidget(m_reader);
        m_reader->loadDocument(it.value(), item->currentChapterIndex, item->currentPageIndex);
        menuBar()->setVisible(false);
        showFullScreen();
        return;
    }

    // Immediately prefetch target page in parallel on background worker
    const int targetPage = (item->format.compare("pdf", Qt::CaseInsensitive) == 0)
        ? std::max(1, item->currentChapterIndex + 1)
        : std::max(1, item->currentPageIndex + 1);
    if (item->format.compare("pdf", Qt::CaseInsensitive) == 0) {
        PdfPageService::instance().prefetchPage(filePath, bookId, targetPage, 150);
        PdfPageService::instance().prefetchPage(filePath, bookId, targetPage + 1, 150);
    }

    auto* progress = new QProgressDialog(this);
    progress->setWindowTitle(tr("Opening Book"));
    progress->setLabelText(tr("Opening %1...\nPlease wait.").arg(item->title));
    progress->setRange(0, 0);
    progress->setCancelButton(nullptr);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(400); // Only pop up for genuinely slow files >400ms
    progress->setStyleSheet(
        "QProgressDialog { background-color: #1E293B; color: #FFFFFF; font-size: 13px; border-radius: 8px; }"
        "QLabel { color: #F1F5F9; font-weight: bold; padding: 10px; }"
    );

    auto* watcher = new QFutureWatcher<std::optional<BookDocument>>(this);
    connect(watcher, &QFutureWatcher<std::optional<BookDocument>>::finished, this, [this, watcher, progress, item = *item]() {
        progress->close();
        progress->deleteLater();
        auto docOpt = watcher->result();
        watcher->deleteLater();

        if (docOpt.has_value()) {
            auto doc = std::move(*docOpt);
            doc.id = item.id;
            doc.filePath = item.filePath;
            if (item.coverImagePath.isEmpty() && !doc.coverImageBytes.isEmpty()) {
                CoverService::instance().saveCoverBytes(item.id, doc.coverImageBytes);
            }
            if (s_openedDocCache.size() > 8) {
                s_openedDocCache.clear();
            }
            s_openedDocCache.insert(item.id, doc);
            m_stackedWidget->setCurrentWidget(m_reader);
            m_reader->loadDocument(doc, item.currentChapterIndex, item.currentPageIndex);
            menuBar()->setVisible(false);
            showFullScreen();
        } else {
            QMessageBox::warning(this, tr("Failed to Open"), tr("Could not read book file: %1").arg(item.filePath));
        }
    });

    watcher->setFuture(QtConcurrent::run([filePath]() {
        return ParserFactory::parse(filePath);
    }));
}

void MainWindow::closeReader() {
    if (isFullScreen()) {
        showNormal();
        menuBar()->setVisible(true);
    }
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
    connect(dlg, &LocalFilesBrowserDialog::openDirectlyRequested, this, &MainWindow::openFileDirectly);
    dlg->show();
}

} // namespace LunaRead
