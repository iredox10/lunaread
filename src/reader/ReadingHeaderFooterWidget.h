#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <QTimer>
#include <optional>
#include "../core/ThemeTokens.h"
#include "../core/Types.h"

namespace LunaRead {

/// Battery status information
struct BatteryStatusInfo {
    int percentage = 100;
    bool isCharging = false;
    bool isAcConnected = false;
    bool hasBattery = false;

    static BatteryStatusInfo querySystem();
};

/**
 * @brief Header Status Bar for LunaRead e-book reader.
 *
 * Implements Moon+ Reader's detailed top header status bar:
 * - Left: Book title (elided if needed)
 * - Center: Current chapter title (elided if needed)
 * - Right: Time left in current chapter (e.g. "12 mins left", calculated from WPM & words left)
 */
class ReadingHeaderWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReadingHeaderWidget(QWidget* parent = nullptr);
    ~ReadingHeaderWidget() override = default;

    void setBookTitle(const QString& title);
    QString bookTitle() const { return m_bookTitle; }

    void setChapterTitle(const QString& title);
    QString chapterTitle() const { return m_chapterTitle; }

    void setTimeLeft(int wpm, int wordsLeftInChapter);
    void setTimeLeftText(const QString& text);
    QString timeLeftText() const { return m_timeLeftText; }

    int wordsPerMinute() const { return m_wpm; }
    int wordsLeftInChapter() const { return m_wordsLeft; }

    void applyTheme(const ReaderThemeTokens& tokens);
    void updateTheme(const ReaderThemeTokens& tokens) { applyTheme(tokens); }
    ReaderThemeTokens theme() const { return m_tokens; }

    void setDrawBackground(bool draw);
    bool drawBackground() const { return m_drawBackground; }

    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QString formatTimeLeft(int wpm, int wordsLeft) const;

    QString m_bookTitle;
    QString m_chapterTitle;
    QString m_timeLeftText{QStringLiteral("0 mins left")};
    int m_wpm = 200;
    int m_wordsLeft = 0;
    bool m_drawBackground = true;
    ReaderThemeTokens m_tokens;
};

/**
 * @brief Footer Status Bar for LunaRead e-book reader.
 *
 * Implements Moon+ Reader's detailed bottom status bar:
 * - Left: System clock (HH:mm) + Battery percentage indicator icon
 * - Center: Page progress ("Page 42 of 310")
 * - Right: Total progress ("34.5%")
 * - Bottom edge: Thin sleek progress bar with optional chapter ticks
 */
class ReadingFooterWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReadingFooterWidget(QWidget* parent = nullptr);
    ~ReadingFooterWidget() override = default;

    void setPageProgress(int curPage, int totalPages);
    int currentPage() const { return m_curPage; }
    int totalPages() const { return m_totalPages; }
    QString pageProgressText() const { return m_pageProgressText; }

    void setChapters(int curChapter, int totalChapters);
    int currentChapter() const { return m_curChapter; }
    int totalChapters() const { return m_totalChapters; }

    void setTotalProgress(double pct);
    double totalProgressPercentage() const { return m_pct; }
    QString totalProgressText() const { return m_totalProgressText; }

    void updateBattery();
    void updateClock();
    void setClockOverride(const QString& timeStr);
    void clearClockOverride();
    QString clockText() const { return m_clockText; }

    void setBatteryOverride(int percentage, bool isCharging, bool isAc, bool hasBattery);
    void clearBatteryOverride();
    BatteryStatusInfo batteryInfo() const { return m_battery; }

    void applyTheme(const ReaderThemeTokens& tokens);
    void updateTheme(const ReaderThemeTokens& tokens) { applyTheme(tokens); }
    ReaderThemeTokens theme() const { return m_tokens; }

    void setDrawBackground(bool draw);
    bool drawBackground() const { return m_drawBackground; }

    QSize sizeHint() const override;

signals:
    void clicked();
    void progressSeekRequested(double percentage);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void drawBatteryIcon(QPainter* painter, const QRectF& iconRect);
    void drawProgressBar(QPainter* painter, const QRectF& barRect);

    int m_curPage = 1;
    int m_totalPages = 1;
    int m_curChapter = 1;
    int m_totalChapters = 1;
    double m_pct = 0.0;
    QString m_pageProgressText{QStringLiteral("Page 1")};
    QString m_totalProgressText{QStringLiteral("0.0%")};
    QString m_clockText{QStringLiteral("00:00")};
    std::optional<QString> m_clockOverride;

    BatteryStatusInfo m_battery;
    bool m_batteryOverridden = false;
    bool m_drawBackground = true;
    ReaderThemeTokens m_tokens;
    QTimer* m_timer = nullptr;
};

/**
 * @brief Composite Reader Status Bar & Head-Up Display widget.
 *
 * Coordinates Header and Footer status bars seamlessly for LunaRead.
 */
class ReadingHeaderFooterWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReadingHeaderFooterWidget(QWidget* parent = nullptr);
    ~ReadingHeaderFooterWidget() override = default;

    void setBookTitle(const QString& title);
    QString bookTitle() const;

    void setChapterTitle(const QString& title);
    QString chapterTitle() const;

    /// Primary method required to update reader statistics across header & footer
    void updateStats(int curPage, int totalPages, int curChapter, int totalChapters, double pct, int wpm, int wordsLeftInChapter);

    void applyTheme(const ReaderThemeTokens& tokens);
    void updateTheme(const ReaderThemeTokens& tokens) { applyTheme(tokens); }
    void setTheme(const ReaderThemeTokens& tokens) { applyTheme(tokens); }
    void setThemeMode(ReaderThemeMode mode);
    ReaderThemeTokens theme() const { return m_tokens; }

    void setHeaderVisible(bool visible);
    bool isHeaderVisible() const;

    void setFooterVisible(bool visible);
    bool isFooterVisible() const;

    void setDrawBackground(bool draw);
    bool drawBackground() const;

    void reposition(const QRect& bounds);
    void reposition(const QSize& size);
    void updateLayout();

    using QWidget::setGeometry;
    void setGeometry(const QRect& rect);

    ReadingHeaderWidget* headerWidget();
    ReadingFooterWidget* footerWidget();
    const ReadingHeaderWidget* headerWidget() const { return m_header; }
    const ReadingFooterWidget* footerWidget() const { return m_footer; }

    // Statistics getters
    int currentPage() const { return m_curPage; }
    int totalPages() const { return m_totalPages; }
    int currentChapter() const { return m_curChapter; }
    int totalChapters() const { return m_totalChapters; }
    double progressPercentage() const { return m_pct; }
    int wordsPerMinute() const { return m_wpm; }
    int wordsLeftInChapter() const { return m_wordsLeft; }
    QString timeLeftText() const;
    QString pageProgressText() const;
    QString totalProgressText() const;
    QString clockText() const;
    BatteryStatusInfo batteryInfo() const;

    // Test overrides
    void setClockOverride(const QString& timeStr);
    void clearClockOverride();
    void setBatteryOverride(int percentage, bool isCharging, bool isAc, bool hasBattery);
    void clearBatteryOverride();

signals:
    void headerClicked();
    void footerClicked();
    void progressSeekRequested(double percentage);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    ReadingHeaderWidget* m_header = nullptr;
    ReadingFooterWidget* m_footer = nullptr;
    ReaderThemeTokens m_tokens;

    int m_curPage = 1;
    int m_totalPages = 1;
    int m_curChapter = 1;
    int m_totalChapters = 1;
    double m_pct = 0.0;
    int m_wpm = 200;
    int m_wordsLeft = 0;
};

} // namespace LunaRead
