#pragma once

#include <QDialog>
#include <QString>
#include <QList>
#include <QHash>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QTableWidget;
class QProgressBar;
class QComboBox;
class QPushButton;
class QGroupBox;
class QFrame;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Modern statistics dashboard dialog for LunaRead.
 *
 * Visualizes reading analytics and library insights directly from DatabaseManager:
 * - Summary cards using styled QFrame / QGroupBox:
 *   1. Total Reading Time (hours / minutes) and session count
 *   2. Today's Reading Duration
 *   3. Library Status (total books on shelf, books reading, books finished >= 95%)
 *   4. Current Reading Speed (average WPM, total pages read, total words read)
 * - Interactive filter for all books or a specific book
 * - Recent reading sessions table (timestamp, book title, duration, pages read, speed)
 * - Modern responsive dark and light theme styling conforming to LunaRead tokens
 */
class ReadingStatsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ReadingStatsDialog(QWidget* parent = nullptr);
    ~ReadingStatsDialog() override = default;

    /// Reload all statistics directly from DatabaseManager::instance()
    void refreshStats();

    /// Switch between dark and light appearance
    void setDarkMode(bool isDark);
    [[nodiscard]] bool isDarkMode() const { return m_isDark; }

    /// Apply visual reader theme tokens
    void applyTheme(const ReaderThemeTokens& tokens);

    /// Filter stats by specific book (empty string = all books)
    void setSelectedBookId(const QString& bookId);
    [[nodiscard]] QString selectedBookId() const { return m_selectedBookId; }

    // Direct stat accessors (useful for testing & programmatic inspection)
    [[nodiscard]] qint64 totalReadingTimeSeconds() const { return m_totalReadingTimeSecs; }
    [[nodiscard]] qint64 todayReadingTimeSeconds() const { return m_todayReadingTimeSecs; }
    [[nodiscard]] int totalBooksCount() const { return m_totalBooks; }
    [[nodiscard]] int booksReadingCount() const { return m_booksReading; }
    [[nodiscard]] int booksFinishedCount() const { return m_booksFinished; }
    [[nodiscard]] double averageWpm() const { return m_avgWpm; }
    [[nodiscard]] qint64 totalPagesRead() const { return m_totalPagesRead; }
    [[nodiscard]] qint64 totalWordsRead() const { return m_totalWordsRead; }
    [[nodiscard]] int sessionCount() const { return m_sessionCount; }

    // Formatting utility functions
    static QString formatDuration(qint64 totalSeconds);
    static QString formatHoursMinutes(qint64 totalSeconds);
    static QString formatTimestamp(qint64 epochSecondsOrMilli);

private slots:
    void onBookFilterChanged(int index);
    void onRefreshClicked();
    void onClearHistoryClicked();
    void onToggleDarkMode();

private:
    void setupUi();
    QFrame* createCard(const QString& title, QLabel** outValueLabel, QLabel** outSubtextLabel, QWidget* extraWidget = nullptr);
    void setupSummaryCards(QVBoxLayout* contentLayout);
    void setupSessionsSection(QVBoxLayout* contentLayout);
    void populateBookFilter();
    void updateTable(const QList<ReadingStatEntry>& sessions);
    void updateStylesheet();

    // Controls
    QComboBox* m_bookFilterCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_themeToggleBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // Card 1: Total Reading Time
    QLabel* m_totalTimeValueLabel = nullptr;
    QLabel* m_totalTimeSubLabel = nullptr;

    // Card 2: Today's Reading Time
    QLabel* m_todayTimeValueLabel = nullptr;
    QLabel* m_todayTimeSubLabel = nullptr;

    // Card 3: Library Status
    QLabel* m_shelfValueLabel = nullptr;
    QLabel* m_shelfSubLabel = nullptr;
    QProgressBar* m_shelfProgressBar = nullptr;

    // Card 4: Reading Speed
    QLabel* m_speedValueLabel = nullptr;
    QLabel* m_speedSubLabel = nullptr;

    // Sessions Table & container
    QGroupBox* m_sessionsGroupBox = nullptr;
    QTableWidget* m_sessionsTable = nullptr;
    QLabel* m_emptySessionsLabel = nullptr;

    // State
    QString m_selectedBookId;
    bool m_isDark = false;
    ReaderThemeTokens m_tokens = ReaderThemeTokens::modernLight();

    // Cache
    qint64 m_totalReadingTimeSecs = 0;
    qint64 m_todayReadingTimeSecs = 0;
    int m_totalBooks = 0;
    int m_booksReading = 0;
    int m_booksFinished = 0;
    double m_avgWpm = 0.0;
    qint64 m_totalPagesRead = 0;
    qint64 m_totalWordsRead = 0;
    int m_sessionCount = 0;

    QHash<QString, QString> m_bookTitles; // bookId -> title
    bool m_isPopulatingFilter = false;
};

} // namespace LunaRead
