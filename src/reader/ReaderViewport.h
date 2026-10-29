#pragma once

#include <QWidget>
#include <QSet>
#include <QStackedWidget>
#include <QVariantAnimation>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"
#include "Typesetter.h"
#include "PageCurlRenderer.h"
#include "ContinuousScrollRenderer.h"
#include "DualPageRenderer.h"
#include "GestureController.h"
#include "DesktopShortcuts.h"
#include "../features/TtsService.h"

namespace LunaRead {

class TocDrawerWidget;
class TtsControlBar;
class DogEarBookmarkWidget;
class AutoScrollOverlayWidget;
class ReadingRulerWidget;
class NightFilterOverlayWidget;
class TextSelectionMenuWidget;
class EyeStrainTimerService;
class QuickNavPopoverWidget;
class FootnotePopoverWidget;

class ReaderViewport : public QWidget {
    Q_OBJECT
public:
    explicit ReaderViewport(QWidget* parent = nullptr);
    ~ReaderViewport() override;

    void loadDocument(const BookDocument& doc, int initialChapter = 0, int initialPage = 0);
    void flushReadingSession();
    void setReadingMode(ReadingMode mode);
    [[nodiscard]] ReadingMode readingMode() const noexcept { return m_readingMode; }
    void setThemeMode(ReaderThemeMode theme);
    [[nodiscard]] DogEarBookmarkWidget* bookmarkWidget() const noexcept { return m_dogEarBookmark; }

public slots:
    void setMangaMode(bool enabled);
    void setWebtoonMode(bool enabled);
    void nextPage();
    void prevPage();
    void toggleToolbar();
    void toggleTheme();
    void toggleBookmark();
    void toggleAutoScroll();
    void toggleReadingRuler();
    void openSearch();
    void openToc();
    void openFontSettings();
    void exportAnnotations();
    void openAllAnnotations();
    void openBookExporter();
    void toggleNightFilter();
    void showNightFilterControls();
    void toggleQuickNav();
    void openTapZoneConfig();
    void toggleTts();
    void toggleAutoCropMargins();
    void openSpeedReader();
    void jumpToChapter(int chapterIndex);
    void jumpToPage(int pageIndex);
    [[nodiscard]] bool isPageTransitionsEnabled() const noexcept { return m_pageTransitionsEnabled; }
    void setPageTransitionsEnabled(bool enabled) noexcept { m_pageTransitionsEnabled = enabled; }

signals:
    void closeRequested();
    void fullscreenRequested();
    void progressUpdated(double percentage);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void repaginate();
    void updateHudStyle();
    void setupHudButtons();
    void updateBookmarkState();
    void handleAnchorClicked(const QString& href, const QPointF& clickPos);
    void returnToPreviousLocation();
    bool tryOpenHighlightAt(const QPointF& pos, const QPoint& globalPos);
    void openHighlightMenu(const AnnotationRecord& annotation, const QPoint& globalPos);
    void refreshAnnotations();
    bool panImagePageBy(qreal deltaPixels);
    void resetImagePan();
    void updateFitButtonLabel();
    [[nodiscard]] bool isFixedLayoutDocument() const;
    void requestPageImage(int chapterIndex);

    BookDocument m_doc;
    ReadingMode m_readingMode = ReadingMode::DualPage;
    bool m_userExplicitlyChangedMode = false;
    ReaderThemeTokens m_tokens;
    Typesetter m_typesetter;
    QSet<int> m_pendingPageLoads;
    QHash<int, int> m_pageLoadAttempts;

    int m_currentChapter = 0;
    int m_currentPageInChapter = 0;
    QVector<ParsedPage> m_pages;

    // Page curl animation & input tracking
    QVariantAnimation* m_curlAnimation = nullptr;
    qreal m_curlProgress = 0.0;
    QPointF m_pressPos;
    QPointF m_dragPos;
    bool m_isDragging = false;

    // Alternate renderers
    ContinuousScrollRenderer* m_scrollRenderer = nullptr;
    DualPageRenderer* m_dualPageRenderer = nullptr;
    GestureController* m_gestures = nullptr;
    DesktopShortcuts* m_shortcuts = nullptr;

    // Modern HUD overlays & Drawers
    QWidget* m_headerBar = nullptr;
    QWidget* m_footerBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_pageInfoLabel = nullptr;
    QSlider* m_progressSlider = nullptr;
    QPushButton* m_fitBtn = nullptr;
    bool m_isToolbarVisible = false;

    TocDrawerWidget* m_tocDrawer = nullptr;
    TtsControlBar* m_ttsBar = nullptr;
    TtsService* m_ttsService = nullptr;
    DogEarBookmarkWidget* m_dogEarBookmark = nullptr;
    AutoScrollOverlayWidget* m_autoScrollOverlay = nullptr;
    ReadingRulerWidget* m_readingRuler = nullptr;
    TextSelectionMenuWidget* m_selectionMenu = nullptr;
    NightFilterOverlayWidget* m_nightFilter = nullptr;
    EyeStrainTimerService* m_eyeTimer = nullptr;
    QuickNavPopoverWidget* m_quickNav = nullptr;
    FootnotePopoverWidget* m_footnotePopover = nullptr;
    QPushButton* m_returnJumpBtn = nullptr;
    std::optional<QPair<int, int>> m_lastJumpLocation;
    QString m_activeTtsSentence;

    // Page transition animation setting (default: disabled for instantaneous turns)
    bool m_pageTransitionsEnabled = false;

    // PDF / Image Page Region Selection & Highlighting
    bool m_isSelecting = false;
    QPointF m_selectEnd;
    QRectF m_selectionRect;
    QRectF m_pendingNormalizedRect;
    int m_pendingChapterIndex = 0;
    int m_pendingPageIndex = 0;
    QString m_editingAnnotationId;
    qreal m_imagePanY = 0.0;

    // Real-time Reading Session & Streak Analytics
    qint64 m_sessionStartTime = 0;
    int m_sessionPagesTurned = 0;
    int m_sessionWordsRead = 0;
    QTimer* m_sessionTimer = nullptr;
};

} // namespace LunaRead
