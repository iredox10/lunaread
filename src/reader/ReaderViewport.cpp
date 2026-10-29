#include "ReaderViewport.h"
#include "DogEarBookmarkWidget.h"
#include "../features/PdfPageService.h"
#include "../ui/TocDrawerWidget.h"
#include "../ui/TtsControlBar.h"
#include "../ui/SearchInBookDialog.h"
#include "../ui/FontSettingsDialog.h"
#include "../ui/AutoScrollOverlayWidget.h"
#include "../ui/NightFilterOverlayWidget.h"
#include "../ui/QuickNavPopoverWidget.h"
#include "../ui/TapZoneConfigDialog.h"
#include "../ui/BookExporterDialog.h"
#include "../ui/SpeedReadingDialog.h"
#include "../ui/AllAnnotationsDialog.h"
#include "../features/AmbientSoundService.h"
#include "../features/ImageCropService.h"
#include "../ui/ReadingRulerWidget.h"
#include "../ui/TextSelectionMenuWidget.h"
#include "../ui/DictionaryDialog.h"
#include "../ui/AnnotationExportDialog.h"
#include "../ui/QuickNoteDialog.h"
#include "../ui/EyeRestDialog.h"
#include "../ui/FootnotePopoverWidget.h"
#include <QDesktopServices>
#include <QUrl>
#include "../features/EyeStrainTimerService.h"
#include "../features/AnnotationManager.h"
#include "../db/DatabaseManager.h"
#include <QScrollBar>
#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFontMetrics>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <cmath>

namespace LunaRead {

ReaderViewport::ReaderViewport(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    m_tokens = ReaderThemeTokens::parchment();

    m_gestures = new GestureController(this);
    m_shortcuts = new DesktopShortcuts(this);
    installEventFilter(m_shortcuts);

    m_scrollRenderer = new ContinuousScrollRenderer(this);
    m_scrollRenderer->hide();

    m_dualPageRenderer = new DualPageRenderer(this);
    m_dualPageRenderer->hide();
    connect(m_dualPageRenderer, &DualPageRenderer::pageTurnRequested, this, [this](int delta) {
        if (delta > 0) {
            for (int i = 0; i < delta; ++i) nextPage();
        } else if (delta < 0) {
            for (int i = 0; i < -delta; ++i) prevPage();
        }
    });
    connect(m_dualPageRenderer, &DualPageRenderer::menuToggleRequested, this, &ReaderViewport::toggleToolbar);
    connect(m_dualPageRenderer, &DualPageRenderer::selectionMade, this, [this](const QString& text, const QRectF& normRect, int chIdx, int pgIdx, const QPoint& globalPos) {
        m_pendingNormalizedRect = normRect;
        m_pendingChapterIndex = chIdx;
        m_pendingPageIndex = pgIdx;
        if (m_isToolbarVisible) toggleToolbar(); // Moon+ hides chrome while selecting
        m_editingAnnotationId.clear();
        m_selectionMenu->clearEditingAnnotation();
        m_selectionMenu->setSelectedText(text);
        QPoint localPos = mapFromGlobal(globalPos);
        m_selectionMenu->showAt(localPos, rect());
    });
    connect(m_dualPageRenderer, &DualPageRenderer::annotationTapped, this, [this](const QString& annotationId, const QPoint& globalPos) {
        auto ann = DatabaseManager::instance().getAnnotationById(annotationId);
        if (ann) openHighlightMenu(*ann, globalPos);
    });

    m_curlAnimation = new QVariantAnimation(this);
    m_curlAnimation->setDuration(320);
    connect(m_curlAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_curlProgress = val.toReal();
        update();
    });

    // 1. Build Header Toolbar
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(48);
    auto* hLayout = new QHBoxLayout(m_headerBar);
    hLayout->setContentsMargins(16, 0, 16, 0);
    hLayout->setSpacing(8);

    auto* backBtn = new QPushButton(tr("← Shelf"), m_headerBar);
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        flushReadingSession();
        emit closeRequested();
    });
    hLayout->addWidget(backBtn);

    auto* tocBtn = new QPushButton(tr("☰ Contents"), m_headerBar);
    connect(tocBtn, &QPushButton::clicked, this, &ReaderViewport::openToc);
    hLayout->addWidget(tocBtn);

    auto* searchBtn = new QPushButton(tr("🔍 Search"), m_headerBar);
    connect(searchBtn, &QPushButton::clicked, this, &ReaderViewport::openSearch);
    hLayout->addWidget(searchBtn);

    m_titleLabel = new QLabel(m_headerBar);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    hLayout->addWidget(m_titleLabel, 1);

    auto* fontBtn = new QPushButton(tr("Aa Typography"), m_headerBar);
    connect(fontBtn, &QPushButton::clicked, this, &ReaderViewport::openFontSettings);
    hLayout->addWidget(fontBtn);

    auto* ttsBtn = new QPushButton(tr("🔊 TTS"), m_headerBar);
    connect(ttsBtn, &QPushButton::clicked, this, &ReaderViewport::toggleTts);
    hLayout->addWidget(ttsBtn);

    auto* themeBtn = new QPushButton(tr("🎨 Theme"), m_headerBar);
    connect(themeBtn, &QPushButton::clicked, this, &ReaderViewport::toggleTheme);
    hLayout->addWidget(themeBtn);

    auto* scrollBtn = new QPushButton(tr("⏳ Scroll"), m_headerBar);
    connect(scrollBtn, &QPushButton::clicked, this, &ReaderViewport::toggleAutoScroll);
    hLayout->addWidget(scrollBtn);

    auto* rulerBtn = new QPushButton(tr("📏 Ruler"), m_headerBar);
    connect(rulerBtn, &QPushButton::clicked, this, &ReaderViewport::toggleReadingRuler);
    hLayout->addWidget(rulerBtn);

    auto* nightBtn = new QPushButton(tr("🌙 Night"), m_headerBar);
    connect(nightBtn, &QPushButton::clicked, this, &ReaderViewport::showNightFilterControls);
    hLayout->addWidget(nightBtn);

    auto* speedBtn = new QPushButton(tr("⚡ RSVP"), m_headerBar);
    speedBtn->setToolTip(tr("⚡ Speed Reader (Rapid Serial Visual Presentation)"));
    connect(speedBtn, &QPushButton::clicked, this, &ReaderViewport::openSpeedReader);
    hLayout->addWidget(speedBtn);

    auto* cropBtn = new QPushButton(tr("✂ Crop"), m_headerBar);
    cropBtn->setToolTip(tr("Auto Crop White Margins (Expands book text to fill screen)"));
    cropBtn->setCheckable(true);
    cropBtn->setChecked(ImageCropService::instance().isAutoCropEnabled());
    connect(cropBtn, &QPushButton::toggled, this, [this](bool checked) {
        ImageCropService::instance().setAutoCropEnabled(checked);
        repaginate();
        update();
    });
    hLayout->addWidget(cropBtn);

    auto* fitBtn = new QPushButton(tr("↔ Fit Width"), m_headerBar);
    fitBtn->setToolTip(tr("Toggle Fit Width / Fit Page (Expands page to fill screen width)"));
    m_fitBtn = fitBtn;
    connect(fitBtn, &QPushButton::clicked, this, [this]() {
        const bool toWidth = m_typesetter.fitMode() == Typesetter::PageFitMode::FitPage;
        m_typesetter.setFitMode(toWidth ? Typesetter::PageFitMode::FitWidth
                                        : Typesetter::PageFitMode::FitPage);
        // Fit Width is a single-page zoom; inside a spread the page is clamped
        // to a half-screen slot, which would hide the width-filled zoom.
        if (toWidth && (m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl)) {
            resetImagePan();
            setReadingMode(ReadingMode::PageCurl);
        }
        resetImagePan();
        updateFitButtonLabel();
        repaginate();
        update();
    });
    hLayout->addWidget(fitBtn);

    auto* dualBtn = new QPushButton(tr("📖 Dual"), m_headerBar);
    dualBtn->setToolTip(tr("Dual Page Spread (Side-by-side reading for wide screens)"));
    dualBtn->setCheckable(true);
    dualBtn->setChecked(m_readingMode == ReadingMode::DualPage);
    connect(dualBtn, &QPushButton::toggled, this, [this](bool checked) {
        setReadingMode(checked ? ReadingMode::DualPage : ReadingMode::PageCurl);
    });
    hLayout->addWidget(dualBtn);

    auto* fsBtn = new QPushButton(tr("⛶ Fullscreen"), m_headerBar);
    fsBtn->setToolTip(tr("Toggle Fullscreen Reading Mode (F11)"));
    connect(fsBtn, &QPushButton::clicked, this, &ReaderViewport::fullscreenRequested);
    hLayout->addWidget(fsBtn);

    m_headerBar->hide();

    // 2. Build Footer Toolbar
    m_footerBar = new QWidget(this);
    m_footerBar->setFixedHeight(50);
    auto* fLayout = new QHBoxLayout(m_footerBar);
    fLayout->setContentsMargins(16, 0, 16, 0);

    m_progressSlider = new QSlider(Qt::Horizontal, m_footerBar);
    m_progressSlider->setRange(0, 100);
    connect(m_progressSlider, &QSlider::sliderMoved, this, [this](int value) {
        if (m_doc.totalChapters > 0) {
            int targetCh = (value * m_doc.totalChapters) / 100;
            jumpToChapter(std::clamp(targetCh, 0, m_doc.totalChapters - 1));
        }
    });
    fLayout->addWidget(m_progressSlider, 1);

    m_pageInfoLabel = new QLabel(m_footerBar);
    fLayout->addWidget(m_pageInfoLabel);
    m_footerBar->hide();

    // 3. Navigation Drawer & TTS HUD
    m_tocDrawer = new TocDrawerWidget(this);
    m_tocDrawer->hide();
    connect(m_tocDrawer, &TocDrawerWidget::chapterSelected, this, &ReaderViewport::jumpToChapter);
    connect(m_tocDrawer, &TocDrawerWidget::pageSelected, this, &ReaderViewport::jumpToPage);
    connect(m_tocDrawer, &TocDrawerWidget::annotationSelected, this, [this](const QString& id) {
        auto anns = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
        for (const auto& a : anns) {
            if (a.id == id) {
                jumpToChapter(a.chapterIndex);
                jumpToPage(a.pageIndex);
                if (m_tocDrawer->isOpen()) m_tocDrawer->closeDrawer();
                break;
            }
        }
    });
    connect(m_tocDrawer, &TocDrawerWidget::annotationDeleted, this, [this](const QString& id) {
        DatabaseManager::instance().deleteAnnotation(id);
        repaginate();
    });
    connect(m_tocDrawer, &TocDrawerWidget::exportAnnotationsRequested, this, &ReaderViewport::exportAnnotations);

    m_ttsService = new TtsService(this);
    m_ttsBar = new TtsControlBar(m_ttsService, this);
    m_ttsBar->hide();
    connect(m_ttsBar, &TtsControlBar::sentenceHighlighted, this, [this](int /*index*/, const QString& sentence) {
        m_activeTtsSentence = sentence;
        if (m_dualPageRenderer) m_dualPageRenderer->setActiveSentence(sentence);
        update();
    });
    connect(m_ttsBar, &TtsControlBar::closeRequested, this, [this]() {
        m_ttsBar->hide();
    });

    m_quickNav = new QuickNavPopoverWidget(this);
    m_quickNav->hide();
    connect(m_quickNav, &QuickNavPopoverWidget::pageJumpRequested, this, &ReaderViewport::jumpToPage);
    connect(m_quickNav, &QuickNavPopoverWidget::chapterJumpRequested, this, &ReaderViewport::jumpToChapter);
    connect(m_quickNav, &QuickNavPopoverWidget::closeRequested, this, [this]() {
        m_quickNav->hide();
    });

    // 4. Connect shortcuts
    connect(m_shortcuts, &DesktopShortcuts::nextPageRequested, this, &ReaderViewport::nextPage);
    connect(m_shortcuts, &DesktopShortcuts::prevPageRequested, this, &ReaderViewport::prevPage);
    connect(m_shortcuts, &DesktopShortcuts::toggleThemeRequested, this, &ReaderViewport::toggleTheme);
    connect(m_shortcuts, &DesktopShortcuts::searchRequested, this, &ReaderViewport::openSearch);
    connect(m_shortcuts, &DesktopShortcuts::openTocRequested, this, &ReaderViewport::openToc);
    connect(m_shortcuts, &DesktopShortcuts::fontSettingsRequested, this, &ReaderViewport::openFontSettings);
    connect(m_shortcuts, &DesktopShortcuts::toggleTtsRequested, this, &ReaderViewport::toggleTts);
    connect(m_shortcuts, &DesktopShortcuts::toggleAutoScrollRequested, this, &ReaderViewport::toggleAutoScroll);
    connect(m_shortcuts, &DesktopShortcuts::toggleReadingRulerRequested, this, &ReaderViewport::toggleReadingRuler);
    connect(m_shortcuts, &DesktopShortcuts::toggleNightFilterRequested, this, &ReaderViewport::toggleNightFilter);
    connect(m_shortcuts, &DesktopShortcuts::nextChapterRequested, this, [this]() {
        if (m_currentChapter + 1 < m_doc.chapters.size()) {
            jumpToChapter(m_currentChapter + 1);
        }
    });
    connect(m_shortcuts, &DesktopShortcuts::prevChapterRequested, this, [this]() {
        if (m_currentChapter > 0) {
            jumpToChapter(m_currentChapter - 1);
        }
    });
    connect(m_shortcuts, &DesktopShortcuts::fullscreenToggleRequested, this, &ReaderViewport::fullscreenRequested);
    connect(m_shortcuts, &DesktopShortcuts::escapeRequested, this, [this]() {
        if (m_selectionMenu && m_selectionMenu->isVisible()) {
            m_selectionMenu->dismiss();
            m_selectionRect = QRectF();
            update();
        } else if (m_tocDrawer->isOpen()) {
            m_tocDrawer->closeDrawer();
        } else if (m_isToolbarVisible) {
            toggleToolbar();
        } else if (window()->isFullScreen()) {
            emit fullscreenRequested();
        } else {
            flushReadingSession();
            emit closeRequested();
        }
    });

    updateHudStyle();

    // 5. Dog-Ear Bookmark
    m_dogEarBookmark = new DogEarBookmarkWidget(this);
    connect(m_dogEarBookmark, &DogEarBookmarkWidget::bookmarkToggled, this, [this](bool bookmarked) {
        if (m_doc.id.isEmpty()) return;
        if (bookmarked) {
            AnnotationRecord ann;
            ann.bookId = m_doc.id;
            ann.type = QStringLiteral("bookmark");
            ann.chapterIndex = m_currentChapter;
            ann.pageIndex = m_currentPageInChapter;
            ann.selectedText = QString("Chapter %1, Page %2").arg(m_currentChapter + 1).arg(m_currentPageInChapter + 1);
            DatabaseManager::instance().insertAnnotation(ann);
            if (m_tocDrawer) m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(m_doc.id));
        } else {
            auto list = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
            for (const auto& a : list) {
                if (a.type == "bookmark" && a.chapterIndex == m_currentChapter && a.pageIndex == m_currentPageInChapter) {
                    DatabaseManager::instance().deleteAnnotation(a.id);
                }
            }
            if (m_tocDrawer) m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(m_doc.id));
        }
    });

    // 6. Reading Ruler
    m_readingRuler = new ReadingRulerWidget(this);
    m_readingRuler->hide();

    // 7. Auto-Scroll Overlay
    m_autoScrollOverlay = new AutoScrollOverlayWidget(this);
    m_autoScrollOverlay->hide();
    connect(m_autoScrollOverlay, &AutoScrollOverlayWidget::scrollStepRequested, this, [this](int delta) {
        if (m_readingMode == ReadingMode::ContinuousScroll) {
            m_scrollRenderer->verticalScrollBar()->setValue(m_scrollRenderer->verticalScrollBar()->value() + delta);
        } else {
            static int accumulator = 0;
            accumulator += delta;
            if (accumulator > 500) {
                accumulator = 0;
                nextPage();
            }
        }
    });
    connect(m_autoScrollOverlay, &AutoScrollOverlayWidget::pageTurnRequested, this, [this](int delta) {
        if (delta > 0) nextPage();
        else prevPage();
    });

    // 7b. Footnote Popover & Return Jump
    m_footnotePopover = new FootnotePopoverWidget(this);
    m_footnotePopover->hide();
    connect(m_footnotePopover, &FootnotePopoverWidget::jumpRequested, this, [this](int chIdx, int pgIdx) {
        m_lastJumpLocation = qMakePair(m_currentChapter, m_currentPageInChapter);
        jumpToChapter(chIdx);
        if (pgIdx >= 0) jumpToPage(pgIdx);
        if (m_returnJumpBtn) {
            m_returnJumpBtn->show();
            m_returnJumpBtn->raise();
        }
    });

    m_returnJumpBtn = new QPushButton(tr("↩ Return to Previous Page"), this);
    m_returnJumpBtn->setCursor(Qt::PointingHandCursor);
    m_returnJumpBtn->setStyleSheet(
        "QPushButton { background-color: rgba(30, 41, 59, 0.9); color: white; border: 1px solid rgba(255, 255, 255, 0.2); "
        "border-radius: 18px; padding: 6px 16px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: rgba(59, 130, 246, 0.9); }"
    );
    m_returnJumpBtn->hide();
    connect(m_returnJumpBtn, &QPushButton::clicked, this, &ReaderViewport::returnToPreviousLocation);

    // 8. Text Selection Menu
    m_selectionMenu = new TextSelectionMenuWidget(this);
    m_selectionMenu->hide();
    connect(m_selectionMenu, &TextSelectionMenuWidget::highlightRequested, this, [this](const QString& text, const QColor& color) {
        if (m_doc.id.isEmpty()) return;

        if (!m_editingAnnotationId.isEmpty()) {
            // Moon+ Reader: picking a color on an existing highlight recolors it
            if (auto existing = DatabaseManager::instance().getAnnotationById(m_editingAnnotationId)) {
                AnnotationRecord updated = *existing;
                updated.color = color;
                updated.updatedAt = QDateTime::currentDateTime();
                DatabaseManager::instance().updateAnnotation(updated);
            }
            m_editingAnnotationId.clear();
            m_selectionRect = QRectF();
            m_pendingNormalizedRect = QRectF();
            refreshAnnotations();
            repaginate();
            update();
            return;
        }

        AnnotationRecord ann;
        ann.bookId = m_doc.id;
        ann.type = QStringLiteral("highlight");
        ann.chapterIndex = m_pendingChapterIndex;
        ann.pageIndex = m_pendingPageIndex;
        ann.selectedText = text;
        ann.color = color;
        ann.normalizedRect = m_pendingNormalizedRect;
        DatabaseManager::instance().insertAnnotation(ann);
        if (m_tocDrawer) m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(m_doc.id));
        m_selectionRect = QRectF();
        m_pendingNormalizedRect = QRectF();
        repaginate();
        update();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::deleteRequested, this, [this](const QString& annotationId) {
        if (annotationId.isEmpty()) return;
        DatabaseManager::instance().deleteAnnotation(annotationId);
        m_editingAnnotationId.clear();
        refreshAnnotations();
        repaginate();
        update();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::dismissed, this, [this]() {
        m_editingAnnotationId.clear();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::defineRequested, this, [this](const QString& text) {
        auto* dlg = new DictionaryDialog(text, new DictionaryService(this), this);
        dlg->setBookContext(m_doc.id, m_doc.title);
        connect(dlg, &DictionaryDialog::pronounceRequested, this, [this](const QString& word) {
            if (m_ttsService) {
                m_ttsService->speakText(word);
            }
        });
        dlg->show();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::addNoteRequested, this, [this](const QString& text) {
        auto* dlg = new QuickNoteDialog(text, this);
        connect(dlg, &QuickNoteDialog::noteSaved, this, [this, text](const QString& note, const QColor& col, const QString& /*tags*/) {
            if (m_doc.id.isEmpty()) return;
            AnnotationRecord ann;
            ann.bookId = m_doc.id;
            ann.type = QStringLiteral("note");
            ann.chapterIndex = m_pendingChapterIndex;
            ann.pageIndex = m_pendingPageIndex;
            ann.selectedText = text;
            ann.note = note;
            ann.color = col;
            ann.normalizedRect = m_pendingNormalizedRect;
            DatabaseManager::instance().insertAnnotation(ann);
            if (m_tocDrawer) m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(m_doc.id));
            m_selectionRect = QRectF();
            m_pendingNormalizedRect = QRectF();
            repaginate();
            update();
        });
        dlg->show();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::underlineRequested, this, [this](const QString& text) {
        if (m_doc.id.isEmpty()) return;
        AnnotationRecord ann;
        ann.bookId = m_doc.id;
        ann.type = QStringLiteral("underline");
        ann.chapterIndex = m_pendingChapterIndex;
        ann.pageIndex = m_pendingPageIndex;
        ann.selectedText = text;
        ann.color = QColor(33, 150, 243); // Material Blue
        ann.normalizedRect = m_pendingNormalizedRect;
        DatabaseManager::instance().insertAnnotation(ann);
        if (m_tocDrawer) m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(m_doc.id));
        m_selectionRect = QRectF();
        m_pendingNormalizedRect = QRectF();
        repaginate();
        update();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::strikethroughRequested, this, [this](const QString& text) {
        if (m_doc.id.isEmpty()) return;
        AnnotationRecord ann;
        ann.bookId = m_doc.id;
        ann.type = QStringLiteral("strikethrough");
        ann.chapterIndex = m_pendingChapterIndex;
        ann.pageIndex = m_pendingPageIndex;
        ann.selectedText = text;
        ann.color = QColor(244, 67, 54); // Material Red
        ann.normalizedRect = m_pendingNormalizedRect;
        DatabaseManager::instance().insertAnnotation(ann);
        if (m_tocDrawer) m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(m_doc.id));
        m_selectionRect = QRectF();
        m_pendingNormalizedRect = QRectF();
        repaginate();
        update();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::speakRequested, this, [this](const QString& text) {
        if (m_ttsService) {
            m_ttsService->speakText(text);
        }
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::searchRequested, this, [this](const QString& text) {
        auto* dlg = new SearchInBookDialog(m_doc, this);
        dlg->applyTheme(m_tokens);
        dlg->setSearchText(text);
        connect(dlg, &SearchInBookDialog::matchSelected, this, [this, dlg](int chIdx, int offset, int) {
            m_activeTtsSentence = dlg->searchTerm();
            if (m_dualPageRenderer) m_dualPageRenderer->setActiveSentence(m_activeTtsSentence);
            jumpToChapter(chIdx);
            for (int p = 0; p < m_pages.size(); ++p) {
                if (offset >= m_pages[p].characterStartIndex && offset <= m_pages[p].characterEndIndex) {
                    jumpToPage(p);
                    break;
                }
            }
            update();
        });
        dlg->show();
    });
    connect(m_selectionMenu, &TextSelectionMenuWidget::translateRequested, this, [](const QString& text) {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://translate.google.com/?sl=auto&tl=en&text=%1&op=translate")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(text)))));
    });

    // 9. Eye Strain Reminder Service
    m_eyeTimer = new EyeStrainTimerService(this);
    connect(m_eyeTimer, &EyeStrainTimerService::eyeRestRequired, this, [this](int minutes) {
        auto* dlg = new EyeRestDialog(minutes, this);
        dlg->show();
    });
    m_eyeTimer->start();

    // 10. Night Dimmer & Blue Light Filter Overlay
    m_nightFilter = new NightFilterOverlayWidget(this);
    m_nightFilter->setGeometry(rect());
    m_nightFilter->raise();

    // 11. Reading Session Analytics
    m_sessionTimer = new QTimer(this);
    m_sessionTimer->setInterval(30000); // Record progress every 30s
    connect(m_sessionTimer, &QTimer::timeout, this, &ReaderViewport::flushReadingSession);
    m_sessionTimer->start();
}

ReaderViewport::~ReaderViewport() {
    flushReadingSession();
}

void ReaderViewport::flushReadingSession() {
    if (m_doc.id.isEmpty() || m_sessionStartTime <= 0) return;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int durationSecs = static_cast<int>((now - m_sessionStartTime) / 1000);
    if (durationSecs >= 3) {
        DatabaseManager::instance().recordReadingSession(
            m_doc.id, m_sessionStartTime, now, durationSecs, m_sessionWordsRead, m_sessionPagesTurned
        );
        DatabaseManager::instance().recordReadingActivity(
            QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")),
            durationSecs, m_sessionPagesTurned
        );
    }
    m_sessionStartTime = now;
    m_sessionPagesTurned = 0;
    m_sessionWordsRead = 0;
}

void ReaderViewport::loadDocument(const BookDocument& doc, int initialChapter, int initialPage) {
    flushReadingSession();
    m_sessionStartTime = QDateTime::currentMSecsSinceEpoch();
    m_sessionPagesTurned = 0;
    m_sessionWordsRead = 0;

    m_doc = doc;
    m_currentChapter = std::clamp(initialChapter, 0, std::max(0, static_cast<int>(doc.chapters.size()) - 1));
    m_currentPageInChapter = std::max(0, initialPage);
    m_titleLabel->setText(doc.title);
    m_tocDrawer->setToc(doc.toc);

    // Fixed-layout pages fill the screen width, kept inside a reading gutter so
    // the text never touches the window edges; the page pans vertically.
    m_typesetter.setFitMode(isFixedLayoutDocument() ? Typesetter::PageFitMode::FitWidth
                                                    : Typesetter::PageFitMode::FitPage);
    updateFitButtonLabel();
    resetImagePan();
    if (!doc.id.isEmpty()) {
        m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(doc.id));
    }

    // Lazy TTS text loading: avoid blocking UI when opening book.
    // m_ttsService->loadText() will be called when TTS is explicitly played.

    // Only configure scroll renderer if current reading mode is continuous scroll
    if (m_readingMode == ReadingMode::ContinuousScroll || m_readingMode == ReadingMode::WebtoonVertical) {
        m_scrollRenderer->setDocument(doc, m_tokens);
    }
    m_dualPageRenderer->setTypesetter(&m_typesetter);
    m_dualPageRenderer->setBookInfo(doc.title, doc.chapters.isEmpty() ? QString() : doc.chapters[0].title);
    m_dualPageRenderer->setBookId(doc.id);
    m_dualPageRenderer->setFilePath(doc.filePath);

    if (doc.format.compare("cbz", Qt::CaseInsensitive) == 0 ||
        doc.format.compare("cbr", Qt::CaseInsensitive) == 0) {
        if (doc.title.contains("webtoon", Qt::CaseInsensitive) || doc.title.contains("manhwa", Qt::CaseInsensitive)) {
            setReadingMode(ReadingMode::WebtoonVertical);
        } else {
            setReadingMode(ReadingMode::MangaRtl);
        }
    } else if (isFixedLayoutDocument()) {
        setReadingMode(ReadingMode::PageCurl);
    } else if (width() > 1000 && width() > height() * 1.25) {
        setReadingMode(ReadingMode::DualPage);
    }

    if (m_quickNav) {
        QVector<QString> chTitles;
        for (const auto& ch : doc.chapters) chTitles.append(ch.title);
        m_quickNav->setChapters(chTitles, m_currentChapter);
        m_quickNav->setBookTitle(doc.title);
    }

    repaginate();
}

void ReaderViewport::setReadingMode(ReadingMode mode) {
    m_readingMode = mode;
    bool isScroll = (mode == ReadingMode::ContinuousScroll || mode == ReadingMode::WebtoonVertical);
    bool isDual = (mode == ReadingMode::DualPage || mode == ReadingMode::MangaRtl);

    m_scrollRenderer->setVisible(isScroll);
    if (isScroll) {
        m_scrollRenderer->setGeometry(rect());
        m_scrollRenderer->setWebtoonMode(mode == ReadingMode::WebtoonVertical);
        m_scrollRenderer->raise();
    }

    m_dualPageRenderer->setVisible(isDual);
    if (isDual) {
        m_dualPageRenderer->setGeometry(rect());
        m_dualPageRenderer->setRightToLeft(mode == ReadingMode::MangaRtl);
        m_dualPageRenderer->raise();
    }

    m_headerBar->raise();
    m_footerBar->raise();
    if (m_tocDrawer) m_tocDrawer->raise();
    if (m_dogEarBookmark) m_dogEarBookmark->raise();

    repaginate();
    update();
}

void ReaderViewport::setMangaMode(bool enabled) {
    setReadingMode(enabled ? ReadingMode::MangaRtl : ReadingMode::DualPage);
}

void ReaderViewport::setWebtoonMode(bool enabled) {
    setReadingMode(enabled ? ReadingMode::WebtoonVertical : ReadingMode::ContinuousScroll);
}

void ReaderViewport::toggleAutoCropMargins() {
    auto& crop = ImageCropService::instance();
    crop.setAutoCropEnabled(!crop.isAutoCropEnabled());
    repaginate();
    update();
}

void ReaderViewport::setThemeMode(ReaderThemeMode theme) {
    m_tokens = ReaderThemeTokens::fromMode(theme);
    m_typesetter.invalidateCache();
    m_scrollRenderer->updateTheme(m_tokens);
    m_dualPageRenderer->updateTheme(m_tokens);
    m_tocDrawer->applyTheme(m_tokens);
    m_ttsBar->updateTheme(m_tokens);
    updateHudStyle();
    repaginate();
}

void ReaderViewport::toggleTheme() {
    if (m_tokens.id == "parchment") setThemeMode(ReaderThemeMode::OledBlack);
    else if (m_tokens.id == "oledBlack") setThemeMode(ReaderThemeMode::Sepia);
    else if (m_tokens.id == "sepia") setThemeMode(ReaderThemeMode::Night);
    else if (m_tokens.id == "night") setThemeMode(ReaderThemeMode::ModernLight);
    else setThemeMode(ReaderThemeMode::Parchment);
}

void ReaderViewport::toggleBookmark() {
    if (m_dogEarBookmark) {
        m_dogEarBookmark->setBookmarked(!m_dogEarBookmark->isBookmarked());
    }
}

void ReaderViewport::toggleAutoScroll() {
    if (m_autoScrollOverlay) {
        if (m_autoScrollOverlay->isVisible()) {
            m_autoScrollOverlay->hide();
        } else {
            m_autoScrollOverlay->show();
            m_autoScrollOverlay->raise();
        }
    }
}

void ReaderViewport::toggleReadingRuler() {
    if (m_readingRuler) {
        m_readingRuler->toggleRuler();
        m_readingRuler->raise();
    }
}

void ReaderViewport::toggleNightFilter() {
    if (m_nightFilter) {
        m_nightFilter->toggleFilter();
    }
}

void ReaderViewport::showNightFilterControls() {
    if (m_nightFilter) {
        m_nightFilter->toggleControls();
    }
}

void ReaderViewport::toggleQuickNav() {
    if (m_quickNav) {
        m_quickNav->togglePopover(this);
    }
}

void ReaderViewport::openTapZoneConfig() {
    auto* dlg = new TapZoneConfigDialog(m_gestures, this);
    dlg->show();
}

void ReaderViewport::openSearch() {
    auto* dlg = new SearchInBookDialog(m_doc, this);
    dlg->applyTheme(m_tokens);
    connect(dlg, &SearchInBookDialog::matchSelected, this, [this, dlg](int chIdx, int offset, int /*len*/) {
        m_activeTtsSentence = dlg->searchTerm();
        if (m_dualPageRenderer) m_dualPageRenderer->setActiveSentence(m_activeTtsSentence);
        jumpToChapter(chIdx);
        for (int p = 0; p < m_pages.size(); ++p) {
            if (offset >= m_pages[p].characterStartIndex && offset <= m_pages[p].characterEndIndex) {
                jumpToPage(p);
                break;
            }
        }
        update();
    });
    dlg->show();
}

void ReaderViewport::openToc() {
    m_tocDrawer->setCurrentReadingPosition(m_currentChapter, m_currentPageInChapter);
    if (!m_doc.id.isEmpty()) {
        m_tocDrawer->setAnnotations(DatabaseManager::instance().getAnnotationsForBook(m_doc.id));
    }
    m_tocDrawer->toggleDrawer();
}

void ReaderViewport::exportAnnotations() {
    if (m_doc.id.isEmpty()) return;
    auto anns = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
    auto* dlg = new AnnotationExportDialog(m_doc.title, anns, this);
    dlg->applyTheme(m_tokens);
    dlg->show();
}

void ReaderViewport::openBookExporter() {
    auto* dlg = new BookExporterDialog(m_doc, m_currentChapter, this);
    dlg->applyTheme(m_tokens);
    dlg->show();
}

void ReaderViewport::openAllAnnotations() {
    auto* dlg = new AllAnnotationsDialog(m_doc.id, m_doc.title, this);
    dlg->applyTheme(m_tokens);
    connect(dlg, &AllAnnotationsDialog::jumpToAnnotationRequested, this, [this](int ch, int pg) {
        jumpToChapter(ch);
        jumpToPage(pg);
    });
    connect(dlg, &AllAnnotationsDialog::annotationChanged, this, [this]() {
        repaginate();
    });
    dlg->exec();
}

void ReaderViewport::openSpeedReader() {
    QString textToRead;
    if (m_currentChapter < m_doc.chapters.size()) {
        textToRead = m_doc.chapters[m_currentChapter].textContent;
    }
    if (textToRead.trimmed().isEmpty()) {
        for (const auto& ch : m_doc.chapters) {
            textToRead.append(ch.textContent).append("\n\n");
        }
    }
    auto* dlg = new SpeedReadingDialog(textToRead, 350, this);
    dlg->applyTheme(m_tokens);
    dlg->exec();
}

void ReaderViewport::openFontSettings() {
    auto* dlg = new FontSettingsDialog(this);
    connect(dlg, &FontSettingsDialog::fontChanged, this, [this](const QFont& f) {
        m_typesetter.setFont(f);
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::lineHeightChanged, this, [this](qreal lh) {
        m_typesetter.setLineHeight(lh);
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::marginsChanged, this, [this](qreal h, qreal v) {
        m_typesetter.setMargins(QMarginsF(h, v, h, v));
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::paragraphIndentChanged, this, [this](qreal indent) {
        m_typesetter.setParagraphIndent(indent);
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::dropCapsChanged, this, [this](bool enabled) {
        m_typesetter.setDropCapsEnabled(enabled);
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::alignmentChanged, this, [this](Qt::Alignment align) {
        m_typesetter.setAlignment(align);
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::themeChanged, this, &ReaderViewport::setThemeMode);
    dlg->show();
}

void ReaderViewport::toggleTts() {
    if (m_ttsBar->isVisible()) {
        m_ttsBar->hide();
    } else {
        if (m_ttsService && m_ttsService->sentenceCount() == 0) {
            QString fullText;
            for (const auto& ch : m_doc.chapters) {
                fullText.append(ch.textContent).append("\n\n");
            }
            m_ttsService->loadText(fullText);
        }
        m_ttsBar->show();
        m_ttsBar->reposition(size());
        m_ttsBar->play();
    }
}

void ReaderViewport::jumpToChapter(int chapterIndex) {
    if (chapterIndex >= 0 && chapterIndex < m_doc.chapters.size()) {
        m_currentChapter = chapterIndex;
        m_currentPageInChapter = 0;
        resetImagePan();
        repaginate();
    }
}

void ReaderViewport::jumpToPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < m_pages.size()) {
        m_currentPageInChapter = pageIndex;
        resetImagePan();
        update();
    }
}

void ReaderViewport::updateHudStyle() {
    QString barStyle = QString("background-color: %1; border: 1px solid %2; border-radius: 8px; color: %3;")
        .arg(m_tokens.backgroundColor.name())
        .arg(m_tokens.dividerColor.name())
        .arg(m_tokens.textColor.name());

    m_headerBar->setStyleSheet(barStyle);
    m_footerBar->setStyleSheet(barStyle);
}

bool ReaderViewport::isFixedLayoutDocument() const {
    return m_doc.format.compare("pdf", Qt::CaseInsensitive) == 0;
}

void ReaderViewport::resetImagePan() {
    m_imagePanY = 0.0;
    m_typesetter.setImagePanY(0.0);
}

void ReaderViewport::updateFitButtonLabel() {
    if (!m_fitBtn) return;
    const bool fitWidth = m_typesetter.fitMode() == Typesetter::PageFitMode::FitWidth;
    m_fitBtn->setText(fitWidth ? tr("↕ Fit Page") : tr("↔ Fit Width"));
    m_fitBtn->setToolTip(fitWidth
        ? tr("Currently fitting the full screen width. Click to show the whole page.")
        : tr("Currently showing the whole page. Click to fit the full screen width."));
}

bool ReaderViewport::panImagePageBy(qreal deltaPixels) {
    if (m_pages.isEmpty() || m_currentPageInChapter >= m_pages.size()) return false;
    if (m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl) return false;
    if (m_readingMode == ReadingMode::ContinuousScroll || m_readingMode == ReadingMode::WebtoonVertical) return false;

    const qreal range = m_typesetter.imagePanRange(m_pages[m_currentPageInChapter], rect());
    if (range <= 0.0) return false;

    const qreal next = std::clamp(m_imagePanY + deltaPixels, 0.0, range);
    if (qFuzzyCompare(next + 1.0, m_imagePanY + 1.0)) return false;

    m_imagePanY = next;
    m_typesetter.setImagePanY(m_imagePanY);
    update();
    return true;
}

void ReaderViewport::repaginate() {
    if (m_doc.chapters.isEmpty()) return;

    if (m_currentChapter >= 0 && m_currentChapter < m_doc.chapters.size()) {
        auto& ch = m_doc.chapters[m_currentChapter];
        if (ch.isImagePage && ch.imageBytes.isEmpty() && m_doc.format.compare("pdf", Qt::CaseInsensitive) == 0) {
            // Rasterizing the page blocks for seconds at high DPI, so it runs
            // off the UI thread and repaginates once the bytes arrive.
            requestPageImage(m_currentChapter);
        }

        // Prefetch upcoming scanned pages in the background (text pages need none)
        if (m_doc.format.compare("pdf", Qt::CaseInsensitive) == 0) {
            for (int offset = 1; offset <= 2; ++offset) {
                const int upcoming = m_currentChapter + offset;
                if (upcoming >= 0 && upcoming < m_doc.chapters.size() &&
                    m_doc.chapters[upcoming].isImagePage && m_doc.chapters[upcoming].imageBytes.isEmpty()) {
                    PdfPageService::instance().prefetchPage(m_doc.filePath, m_doc.id, upcoming + 1);
                }
            }
        }
    }

    const auto& ch = m_doc.chapters[m_currentChapter];
    m_pages = m_typesetter.paginate(ch, size(), m_currentChapter, m_tokens);

    if (m_pages.isEmpty()) {
        ParsedPage emptyPage;
        emptyPage.chapterIndex = m_currentChapter;
        emptyPage.text = ch.textContent;
        m_pages.append(emptyPage);
    }

    m_currentPageInChapter = std::clamp(m_currentPageInChapter, 0, static_cast<int>(m_pages.size()) - 1);

    // Keep the zoomed page inside its pannable range after resizes / page turns.
    if (m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl) {
        m_imagePanY = 0.0;
    } else if (!m_pages.isEmpty()) {
        const qreal panRange = m_typesetter.imagePanRange(m_pages[m_currentPageInChapter], rect());
        m_imagePanY = std::clamp(m_imagePanY, 0.0, panRange);
    } else {
        m_imagePanY = 0.0;
    }
    m_typesetter.setImagePanY(m_imagePanY);

    // Update footer info. Fixed-layout pages are one chapter per page, so the
    // meaningful position is the PDF page itself.
    QString info;
    if (isFixedLayoutDocument()) {
        const int percent = m_doc.totalChapters > 0
            ? static_cast<int>(std::lround(100.0 * (m_currentChapter + 1) / m_doc.totalChapters))
            : 0;
        info = tr("Page %1 of %2 • %3%").arg(m_currentChapter + 1).arg(m_doc.totalChapters).arg(percent);
    } else {
        info = QString("Ch %1/%2 • Page %3/%4")
            .arg(m_currentChapter + 1)
            .arg(m_doc.totalChapters)
            .arg(m_currentPageInChapter + 1)
            .arg(m_pages.size());
    }
    m_pageInfoLabel->setText(info);

    double progress = 0.0;
    if (m_doc.totalChapters > 0) {
        progress = (static_cast<double>(m_currentChapter) / m_doc.totalChapters) * 100.0;
        if (m_pages.size() > 1) {
            double pageFrac = (static_cast<double>(m_currentPageInChapter) / m_pages.size());
            progress += (pageFrac / m_doc.totalChapters) * 100.0;
        }
    }
    m_progressSlider->setValue(static_cast<int>(progress));
    emit progressUpdated(progress);

    if (!m_doc.id.isEmpty()) {
        DatabaseManager::instance().updateBookProgress(m_doc.id, m_currentChapter, m_currentPageInChapter, progress);
    }

    if ((m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl) && !m_pages.isEmpty()) {
        std::optional<ParsedPage> rightPage;
        if (m_currentPageInChapter + 1 < m_pages.size()) {
            rightPage = m_pages[m_currentPageInChapter + 1];
        } else if (m_currentChapter + 1 < m_doc.chapters.size()) {
            auto& nextCh = m_doc.chapters[m_currentChapter + 1];
            if (nextCh.isImagePage && nextCh.imageBytes.isEmpty() && m_doc.format.compare("pdf", Qt::CaseInsensitive) == 0) {
                requestPageImage(m_currentChapter + 1);
            }
            auto nextPages = m_typesetter.paginate(nextCh, size(), m_currentChapter + 1, m_tokens);
            if (!nextPages.isEmpty()) {
                rightPage = nextPages.first();
            }
        }
        m_dualPageRenderer->setBookInfo(m_doc.title, ch.title);
        m_dualPageRenderer->setBookId(m_doc.id);
        m_dualPageRenderer->setFilePath(m_doc.filePath);
        QVector<AnnotationRecord> dualAnns;
        if (!m_doc.id.isEmpty()) {
            auto list = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
            dualAnns = QVector<AnnotationRecord>(list.begin(), list.end());
        }
        m_dualPageRenderer->setAnnotations(dualAnns);
        m_dualPageRenderer->setPages(m_pages[m_currentPageInChapter], rightPage, m_tokens);
    }

    if (m_dogEarBookmark && !m_doc.id.isEmpty()) {
        auto anns = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
        bool hasBm = false;
        for (const auto& a : anns) {
            if (a.type == "bookmark" && a.chapterIndex == m_currentChapter && a.pageIndex == m_currentPageInChapter) {
                hasBm = true;
                break;
            }
        }
        m_dogEarBookmark->setBookmarked(hasBm);
    }

    if (m_quickNav) {
        m_quickNav->setChapterInfo(m_currentChapter, m_doc.totalChapters,
            (m_currentChapter < m_doc.chapters.size()) ? m_doc.chapters[m_currentChapter].title : QString());
        m_quickNav->setPageProgress(m_currentPageInChapter, static_cast<int>(m_pages.size()));
        int chapterWords = static_cast<int>(ch.textContent.split(QRegularExpression(R"(\s+)")).size());
        m_quickNav->setWordsRemaining(chapterWords, chapterWords * std::max(1, m_doc.totalChapters - m_currentChapter));
    }

    update();
}

void ReaderViewport::requestPageImage(int chapterIndex) {
    if (m_doc.format.compare("pdf", Qt::CaseInsensitive) != 0) return;
    if (chapterIndex < 0 || chapterIndex >= m_doc.chapters.size()) return;
    if (!m_doc.chapters[chapterIndex].imageBytes.isEmpty()) return;
    if (m_pendingPageLoads.contains(chapterIndex)) return;

    m_pendingPageLoads.insert(chapterIndex);
    const QString filePath = m_doc.filePath;
    const QString bookId = m_doc.id;
    const int pageNumber = chapterIndex + 1;
    QPointer<ReaderViewport> self(this);

    QThreadPool::globalInstance()->start([self, chapterIndex, pageNumber, filePath, bookId]() {
        const QByteArray bytes = PdfPageService::instance().getPageImage(filePath, bookId, pageNumber);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, chapterIndex, bytes]() {
            if (!self) return;
            self->m_pendingPageLoads.remove(chapterIndex);
            if (!bytes.isEmpty() && chapterIndex < self->m_doc.chapters.size()) {
                self->m_doc.chapters[chapterIndex].imageBytes = bytes;
                self->m_pageLoadAttempts.remove(chapterIndex);
                if (chapterIndex == self->m_currentChapter) {
                    self->repaginate();
                    self->update();
                }
                return;
            }

            // pdftoppm can still be finishing a heavy page; retry a couple of
            // times instead of leaving the placeholder on screen forever.
            const int attempts = self->m_pageLoadAttempts.value(chapterIndex, 0) + 1;
            self->m_pageLoadAttempts.insert(chapterIndex, attempts);
            if (attempts <= 2 && chapterIndex == self->m_currentChapter) {
                QTimer::singleShot(1500, self.data(), [self, chapterIndex]() {
                    if (!self || chapterIndex != self->m_currentChapter) return;
                    if (chapterIndex < self->m_doc.chapters.size() &&
                        self->m_doc.chapters[chapterIndex].imageBytes.isEmpty()) {
                        self->requestPageImage(chapterIndex);
                    }
                });
            }
        }, Qt::QueuedConnection);
    });
}

void ReaderViewport::nextPage() {
    AmbientSoundService::instance().playPageFlipSound();
    resetImagePan();
    m_sessionPagesTurned++;
    if (m_currentPageInChapter < m_pages.size()) {
        m_sessionWordsRead += static_cast<int>(m_pages[m_currentPageInChapter].text.split(QRegularExpression(R"(\s+)")).size());
    }

    int step = (m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl) ? 2 : 1;
    if (m_currentPageInChapter + step < m_pages.size()) {
        m_currentPageInChapter += step;
        repaginate();
    } else if (m_currentPageInChapter + 1 < m_pages.size()) {
        m_currentPageInChapter = static_cast<int>(m_pages.size()) - 1;
        repaginate();
    } else if ((m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl) && m_pages.size() <= 1 && m_currentChapter + step < m_doc.chapters.size()) {
        m_currentChapter += step;
        m_currentPageInChapter = 0;
        repaginate();
    } else if (m_currentChapter + 1 < m_doc.chapters.size()) {
        m_currentChapter++;
        m_currentPageInChapter = 0;
        repaginate();
    }
}

void ReaderViewport::prevPage() {
    AmbientSoundService::instance().playPageFlipSound();
    resetImagePan();
    m_sessionPagesTurned++;
    int step = (m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl) ? 2 : 1;
    if (m_currentPageInChapter >= step) {
        m_currentPageInChapter -= step;
        repaginate();
    } else if (m_currentPageInChapter > 0) {
        m_currentPageInChapter = 0;
        repaginate();
    } else if ((m_readingMode == ReadingMode::DualPage || m_readingMode == ReadingMode::MangaRtl) && m_pages.size() <= 1 && m_currentChapter >= step) {
        m_currentChapter -= step;
        m_currentPageInChapter = 0;
        repaginate();
    } else if (m_currentChapter > 0) {
        m_currentChapter--;
        m_currentPageInChapter = 999999;
        repaginate();
    }
}

void ReaderViewport::toggleToolbar() {
    m_isToolbarVisible = !m_isToolbarVisible;
    m_headerBar->setVisible(m_isToolbarVisible);
    m_footerBar->setVisible(m_isToolbarVisible);
}

void ReaderViewport::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_pages.isEmpty()) {
        painter.fillRect(rect(), m_tokens.backgroundColor);
        painter.setPen(m_tokens.textColor);
        painter.setFont(QFont("Roboto", 14));
        painter.drawText(rect(), Qt::AlignCenter, tr("No readable content found in this section.\nClick '← Shelf' above to return to your library."));
        return;
    }

    const ParsedPage& curPage = m_pages[m_currentPageInChapter];
    std::optional<ParsedPage> nextPage;
    if (m_currentPageInChapter + 1 < m_pages.size()) {
        nextPage = m_pages[m_currentPageInChapter + 1];
    } else if (m_currentChapter + 1 < m_doc.chapters.size()) {
        auto& nextCh = m_doc.chapters[m_currentChapter + 1];
        if (nextCh.isImagePage && nextCh.imageBytes.isEmpty() && m_doc.format.compare("pdf", Qt::CaseInsensitive) == 0) {
            requestPageImage(m_currentChapter + 1);
        }
        auto nextPages = m_typesetter.paginate(nextCh, size(), m_currentChapter + 1, m_tokens);
        if (!nextPages.isEmpty()) {
            nextPage = nextPages.first();
        }
    }

    QRectF bounds = rect();
    QString chTitle = (m_currentChapter < m_doc.chapters.size()) ? m_doc.chapters[m_currentChapter].title : QString();

    QVector<AnnotationRecord> annotations;
    if (!m_doc.id.isEmpty()) {
        auto list = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
        annotations = QVector<AnnotationRecord>(list.begin(), list.end());
    }

    PageCurlRenderer::paint(
        &painter, bounds, curPage, nextPage, m_curlProgress,
        QPointF(bounds.width(), bounds.height()), m_dragPos,
        m_tokens, m_readingMode == ReadingMode::DualPage,
        m_activeTtsSentence,
        &m_typesetter,
        m_doc.title,
        chTitle,
        annotations
    );

    // Fixed-layout pages carry no printed folio of their own, so keep a small
    // Moon+ style page indicator in the bottom gutter while reading.
    if (isFixedLayoutDocument() && m_doc.totalChapters > 0 && !m_isToolbarVisible) {
        const QString indicator = QStringLiteral("%1 / %2")
            .arg(m_currentChapter + 1)
            .arg(m_doc.totalChapters);

        QFont font = painter.font();
        font.setPointSizeF(std::max(8.0, font.pointSizeF()));
        font.setBold(true);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        const qreal chipW = metrics.horizontalAdvance(indicator) + 20.0;
        const qreal chipH = metrics.height() + 6.0;
        const QRectF chip(bounds.center().x() - chipW / 2.0,
                          bounds.bottom() - chipH - 10.0,
                          chipW, chipH);

        painter.setPen(Qt::NoPen);
        painter.setBrush(m_tokens.isDark ? QColor(0, 0, 0, 150) : QColor(255, 255, 255, 205));
        painter.drawRoundedRect(chip, chipH / 2.0, chipH / 2.0);
        painter.setPen(m_tokens.headerFooterColor);
        painter.drawText(chip, Qt::AlignCenter, indicator);
        painter.restore();
    }

    // Draw active drag-selection rectangle on top
    if (m_isSelecting && !m_selectionRect.isEmpty()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(m_selectionRect, QColor(255, 235, 59, 90)); // Moon+ Reader soft amber highlight
        painter.setPen(QPen(QColor(245, 158, 11, 220), 1.5, Qt::SolidLine));
        painter.drawRoundedRect(m_selectionRect, 3.0, 3.0);

        // Moon+ Reader style selection handles at both drag anchors
        painter.setPen(QPen(QColor(255, 255, 255), 2.0));
        painter.setBrush(QColor(245, 158, 11));
        painter.drawEllipse(m_pressPos, 6.0, 6.0);
        painter.drawEllipse(m_selectEnd, 6.0, 6.0);
        painter.restore();
    }
}

void ReaderViewport::resizeEvent(QResizeEvent* /*event*/) {
    if (!m_userExplicitlyChangedMode && !isFixedLayoutDocument() && width() > 1000 && width() > height() * 1.25 && m_readingMode == ReadingMode::PageCurl) {
        setReadingMode(ReadingMode::DualPage);
    }
    m_scrollRenderer->setGeometry(rect());
    m_dualPageRenderer->setGeometry(rect());
    if (m_readingRuler) m_readingRuler->setGeometry(rect());
    if (m_dogEarBookmark) {
        m_dogEarBookmark->setGeometry(width() - 52, 0, 52, 52);
        m_dogEarBookmark->raise();
    }
    m_headerBar->setGeometry(16, 16, width() - 32, 48);
    m_footerBar->setGeometry(16, height() - 66, width() - 32, 50);
    m_tocDrawer->setGeometry(0, 0, m_tocDrawer->width(), height());
    m_ttsBar->reposition(size());
    m_headerBar->raise();
    m_footerBar->raise();
    m_tocDrawer->raise();
    m_ttsBar->raise();
    if (m_quickNav && m_quickNav->isVisible()) {
        m_quickNav->reposition(this);
        m_quickNav->raise();
    }
    if (m_nightFilter) {
        m_nightFilter->setGeometry(rect());
        m_nightFilter->raise();
    }
    if (m_returnJumpBtn && m_returnJumpBtn->isVisible()) {
        m_returnJumpBtn->setGeometry((width() - 220) / 2, 70, 220, 36);
        m_returnJumpBtn->raise();
    }
    repaginate();
}

void ReaderViewport::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
    if (!m_userExplicitlyChangedMode && !isFixedLayoutDocument() && width() > 1000 && width() > height() * 1.25 && m_readingMode == ReadingMode::PageCurl) {
        setReadingMode(ReadingMode::DualPage);
    }
    m_scrollRenderer->setGeometry(rect());
    m_dualPageRenderer->setGeometry(rect());
    if (m_readingRuler) m_readingRuler->setGeometry(rect());
    if (m_dogEarBookmark) {
        m_dogEarBookmark->setGeometry(width() - 52, 0, 52, 52);
        m_dogEarBookmark->raise();
    }
    m_headerBar->setGeometry(16, 16, width() - 32, 48);
    m_footerBar->setGeometry(16, height() - 66, width() - 32, 50);
    m_tocDrawer->setGeometry(0, 0, m_tocDrawer->width(), height());
    m_ttsBar->reposition(size());
    m_headerBar->raise();
    m_footerBar->raise();
    m_tocDrawer->raise();
    m_ttsBar->raise();
    if (m_quickNav && m_quickNav->isVisible()) {
        m_quickNav->reposition(this);
        m_quickNav->raise();
    }
    if (m_nightFilter) {
        m_nightFilter->setGeometry(rect());
        m_nightFilter->raise();
    }
    if (m_returnJumpBtn && m_returnJumpBtn->isVisible()) {
        m_returnJumpBtn->setGeometry((width() - 220) / 2, 70, 220, 36);
        m_returnJumpBtn->raise();
    }
    repaginate();
}

void ReaderViewport::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->position();
        m_dragPos = event->position();
        m_selectEnd = event->position();
        m_isDragging = false;
        m_isSelecting = false;
        m_selectionRect = QRectF();
        m_curlProgress = 0.0;
        if (m_selectionMenu && m_selectionMenu->isVisible()) {
            m_selectionMenu->dismiss();
        }
    }
}

void ReaderViewport::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        qreal dist = (event->position() - m_pressPos).manhattanLength();
        if (dist > 8.0) {
            bool isEdgeDrag = (m_pressPos.x() > width() - 80 || m_pressPos.x() < 80);
            if (isEdgeDrag && m_readingMode == ReadingMode::PageCurl && m_pageTransitionsEnabled) {
                m_isDragging = true;
                m_dragPos = event->position();
                qreal progress = 1.0 - (m_dragPos.x() / static_cast<qreal>(width()));
                m_curlProgress = std::clamp(progress, 0.0, 1.0);
                update();
            } else {
                m_isSelecting = true;
                m_selectEnd = event->position();
                m_selectionRect = QRectF(m_pressPos, m_selectEnd).normalized();
                update();
            }
        }
    }
}

void ReaderViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_isSelecting) {
            m_isSelecting = false;
            if (m_selectionRect.width() > 10.0 && m_selectionRect.height() > 8.0) {
                QString selectedText;
                if (!m_pages.isEmpty() && m_currentPageInChapter < m_pages.size()) {
                    const ParsedPage& curPage = m_pages[m_currentPageInChapter];
                    if (curPage.isImagePage) {
                        QPixmap pm = m_typesetter.preparedPagePixmap(curPage.chapterIndex, curPage.imageBytes);
                        QSize imgSize = pm.isNull() ? QSize(width(), height()) : pm.size();
                        QRectF targetRect = m_typesetter.imageTargetRect(rect(), imgSize);
                        QRectF sel = m_selectionRect.intersected(targetRect);
                        if (sel.width() > 5 && sel.height() > 5) {
                            qreal nx = (sel.x() - targetRect.x()) / targetRect.width();
                            qreal ny = (sel.y() - targetRect.y()) / targetRect.height();
                            qreal nw = sel.width() / targetRect.width();
                            qreal nh = sel.height() / targetRect.height();
                            m_pendingNormalizedRect = QRectF(nx, ny, nw, nh);
                            m_pendingChapterIndex = m_currentChapter;
                            m_pendingPageIndex = m_currentPageInChapter;

                            selectedText = PdfPageService::instance().getPageRegionText(
                                m_doc.filePath, m_currentChapter + 1, m_pendingNormalizedRect
                            );
                        }
                        if (selectedText.trimmed().isEmpty()) {
                            selectedText = PdfPageService::instance().getPageText(m_doc.filePath, m_currentChapter + 1);
                        }
                    } else {
                        m_pendingNormalizedRect = QRectF();
                        m_pendingChapterIndex = m_currentChapter;
                        m_pendingPageIndex = m_currentPageInChapter;
                        selectedText = m_typesetter.selectedTextForRange(m_pressPos, event->position(), rect(), curPage);
                    }
                }

                if (!selectedText.trimmed().isEmpty()) {
                    if (m_isToolbarVisible) toggleToolbar(); // Moon+ hides chrome while selecting
                    m_editingAnnotationId.clear();
                    m_selectionMenu->clearEditingAnnotation();
                    m_selectionMenu->setSelectedText(selectedText.trimmed());
                    m_selectionMenu->showAt(event->position().toPoint(), rect());
                    update();
                    return;
                }
            }
            m_selectionRect = QRectF();
            update();
            return;
        }

        if (!m_isDragging) {
            // 1. Moon+ Reader: tapping an existing highlight opens its manage toolbar
            if (tryOpenHighlightAt(event->position(), event->globalPosition().toPoint())) {
                return;
            }

            // 2. Check for Hyperlink / Footnote Click
            if (!m_pages.isEmpty() && m_currentPageInChapter < m_pages.size()) {
                const ParsedPage& curPage = m_pages[m_currentPageInChapter];
                QString href = m_typesetter.anchorAt(event->position(), rect(), curPage);
                if (!href.isEmpty()) {
                    handleAnchorClicked(href, event->position());
                    return;
                }
            }

            // 3. TAP / CLICK: Moon+ Reader 9-Zone Configurable Action Mapping
            TapZoneAction action = m_gestures->getActionForPoint(event->position(), size());
            switch (action) {
                case TapZoneAction::NextPage:
                    nextPage();
                    break;
                case TapZoneAction::PrevPage:
                    prevPage();
                    break;
                case TapZoneAction::ToggleMenu:
                    toggleToolbar();
                    break;
                case TapZoneAction::ToggleBookmark:
                    toggleBookmark();
                    break;
                case TapZoneAction::ToggleNightFilter:
                    toggleNightFilter();
                    break;
                case TapZoneAction::ToggleTts:
                    toggleTts();
                    break;
                case TapZoneAction::ToggleAutoScroll:
                    toggleAutoScroll();
                    break;
                case TapZoneAction::OpenToc:
                    openToc();
                    break;
                case TapZoneAction::OpenQuickNav:
                    toggleQuickNav();
                    break;
                case TapZoneAction::ToggleFullscreen:
                    emit fullscreenRequested();
                    break;
                case TapZoneAction::None:
                default:
                    break;
            }
        } else {
            // Drag release
            m_isDragging = false;
            if (m_curlProgress > 0.25) {
                nextPage();
            }
            m_curlProgress = 0.0;
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        MouseButtonAction act = m_gestures ? m_gestures->rightClickAction() : MouseButtonAction::DefaultMenu;
        switch (act) {
            case MouseButtonAction::PreviousPage: prevPage(); break;
            case MouseButtonAction::NextPage: nextPage(); break;
            case MouseButtonAction::ToggleFullscreen: emit fullscreenRequested(); break;
            case MouseButtonAction::ToggleBookmark: toggleBookmark(); break;
            case MouseButtonAction::ToggleAutoScroll: toggleAutoScroll(); break;
            case MouseButtonAction::OpenQuickNav: toggleQuickNav(); break;
            case MouseButtonAction::DefaultMenu:
            default:
                toggleToolbar();
                break;
        }
        event->accept();
        return;
    } else if (event->button() == Qt::MiddleButton) {
        MouseButtonAction act = m_gestures ? m_gestures->middleClickAction() : MouseButtonAction::ToggleAutoScroll;
        switch (act) {
            case MouseButtonAction::DefaultMenu: toggleToolbar(); break;
            case MouseButtonAction::PreviousPage: prevPage(); break;
            case MouseButtonAction::NextPage: nextPage(); break;
            case MouseButtonAction::ToggleFullscreen: emit fullscreenRequested(); break;
            case MouseButtonAction::ToggleBookmark: toggleBookmark(); break;
            case MouseButtonAction::OpenQuickNav: toggleQuickNav(); break;
            case MouseButtonAction::ToggleAutoScroll:
            default:
                toggleAutoScroll();
                break;
        }
        event->accept();
        return;
    }
}

void ReaderViewport::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_PageDown:
        case Qt::Key_Space:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            nextPage();
            event->accept();
            break;
        case Qt::Key_Down:
            if (!panImagePageBy(140.0)) nextPage();
            event->accept();
            break;
        case Qt::Key_Left:
        case Qt::Key_PageUp:
        case Qt::Key_Backspace:
            prevPage();
            event->accept();
            break;
        case Qt::Key_Up:
            if (!panImagePageBy(-140.0)) prevPage();
            event->accept();
            break;
        case Qt::Key_Home:
            jumpToChapter(0);
            jumpToPage(0);
            event->accept();
            break;
        case Qt::Key_End:
            if (!m_doc.chapters.isEmpty()) {
                jumpToChapter(static_cast<int>(m_doc.chapters.size()) - 1);
            }
            event->accept();
            break;
        case Qt::Key_Escape:
            emit closeRequested();
            event->accept();
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void ReaderViewport::wheelEvent(QWheelEvent* event) {
    if (m_readingMode == ReadingMode::ContinuousScroll || m_readingMode == ReadingMode::WebtoonVertical) {
        QWidget::wheelEvent(event);
        return;
    }

    MouseWheelAction action = m_gestures ? m_gestures->mouseWheelAction() : MouseWheelAction::TurnPage;
    switch (action) {
        case MouseWheelAction::AdjustFontSize: {
            int deltaPt = (event->angleDelta().y() > 0) ? 1 : -1;
            QFont f = m_typesetter.font();
            int newSize = std::clamp(f.pointSize() + deltaPt, 8, 36);
            f.setPointSize(newSize);
            m_typesetter.setFont(f);
            repaginate();
            break;
        }
        case MouseWheelAction::AdjustNightFilterBrightness: {
            if (m_nightFilter) {
                if (!m_nightFilter->isVisible()) toggleNightFilter();
                int currentB = m_nightFilter->brightness();
                int delta = (event->angleDelta().y() > 0) ? 5 : -5;
                m_nightFilter->setBrightness(std::clamp(currentB + delta, 10, 100));
            }
            break;
        }
        case MouseWheelAction::ToggleAutoScroll: {
            toggleAutoScroll();
            break;
        }
        case MouseWheelAction::TurnPage:
        default: {
            const int delta = event->angleDelta().y();
            // Zoomed fixed-layout pages scroll vertically before the page turns.
            if (delta != 0 && panImagePageBy(-delta)) {
                break;
            }
            if (delta < 0) {
                nextPage();
            } else if (delta > 0) {
                prevPage();
            }
            break;
        }
    }
    event->accept();
}

void ReaderViewport::handleAnchorClicked(const QString& href, const QPointF& clickPos) {
    if (href.startsWith("http://", Qt::CaseInsensitive) || 
        href.startsWith("https://", Qt::CaseInsensitive) ||
        href.startsWith("mailto:", Qt::CaseInsensitive)) {
        QDesktopServices::openUrl(QUrl(href));
        return;
    }

    QString targetId = href;
    if (targetId.startsWith('#')) {
        targetId = targetId.mid(1);
    }

    if (targetId.isEmpty()) return;

    // Search for anchor element in current chapter or across chapters
    QString snippet;
    int targetChapter = -1;

    // Check current chapter first
    if (m_currentChapter >= 0 && m_currentChapter < m_doc.chapters.size()) {
        const auto& ch = m_doc.chapters[m_currentChapter];
        if (ch.htmlContent.contains(targetId)) {
            targetChapter = m_currentChapter;
            static const QRegularExpression elemRegex(QString(R"(<([^>]+)\s+[^>]*(?:id|name)=["']%1["'][^>]*>(.*?)</\1>)").arg(QRegularExpression::escape(targetId)), QRegularExpression::DotMatchesEverythingOption);
            auto match = elemRegex.match(ch.htmlContent);
            if (match.hasMatch()) {
                snippet = match.captured(2).trimmed();
            }
        }
    }

    if (snippet.isEmpty()) {
        for (int i = 0; i < m_doc.chapters.size(); ++i) {
            const auto& ch = m_doc.chapters[i];
            if (ch.htmlContent.contains(targetId)) {
                targetChapter = i;
                static const QRegularExpression elemRegex(QString(R"(<([^>]+)\s+[^>]*(?:id|name)=["']%1["'][^>]*>(.*?)</\1>)").arg(QRegularExpression::escape(targetId)), QRegularExpression::DotMatchesEverythingOption);
                auto match = elemRegex.match(ch.htmlContent);
                if (match.hasMatch()) {
                    snippet = match.captured(2).trimmed();
                }
                break;
            }
        }
    }

    if (snippet.isEmpty()) {
        snippet = tr("Reference: %1").arg(targetId);
    }

    m_footnotePopover->setFootnote(targetId, snippet, targetChapter, 0);
    m_footnotePopover->applyTheme(m_tokens);
    m_footnotePopover->reposition(size(), clickPos);
    m_footnotePopover->show();
    m_footnotePopover->raise();
}

void ReaderViewport::returnToPreviousLocation() {
    if (m_lastJumpLocation.has_value()) {
        auto [ch, pg] = *m_lastJumpLocation;
        m_lastJumpLocation.reset();
        if (m_returnJumpBtn) m_returnJumpBtn->hide();
        jumpToChapter(ch);
        jumpToPage(pg);
    }
}

void ReaderViewport::refreshAnnotations() {
    if (m_doc.id.isEmpty()) return;
    const auto anns = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
    if (m_tocDrawer) m_tocDrawer->setAnnotations(anns);
    if (m_dualPageRenderer) {
        m_dualPageRenderer->setAnnotations(QVector<AnnotationRecord>(anns.begin(), anns.end()));
    }
}

void ReaderViewport::openHighlightMenu(const AnnotationRecord& annotation, const QPoint& globalPos) {
    if (!m_selectionMenu) return;
    m_editingAnnotationId = annotation.id;
    if (m_isToolbarVisible) toggleToolbar();
    m_selectionMenu->setSelectedText(annotation.selectedText);
    m_selectionMenu->setEditingAnnotation(annotation.id, annotation.color);
    m_selectionMenu->showAt(mapFromGlobal(globalPos), rect());
}

bool ReaderViewport::tryOpenHighlightAt(const QPointF& pos, const QPoint& globalPos) {
    if (m_doc.id.isEmpty() || m_pages.isEmpty() || m_currentPageInChapter >= m_pages.size()) return false;

    const ParsedPage& page = m_pages[m_currentPageInChapter];
    if (!page.isImagePage || page.imageBytes.isEmpty()) return false;

    QPixmap pm = m_typesetter.preparedPagePixmap(page.chapterIndex, page.imageBytes);
    if (pm.isNull()) return false;

    const QRectF target = m_typesetter.imageTargetRect(rect(), pm.size());
    const auto anns = DatabaseManager::instance().getAnnotationsForBook(m_doc.id);
    for (auto it = anns.crbegin(); it != anns.crend(); ++it) {
        const AnnotationRecord& ann = *it;
        if (ann.type == QStringLiteral("bookmark")) continue;
        if (ann.chapterIndex != m_currentChapter || ann.pageIndex != m_currentPageInChapter) continue;
        if (!ann.normalizedRect.isValid() || ann.normalizedRect.isNull()) continue;

        const QRectF r(
            target.left() + ann.normalizedRect.x() * target.width(),
            target.top() + ann.normalizedRect.y() * target.height(),
            ann.normalizedRect.width() * target.width(),
            ann.normalizedRect.height() * target.height());
        if (r.adjusted(-3.0, -3.0, 3.0, 3.0).contains(pos)) {
            openHighlightMenu(ann, globalPos);
            return true;
        }
    }
    return false;
}

} // namespace LunaRead
