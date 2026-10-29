#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantMap>
#include <QList>
#include <QMap>
#include <optional>
#include "../core/Types.h"

namespace LunaRead {

/**
 * @brief SQLite persistence manager for LunaRead-Qt desktop reader.
 *
 * Manages the SQLite database storing book metadata, user annotations
 * (highlights, bookmarks, notes, underlines), and reading statistics
 * (session duration, words/pages read, reading speed in WPM).
 *
 * Thread-safety: QSqlDatabase connections are thread-affine in Qt;
 * this singleton operates on the main GUI thread.
 */
class DatabaseManager : public QObject {
    Q_OBJECT
public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    static DatabaseManager& instance();

    /**
     * @brief Initializes the database connection and creates tables if needed.
     * @param customPath Path to SQLite file, or ":memory:" for tests.
     * @return true if initialized and tables created successfully.
     */
    bool initialize(const QString& customPath = "");
    void close();
    bool isOpen() const;
    [[nodiscard]] QString databasePath() const { return m_dbPath; }
    [[nodiscard]] QSqlDatabase database() const { return m_db; }

    // --- Books CRUD ---
    bool insertBook(const BookItem& book);
    bool updateBook(const BookItem& book);
    QList<BookItem> getBooks(const QString& formatFilter = "", bool favoritesOnly = false, const QString& search = "");
    QList<BookItem> getAllBooks();
    std::optional<BookItem> getBookById(const QString& id);
    std::optional<BookItem> getBookByHash(const QString& hash);
    std::optional<BookItem> getBookByFilePath(const QString& filePath);
    bool updateCoverImagePath(const QString& id, const QString& coverPath);
    bool updateBookProgress(const QString& id, int chapter, int page, double percentage);
    bool updateBookRating(const QString& id, double rating);
    bool toggleFavorite(const QString& id);
    bool setFavorite(const QString& id, bool isFavorite);
    bool deleteBook(const QString& id);
    int getBookCount() const;

    // --- Annotations CRUD ---
    bool insertAnnotation(const AnnotationRecord& annotation);
    bool updateAnnotation(const AnnotationRecord& annotation);
    QList<AnnotationRecord> getAnnotationsForBook(const QString& bookId, const QString& typeFilter = "");
    QList<AnnotationRecord> getAnnotationsByType(const QString& type);
    std::optional<AnnotationRecord> getAnnotationById(const QString& id);
    bool deleteAnnotation(const QString& id);
    bool deleteAnnotationsForBook(const QString& bookId);
    int getAnnotationCount(const QString& bookId = "") const;

    // --- Reading Stats ---
    bool recordReadingSession(const QString& bookId, qint64 start, qint64 end, int duration, int words, int pages);
    QList<ReadingStatEntry> getStatsForBook(const QString& bookId);
    QList<ReadingStatEntry> getRecentSessions(int limit = 50, const QString& bookId = "");
    qint64 getTotalReadingTimeSeconds(const QString& bookId = "");
    qint64 getTodayReadingTimeSeconds(const QString& bookId = "");
    qint64 getTotalWordsRead(const QString& bookId = "");
    qint64 getTotalPagesRead(const QString& bookId = "");
    double getAverageWpm(const QString& bookId = "");
    int getSessionCount(const QString& bookId = "") const;
    bool deleteReadingSession(const QString& sessionId);
    bool clearStatsForBook(const QString& bookId);

    // --- Daily Reading Activity & Streaks ---
    void recordReadingActivity(const QString& dateStr, int additionalSeconds, int additionalPages = 0);
    int getSecondsReadForDate(const QString& dateStr) const;
    QMap<QString, int> getDailyReadingActivity(int daysPast = 30) const;
    int getCurrentReadingStreakDays() const;
    int getLongestReadingStreakDays() const;
    int getDailyReadingGoalMinutes() const;
    void setDailyReadingGoalMinutes(int mins);

    // --- Vocabulary Notebook ---
    bool insertVocabularyWord(const VocabularyWord& word);
    QList<VocabularyWord> getAllVocabularyWords();
    bool deleteVocabularyWord(const QString& id);
    bool clearVocabularyWords();
    int getVocabularyWordCount() const;

    // --- Transactions & Database Maintenance ---
    bool transaction();
    bool commit();
    bool rollback();
    bool clearDatabase();

private:
    QSqlDatabase m_db;
    QString m_dbPath;
    bool createTables();

    static BookItem recordToBookItem(const QSqlQuery& q);
    static AnnotationRecord recordToAnnotation(const QSqlQuery& q);
    static ReadingStatEntry recordToReadingStat(const QSqlQuery& q);
};

} // namespace LunaRead
