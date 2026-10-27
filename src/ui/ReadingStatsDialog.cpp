#include "ReadingStatsDialog.h"
#include "../db/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QFrame>
#include <QMessageBox>
#include <QDateTime>
#include <QLocale>
#include <cmath>

namespace LunaRead {

ReadingStatsDialog::ReadingStatsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Reading Statistics - LunaRead"));
    resize(820, 680);
    setMinimumSize(700, 560);
    setModal(false);

    setupUi();
    populateBookFilter();
    refreshStats();
    updateStylesheet();
}

void ReadingStatsDialog::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(16);

    // --- Header Section ---
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    auto* titleContainer = new QVBoxLayout();
    titleContainer->setSpacing(2);

    auto* titleLabel = new QLabel(tr("📊 Reading Statistics"), this);
    titleLabel->setObjectName(QStringLiteral("DialogTitle"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subtitleLabel = new QLabel(tr("Overview of your reading pace, library completion, and reading habits"), this);
    subtitleLabel->setObjectName(QStringLiteral("DialogSubtitle"));

    titleContainer->addWidget(titleLabel);
    titleContainer->addWidget(subtitleLabel);
    headerLayout->addLayout(titleContainer, 1);

    // Book Filter dropdown
    auto* filterLabel = new QLabel(tr("Filter:"), this);
    m_bookFilterCombo = new QComboBox(this);
    m_bookFilterCombo->setMinimumWidth(180);
    m_bookFilterCombo->setFixedHeight(34);
    connect(m_bookFilterCombo, &QComboBox::currentIndexChanged, this, &ReadingStatsDialog::onBookFilterChanged);

    // Dark/Light toggle
    m_themeToggleBtn = new QPushButton(tr("🌓 Mode"), this);
    m_themeToggleBtn->setFixedHeight(34);
    m_themeToggleBtn->setToolTip(tr("Toggle Dark / Light appearance"));
    connect(m_themeToggleBtn, &QPushButton::clicked, this, &ReadingStatsDialog::onToggleDarkMode);

    // Refresh button
    m_refreshBtn = new QPushButton(tr("🔄 Refresh"), this);
    m_refreshBtn->setFixedHeight(34);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ReadingStatsDialog::onRefreshClicked);

    headerLayout->addWidget(filterLabel);
    headerLayout->addWidget(m_bookFilterCombo);
    headerLayout->addWidget(m_themeToggleBtn);
    headerLayout->addWidget(m_refreshBtn);

    rootLayout->addLayout(headerLayout);

    // --- Summary Cards Section ---
    setupSummaryCards(rootLayout);

    // --- Recent Sessions Section ---
    setupSessionsSection(rootLayout);

    // --- Bottom Action Bar ---
    auto* bottomLayout = new QHBoxLayout();
    m_clearBtn = new QPushButton(tr("🗑 Clear History"), this);
    m_clearBtn->setFixedHeight(34);
    connect(m_clearBtn, &QPushButton::clicked, this, &ReadingStatsDialog::onClearHistoryClicked);

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setDefault(true);
    m_closeBtn->setFixedHeight(34);
    m_closeBtn->setMinimumWidth(90);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    bottomLayout->addWidget(m_clearBtn);
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(m_closeBtn);

    rootLayout->addLayout(bottomLayout);
}

QFrame* ReadingStatsDialog::createCard(const QString& title, QLabel** outValueLabel, QLabel** outSubtextLabel, QWidget* extraWidget) {
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("StatCard"));
    card->setFrameShape(QFrame::StyledPanel);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(6);

    auto* titleLbl = new QLabel(title, card);
    titleLbl->setObjectName(QStringLiteral("CardTitle"));

    auto* valLbl = new QLabel(QStringLiteral("0"), card);
    valLbl->setObjectName(QStringLiteral("CardValue"));
    auto valFont = valLbl->font();
    valFont.setPointSize(20);
    valFont.setBold(true);
    valLbl->setFont(valFont);

    auto* subLbl = new QLabel(QStringLiteral("-"), card);
    subLbl->setObjectName(QStringLiteral("CardSubtext"));
    subLbl->setWordWrap(true);

    cardLayout->addWidget(titleLbl);
    cardLayout->addWidget(valLbl);
    cardLayout->addWidget(subLbl);

    if (extraWidget) {
        cardLayout->addWidget(extraWidget);
    }
    cardLayout->addStretch(1);

    if (outValueLabel) *outValueLabel = valLbl;
    if (outSubtextLabel) *outSubtextLabel = subLbl;

    return card;
}

void ReadingStatsDialog::setupSummaryCards(QVBoxLayout* contentLayout) {
    auto* gridLayout = new QGridLayout();
    gridLayout->setSpacing(14);

    // Card 1: Total Reading Time
    auto* cardTotalTime = createCard(tr("TOTAL READING TIME"), &m_totalTimeValueLabel, &m_totalTimeSubLabel);

    // Card 2: Today's Reading Duration
    auto* cardTodayTime = createCard(tr("TODAY'S READING"), &m_todayTimeValueLabel, &m_todayTimeSubLabel);

    // Card 3: Library Status / Progress
    m_shelfProgressBar = new QProgressBar(this);
    m_shelfProgressBar->setFixedHeight(10);
    m_shelfProgressBar->setTextVisible(false);
    m_shelfProgressBar->setRange(0, 100);
    m_shelfProgressBar->setValue(0);
    auto* cardShelf = createCard(tr("LIBRARY PROGRESS"), &m_shelfValueLabel, &m_shelfSubLabel, m_shelfProgressBar);

    // Card 4: Reading Speed & Pace
    auto* cardSpeed = createCard(tr("READING PACE"), &m_speedValueLabel, &m_speedSubLabel);

    // Card 5: Daily Goal & Reading Streak
    m_goalProgressBar = new QProgressBar(this);
    m_goalProgressBar->setFixedHeight(10);
    m_goalProgressBar->setTextVisible(false);
    m_goalProgressBar->setRange(0, 100);
    m_goalProgressBar->setValue(0);
    auto* cardGoal = createCard(tr("DAILY GOAL & STREAK"), &m_streakValueLabel, &m_streakSubLabel, m_goalProgressBar);

    gridLayout->addWidget(cardTotalTime, 0, 0);
    gridLayout->addWidget(cardTodayTime, 0, 1);
    gridLayout->addWidget(cardGoal, 0, 2);
    gridLayout->addWidget(cardShelf, 1, 0);
    gridLayout->addWidget(cardSpeed, 1, 1, 1, 2);

    contentLayout->addLayout(gridLayout);
}

void ReadingStatsDialog::setupSessionsSection(QVBoxLayout* contentLayout) {
    m_sessionsGroupBox = new QGroupBox(tr("Recent Reading Sessions"), this);
    auto* groupLayout = new QVBoxLayout(m_sessionsGroupBox);
    groupLayout->setContentsMargins(12, 14, 12, 12);
    groupLayout->setSpacing(8);

    m_sessionsTable = new QTableWidget(this);
    m_sessionsTable->setColumnCount(6);
    m_sessionsTable->setHorizontalHeaderLabels({
        tr("Date & Time"),
        tr("Book Title"),
        tr("Duration"),
        tr("Pages Read"),
        tr("Speed"),
        tr("Words Read")
    });

    m_sessionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_sessionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_sessionsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_sessionsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_sessionsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_sessionsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_sessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sessionsTable->verticalHeader()->setVisible(false);
    m_sessionsTable->setAlternatingRowColors(true);
    m_sessionsTable->setShowGrid(false);

    m_emptySessionsLabel = new QLabel(tr("No reading sessions recorded yet. Start reading a book to track your stats!"), this);
    m_emptySessionsLabel->setAlignment(Qt::AlignCenter);
    m_emptySessionsLabel->setStyleSheet(QStringLiteral("color: #64748B; padding: 24px; font-size: 11pt;"));

    groupLayout->addWidget(m_sessionsTable, 1);
    groupLayout->addWidget(m_emptySessionsLabel);

    contentLayout->addWidget(m_sessionsGroupBox, 1);
}

void ReadingStatsDialog::populateBookFilter() {
    m_isPopulatingFilter = true;
    m_bookFilterCombo->clear();
    m_bookFilterCombo->addItem(tr("All Books (Whole Library)"), QString());

    const auto allBooks = DatabaseManager::instance().getAllBooks();
    for (const auto& book : allBooks) {
        const QString title = book.title.isEmpty() ? tr("Untitled Book") : book.title;
        m_bookFilterCombo->addItem(title, book.id);
        m_bookTitles.insert(book.id, title);
    }

    m_isPopulatingFilter = false;
}

void ReadingStatsDialog::onBookFilterChanged(int index) {
    if (m_isPopulatingFilter) return;
    m_selectedBookId = m_bookFilterCombo->itemData(index).toString();
    refreshStats();
}

void ReadingStatsDialog::setSelectedBookId(const QString& bookId) {
    m_selectedBookId = bookId;
    for (int i = 0; i < m_bookFilterCombo->count(); ++i) {
        if (m_bookFilterCombo->itemData(i).toString() == bookId) {
            m_bookFilterCombo->setCurrentIndex(i);
            break;
        }
    }
    refreshStats();
}

void ReadingStatsDialog::onRefreshClicked() {
    populateBookFilter();
    refreshStats();
}

void ReadingStatsDialog::onClearHistoryClicked() {
    const QString confirmMsg = m_selectedBookId.isEmpty()
        ? tr("Are you sure you want to clear ALL reading sessions across the library? This action cannot be undone.")
        : tr("Are you sure you want to clear reading sessions for this book? This action cannot be undone.");

    auto reply = QMessageBox::question(this, tr("Confirm History Reset"), confirmMsg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        DatabaseManager::instance().clearStatsForBook(m_selectedBookId);
        refreshStats();
    }
}

void ReadingStatsDialog::onToggleDarkMode() {
    setDarkMode(!m_isDark);
}

void ReadingStatsDialog::refreshStats() {
    DatabaseManager& db = DatabaseManager::instance();

    // Cache book titles
    const auto allBooks = db.getAllBooks();
    m_bookTitles.clear();
    int finishedCount = 0;
    int readingCount = 0;

    for (const auto& book : allBooks) {
        const QString title = book.title.isEmpty() ? tr("Untitled Book") : book.title;
        m_bookTitles.insert(book.id, title);
        if (book.progressPercentage >= 95.0) {
            finishedCount++;
        } else if (book.progressPercentage > 0.0) {
            readingCount++;
        }
    }

    if (m_selectedBookId.isEmpty()) {
        // --- Whole Library Stats ---
        m_totalReadingTimeSecs = db.getTotalReadingTimeSeconds();
        m_todayReadingTimeSecs = db.getTodayReadingTimeSeconds();
        m_totalBooks = static_cast<int>(allBooks.size());
        m_booksFinished = finishedCount;
        m_booksReading = readingCount;
        m_avgWpm = db.getAverageWpm();
        m_totalPagesRead = db.getTotalPagesRead();
        m_totalWordsRead = db.getTotalWordsRead();
        m_sessionCount = db.getSessionCount();

        // Update Card 1: Total Time
        m_totalTimeValueLabel->setText(formatHoursMinutes(m_totalReadingTimeSecs));
        m_totalTimeSubLabel->setText(tr("Across %n reading session(s)", "", m_sessionCount));

        // Update Card 2: Today's Duration
        m_todayTimeValueLabel->setText(formatHoursMinutes(m_todayReadingTimeSecs));
        m_todayTimeSubLabel->setText(tr("Recorded reading today"));

        // Update Card 3: Library Status
        m_shelfValueLabel->setText(tr("%1 Finished").arg(m_booksFinished));
        m_shelfSubLabel->setText(tr("%1 Reading • %2 on Shelf").arg(m_booksReading).arg(m_totalBooks));

        const int pct = (m_totalBooks > 0)
            ? static_cast<int>((static_cast<qint64>(m_booksFinished) * 100) / m_totalBooks)
            : 0;
        m_shelfProgressBar->setValue(pct);

        // Update Card 4: Reading Pace
        const int displayWpm = static_cast<int>(std::round(m_avgWpm));
        m_speedValueLabel->setText(tr("%1 WPM").arg(displayWpm));
        m_speedSubLabel->setText(tr("%1 pages read • %2 words")
            .arg(QLocale().toString(m_totalPagesRead))
            .arg(QLocale().toString(m_totalWordsRead)));

        // Update Card 5: Daily Goal & Streak
        int streak = db.getCurrentReadingStreakDays();
        int bestStreak = db.getLongestReadingStreakDays();
        int goalMins = db.getDailyReadingGoalMinutes();
        int todayMins = static_cast<int>(m_todayReadingTimeSecs / 60);
        int goalPct = std::clamp((todayMins * 100) / std::max(1, goalMins), 0, 100);

        m_streakValueLabel->setText(streak > 0 ? tr("🔥 %1 Days").arg(streak) : tr("0 Days"));
        m_streakSubLabel->setText(tr("Goal: %1 / %2 mins (%3%) • Best: %4 days")
            .arg(todayMins).arg(goalMins).arg(goalPct).arg(bestStreak));
        m_goalProgressBar->setValue(goalPct);

        // Sessions Table
        const auto sessions = db.getRecentSessions(100);
        updateTable(sessions);
    } else {
        // --- Per-Book Stats ---
        m_totalReadingTimeSecs = db.getTotalReadingTimeSeconds(m_selectedBookId);
        m_todayReadingTimeSecs = db.getTodayReadingTimeSeconds(m_selectedBookId);
        m_avgWpm = db.getAverageWpm(m_selectedBookId);
        m_totalPagesRead = db.getTotalPagesRead(m_selectedBookId);
        m_totalWordsRead = db.getTotalWordsRead(m_selectedBookId);
        m_sessionCount = db.getSessionCount(m_selectedBookId);

        auto bookOpt = db.getBookById(m_selectedBookId);
        if (bookOpt.has_value()) {
            m_totalBooks = 1;
            m_booksFinished = (bookOpt->progressPercentage >= 95.0) ? 1 : 0;
            m_booksReading = (bookOpt->progressPercentage > 0.0 && bookOpt->progressPercentage < 95.0) ? 1 : 0;

            const int progressInt = static_cast<int>(std::round(bookOpt->progressPercentage));
            m_shelfValueLabel->setText(tr("%1% Completed").arg(progressInt));
            m_shelfSubLabel->setText(tr("Chapter %1 • Page %2")
                .arg(bookOpt->currentChapterIndex + 1)
                .arg(bookOpt->currentPageIndex + 1));
            m_shelfProgressBar->setValue(progressInt);
        } else {
            m_totalBooks = 0;
            m_booksFinished = 0;
            m_booksReading = 0;
            m_shelfValueLabel->setText(tr("0%"));
            m_shelfSubLabel->setText(tr("Book not found"));
            m_shelfProgressBar->setValue(0);
        }

        m_totalTimeValueLabel->setText(formatHoursMinutes(m_totalReadingTimeSecs));
        m_totalTimeSubLabel->setText(tr("%n session(s) in this book", "", m_sessionCount));

        m_todayTimeValueLabel->setText(formatHoursMinutes(m_todayReadingTimeSecs));
        m_todayTimeSubLabel->setText(tr("Recorded today for this book"));

        const int displayWpm = static_cast<int>(std::round(m_avgWpm));
        m_speedValueLabel->setText(tr("%1 WPM").arg(displayWpm));
        m_speedSubLabel->setText(tr("%1 pages read • %2 words")
            .arg(QLocale().toString(m_totalPagesRead))
            .arg(QLocale().toString(m_totalWordsRead)));

        int streak = db.getCurrentReadingStreakDays();
        int goalMins = db.getDailyReadingGoalMinutes();
        int todayMins = static_cast<int>(m_todayReadingTimeSecs / 60);
        int goalPct = std::clamp((todayMins * 100) / std::max(1, goalMins), 0, 100);

        m_streakValueLabel->setText(streak > 0 ? tr("🔥 %1 Days").arg(streak) : tr("0 Days"));
        m_streakSubLabel->setText(tr("Book: %1 / %2 mins goal").arg(todayMins).arg(goalMins));
        m_goalProgressBar->setValue(goalPct);

        const auto sessions = db.getStatsForBook(m_selectedBookId);
        updateTable(sessions);
    }
}

void ReadingStatsDialog::updateTable(const QList<ReadingStatEntry>& sessions) {
    m_sessionsTable->setRowCount(0);

    if (sessions.isEmpty()) {
        m_sessionsTable->hide();
        m_emptySessionsLabel->show();
        return;
    }

    m_emptySessionsLabel->hide();
    m_sessionsTable->show();
    m_sessionsTable->setRowCount(static_cast<int>(sessions.size()));

    for (int row = 0; row < sessions.size(); ++row) {
        const auto& s = sessions[row];

        // Col 0: Timestamp
        auto* itemTime = new QTableWidgetItem(formatTimestamp(s.sessionStart));
        itemTime->setTextAlignment(Qt::AlignCenter);

        // Col 1: Book Title
        QString title = m_bookTitles.value(s.bookId);
        if (title.isEmpty()) {
            title = tr("Unknown Book");
        }
        auto* itemTitle = new QTableWidgetItem(title);
        itemTitle->setToolTip(title);

        // Col 2: Duration
        auto* itemDuration = new QTableWidgetItem(formatDuration(s.durationSeconds));
        itemDuration->setTextAlignment(Qt::AlignCenter);

        // Col 3: Pages
        auto* itemPages = new QTableWidgetItem(QLocale().toString(s.pagesRead));
        itemPages->setTextAlignment(Qt::AlignCenter);

        // Col 4: Speed
        const int wpmVal = static_cast<int>(std::round(s.averageWpm));
        auto* itemSpeed = new QTableWidgetItem(tr("%1 WPM").arg(wpmVal));
        itemSpeed->setTextAlignment(Qt::AlignCenter);

        // Col 5: Words
        auto* itemWords = new QTableWidgetItem(QLocale().toString(s.wordsRead));
        itemWords->setTextAlignment(Qt::AlignCenter);

        const auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        itemTime->setFlags(flags);
        itemTitle->setFlags(flags);
        itemDuration->setFlags(flags);
        itemPages->setFlags(flags);
        itemSpeed->setFlags(flags);
        itemWords->setFlags(flags);

        m_sessionsTable->setItem(row, 0, itemTime);
        m_sessionsTable->setItem(row, 1, itemTitle);
        m_sessionsTable->setItem(row, 2, itemDuration);
        m_sessionsTable->setItem(row, 3, itemPages);
        m_sessionsTable->setItem(row, 4, itemSpeed);
        m_sessionsTable->setItem(row, 5, itemWords);
    }
}

void ReadingStatsDialog::setDarkMode(bool isDark) {
    m_isDark = isDark;
    m_tokens = isDark ? ReaderThemeTokens::night() : ReaderThemeTokens::modernLight();
    updateStylesheet();
}

void ReadingStatsDialog::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    m_isDark = tokens.isDark;
    updateStylesheet();
}

void ReadingStatsDialog::updateStylesheet() {
    if (m_isDark) {
        setStyleSheet(QStringLiteral(
            "QDialog {"
            "    background-color: #18181B;"
            "    color: #F4F4F5;"
            "}"
            "QLabel#DialogTitle { color: #FAFAFA; }"
            "QLabel#DialogSubtitle { color: #A1A1AA; font-size: 9.5pt; }"
            "QFrame#StatCard {"
            "    background-color: #27272A;"
            "    border: 1px solid #3F3F46;"
            "    border-radius: 8px;"
            "}"
            "QLabel#CardTitle {"
            "    color: #A1A1AA;"
            "    font-size: 8.5pt;"
            "    font-weight: bold;"
            "    letter-spacing: 0.5px;"
            "}"
            "QLabel#CardValue {"
            "    color: #38BDF8;"
            "}"
            "QLabel#CardSubtext {"
            "    color: #D4D4D8;"
            "    font-size: 9pt;"
            "}"
            "QProgressBar {"
            "    background-color: #3F3F46;"
            "    border: none;"
            "    border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "    background-color: #38BDF8;"
            "    border-radius: 4px;"
            "}"
            "QGroupBox {"
            "    background-color: #202023;"
            "    border: 1px solid #3F3F46;"
            "    border-radius: 8px;"
            "    margin-top: 14px;"
            "    font-weight: bold;"
            "    color: #F4F4F5;"
            "    padding-top: 10px;"
            "}"
            "QGroupBox::title {"
            "    subcontrol-origin: margin;"
            "    subcontrol-position: top left;"
            "    left: 12px;"
            "    padding: 0 4px;"
            "    color: #E4E4E7;"
            "}"
            "QTableWidget {"
            "    background-color: #18181B;"
            "    alternate-background-color: #222226;"
            "    color: #F4F4F5;"
            "    border: 1px solid #3F3F46;"
            "    border-radius: 6px;"
            "    selection-background-color: #0284C7;"
            "    selection-color: #FFFFFF;"
            "}"
            "QHeaderView::section {"
            "    background-color: #27272A;"
            "    color: #D4D4D8;"
            "    border: none;"
            "    border-bottom: 1px solid #3F3F46;"
            "    padding: 6px 8px;"
            "    font-weight: bold;"
            "}"
            "QComboBox {"
            "    background-color: #27272A;"
            "    color: #F4F4F5;"
            "    border: 1px solid #3F3F46;"
            "    border-radius: 6px;"
            "    padding: 4px 10px;"
            "}"
            "QComboBox::drop-down {"
            "    border: none;"
            "}"
            "QPushButton {"
            "    background-color: #27272A;"
            "    color: #F4F4F5;"
            "    border: 1px solid #3F3F46;"
            "    border-radius: 6px;"
            "    padding: 6px 14px;"
            "    font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "    background-color: #3F3F46;"
            "}"
            "QPushButton:default {"
            "    background-color: #0284C7;"
            "    border-color: #0284C7;"
            "    color: #FFFFFF;"
            "}"
            "QPushButton:default:hover {"
            "    background-color: #0369A1;"
            "}"
        ));
    } else {
        setStyleSheet(QStringLiteral(
            "QDialog {"
            "    background-color: #F8FAFC;"
            "    color: #0F172A;"
            "}"
            "QLabel#DialogTitle { color: #0F172A; }"
            "QLabel#DialogSubtitle { color: #64748B; font-size: 9.5pt; }"
            "QFrame#StatCard {"
            "    background-color: #FFFFFF;"
            "    border: 1px solid #E2E8F0;"
            "    border-radius: 8px;"
            "}"
            "QLabel#CardTitle {"
            "    color: #64748B;"
            "    font-size: 8.5pt;"
            "    font-weight: bold;"
            "    letter-spacing: 0.5px;"
            "}"
            "QLabel#CardValue {"
            "    color: #2563EB;"
            "}"
            "QLabel#CardSubtext {"
            "    color: #475569;"
            "    font-size: 9pt;"
            "}"
            "QProgressBar {"
            "    background-color: #E2E8F0;"
            "    border: none;"
            "    border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "    background-color: #2563EB;"
            "    border-radius: 4px;"
            "}"
            "QGroupBox {"
            "    background-color: #FFFFFF;"
            "    border: 1px solid #E2E8F0;"
            "    border-radius: 8px;"
            "    margin-top: 14px;"
            "    font-weight: bold;"
            "    color: #0F172A;"
            "    padding-top: 10px;"
            "}"
            "QGroupBox::title {"
            "    subcontrol-origin: margin;"
            "    subcontrol-position: top left;"
            "    left: 12px;"
            "    padding: 0 4px;"
            "    color: #334155;"
            "}"
            "QTableWidget {"
            "    background-color: #FFFFFF;"
            "    alternate-background-color: #F8FAFC;"
            "    color: #0F172A;"
            "    border: 1px solid #E2E8F0;"
            "    border-radius: 6px;"
            "    selection-background-color: #BFDBFE;"
            "    selection-color: #1E3A8A;"
            "}"
            "QHeaderView::section {"
            "    background-color: #F1F5F9;"
            "    color: #334155;"
            "    border: none;"
            "    border-bottom: 1px solid #E2E8F0;"
            "    padding: 6px 8px;"
            "    font-weight: bold;"
            "}"
            "QComboBox {"
            "    background-color: #FFFFFF;"
            "    color: #0F172A;"
            "    border: 1px solid #CBD5E1;"
            "    border-radius: 6px;"
            "    padding: 4px 10px;"
            "}"
            "QComboBox::drop-down {"
            "    border: none;"
            "}"
            "QPushButton {"
            "    background-color: #FFFFFF;"
            "    color: #334155;"
            "    border: 1px solid #CBD5E1;"
            "    border-radius: 6px;"
            "    padding: 6px 14px;"
            "    font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "    background-color: #F1F5F9;"
            "}"
            "QPushButton:default {"
            "    background-color: #2563EB;"
            "    border-color: #2563EB;"
            "    color: #FFFFFF;"
            "}"
            "QPushButton:default:hover {"
            "    background-color: #1D4ED8;"
            "}"
        ));
    }
}

QString ReadingStatsDialog::formatHoursMinutes(qint64 totalSeconds) {
    if (totalSeconds < 0) totalSeconds = 0;
    const qint64 hours = totalSeconds / 3600;
    const qint64 mins = (totalSeconds % 3600) / 60;
    return QStringLiteral("%1h %2m").arg(hours).arg(mins);
}

QString ReadingStatsDialog::formatDuration(qint64 totalSeconds) {
    if (totalSeconds < 0) totalSeconds = 0;
    const qint64 hours = totalSeconds / 3600;
    const qint64 mins = (totalSeconds % 3600) / 60;
    const qint64 secs = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1h %2m").arg(hours).arg(mins);
    }
    if (mins > 0) {
        return QStringLiteral("%1m %2s").arg(mins).arg(secs);
    }
    return QStringLiteral("%1s").arg(secs);
}

QString ReadingStatsDialog::formatTimestamp(qint64 epochSecondsOrMilli) {
    if (epochSecondsOrMilli <= 0) return QStringLiteral("-");
    QDateTime dt;
    if (epochSecondsOrMilli > 100'000'000'000LL) {
        dt = QDateTime::fromMSecsSinceEpoch(epochSecondsOrMilli);
    } else {
        dt = QDateTime::fromSecsSinceEpoch(epochSecondsOrMilli);
    }
    return dt.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
}

} // namespace LunaRead
