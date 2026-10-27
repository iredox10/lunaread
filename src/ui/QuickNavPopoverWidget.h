#pragma once

#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVector>
#include <QString>
#include <optional>

#include "../core/ThemeTokens.h"
#include "../core/Types.h"

namespace LunaRead {

/**
 * @brief Custom slider for Moon+ Reader style seeking with live floating bubble.
 *
 * Renders a large sleek slider track with a floating speech-bubble tooltip
 * above the thumb handle showing: "Page X of Y (Z%)".
 */
class QuickNavSeekSlider : public QSlider {
    Q_OBJECT

public:
    explicit QuickNavSeekSlider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
    ~QuickNavSeekSlider() override = default;

    void setTotalPages(int totalPages);
    [[nodiscard]] int totalPages() const noexcept { return m_totalPages; }

    void setCurrentPage(int pageNumber);
    [[nodiscard]] int currentPage() const noexcept { return value(); }

    void setBubbleText(const QString& text);
    [[nodiscard]] QString bubbleText() const noexcept { return m_bubbleText; }

    void applyTheme(const ReaderThemeTokens& tokens);
    [[nodiscard]] ReaderThemeTokens theme() const noexcept { return m_tokens; }

signals:
    void seekPageChanged(int pageIndex);
    void seekCommitted(int pageIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void updateBubbleFromPosition(int posX);
    void updateBubbleTextFromValue(int val);

    int m_totalPages = 1;
    QString m_bubbleText{QStringLiteral("Page 1 of 1 (100%)")};
    bool m_isDragging = false;
    bool m_isHovered = false;
    ReaderThemeTokens m_tokens;
};

/**
 * @brief Quick Navigation Popover & Reading Speed (WPM) Time Estimator.
 *
 * Floating popover / bottom navigation bar (Moon+ Reader style) that pops up
 * on demand or when clicking the bottom status bar.
 *
 * Features:
 * - Chapter selector / Previous Chapter / Next Chapter buttons with current chapter title
 * - Large smooth slider for fast page seeking across the book or current chapter
 * - Live floating tooltip/bubble above the slider thumb showing: "Page X of Y (Z%)"
 * - Direct jump: "Go to Page" button opening a quick spinbox/number input
 * - Reading Speed & Time Remaining Estimator:
 *     Displays: "⏱️ ~15 mins left in chapter | ~3.2 hrs left in book"
 *     Words-per-minute (WPM) estimator (defaults to 220 WPM, can be calibrated
 *     based on time spent reading pages)
 * - Signals:
 *     pageJumpRequested(int pageIndex)
 *     chapterJumpRequested(int chapterIndex)
 *     closeRequested()
 * - Styling: semi-transparent frosted card / modern dark HUD styling with smooth rounded corners
 */
class QuickNavPopoverWidget : public QWidget {
    Q_OBJECT

public:
    static constexpr int DefaultWpm = 220;
    static constexpr int MinWpm = 60;
    static constexpr int MaxWpm = 1200;

    explicit QuickNavPopoverWidget(QWidget* parent = nullptr);
    ~QuickNavPopoverWidget() override = default;

    // Document Information
    void setBookTitle(const QString& title);
    [[nodiscard]] QString bookTitle() const { return m_bookTitle; }

    void setChapters(const QVector<QString>& chapterTitles, int currentChapter = 0);
    void setChapterInfo(int currentChapter, int totalChapters, const QString& title = QString());
    [[nodiscard]] int currentChapter() const noexcept { return m_currentChapter; }
    [[nodiscard]] int totalChapters() const noexcept { return m_totalChapters; }
    [[nodiscard]] QString currentChapterTitle() const { return m_chapterTitle; }

    void setPageProgress(int currentPageIndex, int totalPages);
    [[nodiscard]] int currentPageIndex() const noexcept { return m_currentPageIndex; }
    [[nodiscard]] int totalPages() const noexcept { return m_totalPages; }

    void setWordsRemaining(int chapterWordsLeft, int bookWordsLeft);
    [[nodiscard]] int chapterWordsRemaining() const noexcept { return m_chapterWordsLeft; }
    [[nodiscard]] int bookWordsRemaining() const noexcept { return m_bookWordsLeft; }

    // WPM Estimator & Calibration
    void setWordsPerMinute(int wpm);
    [[nodiscard]] int wordsPerMinute() const noexcept { return m_wpm; }

    void recordReadingSession(int wordsRead, qint64 elapsedMs);
    void calibrateWpm(int targetWpm);
    void resetWpmToDefault();

    [[nodiscard]] QString readingTimeEstimateText() const;
    void setReadingTimeEstimateText(const QString& text);

    static QString formatDuration(double minutes);

    // Styling & Theme
    void applyTheme(const ReaderThemeTokens& tokens);
    [[nodiscard]] ReaderThemeTokens theme() const noexcept { return m_tokens; }

    // Display & Positioning
    void showAtBottom(QWidget* targetParent = nullptr);
    void reposition(QWidget* targetParent = nullptr);
    void togglePopover(QWidget* targetParent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

public slots:
    void goToPreviousChapter();
    void goToNextChapter();
    void jumpToPageDirect(int pageNumber);
    void togglePageInput();
    void applyDirectPageJump();
    void increaseWpm();
    void decreaseWpm();

signals:
    void pageJumpRequested(int pageIndex);
    void chapterJumpRequested(int chapterIndex);
    void closeRequested();
    void wpmChanged(int newWpm);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setupUi();
    void updateStyles();
    void updateChapterUi();
    void updateSliderState();
    void updateTimeEstimateLabel();
    void updateWpmLabel();

    // Data State
    QString m_bookTitle;
    int m_currentChapter = 0;
    int m_totalChapters = 1;
    QString m_chapterTitle;
    QVector<QString> m_chapterTitles;

    int m_currentPageIndex = 0; // 0-based
    int m_totalPages = 1;

    int m_chapterWordsLeft = 0;
    int m_bookWordsLeft = 0;
    int m_wpm = DefaultWpm;
    QVector<int> m_wpmSamples;
    std::optional<QString> m_customEstimateText;

    ReaderThemeTokens m_tokens;

    // UI Controls
    QPushButton* m_closeBtn = nullptr;
    QPushButton* m_prevChapterBtn = nullptr;
    QPushButton* m_nextChapterBtn = nullptr;
    QComboBox* m_chapterCombo = nullptr;

    QuickNavSeekSlider* m_slider = nullptr;
    QLabel* m_sliderBubbleLabel = nullptr;

    QWidget* m_directJumpContainer = nullptr;
    QPushButton* m_goToPageBtn = nullptr;
    QSpinBox* m_pageSpinBox = nullptr;
    QPushButton* m_pageConfirmBtn = nullptr;

    QWidget* m_wpmContainer = nullptr;
    QPushButton* m_wpmMinusBtn = nullptr;
    QPushButton* m_wpmPlusBtn = nullptr;
    QLabel* m_wpmLabel = nullptr;
    QPushButton* m_wpmCalibrateBtn = nullptr;

    QLabel* m_timeEstimateLabel = nullptr;
};

} // namespace LunaRead
