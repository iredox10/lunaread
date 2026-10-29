#include "DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QUuid>
#include <QDateTime>
#include <QDate>
#include <QSet>
#include <algorithm>

namespace LunaRead {

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager() {
    close();
}

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager s_instance;
    return s_instance;
}

bool DatabaseManager::initialize(const QString& customPath) {
    QString dbPath = customPath;
    if (dbPath.isEmpty()) {
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appData);
        dbPath = appData + "/lunaread.db";
    }

    const QString connectionName = QStringLiteral("LunaReadConnection");
    if (QSqlDatabase::contains(connectionName)) {
        m_db = QSqlDatabase::database(connectionName);
        if (m_db.isOpen()) {
            m_db.close();
        }
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    }

    m_db.setDatabaseName(dbPath);
    m_dbPath = dbPath;

    if (!m_db.open()) {
        qWarning() << "Failed to open SQLite database:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_db);
    if (!pragma.exec("PRAGMA foreign_keys = ON;")) {
        qWarning() << "Failed to enable SQLite foreign keys:" << pragma.lastError().text();
    }

    return createTables();
}

void DatabaseManager::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isOpen() const {
    return m_db.isOpen();
}

bool DatabaseManager::createTables() {
    QSqlQuery q(m_db);

    bool ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS books (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            author TEXT,
            series TEXT,
            series_index REAL DEFAULT 0.0,
            file_path TEXT NOT NULL UNIQUE,
            file_size INTEGER NOT NULL,
            file_hash TEXT NOT NULL,
            format TEXT NOT NULL,
            cover_image_path TEXT,
            total_chapters INTEGER DEFAULT 0,
            total_pages INTEGER DEFAULT 0,
            current_chapter_index INTEGER DEFAULT 0,
            current_page_index INTEGER DEFAULT 0,
            progress_percentage REAL DEFAULT 0.0,
            last_read_time INTEGER NOT NULL,
            date_added INTEGER NOT NULL,
            is_favorite INTEGER DEFAULT 0,
            rating REAL DEFAULT 0.0,
            description TEXT,
            publisher TEXT,
            publish_year INTEGER DEFAULT 0,
            isbn TEXT
        );
    )");
    if (!ok) {
        qWarning() << "Failed to create books table:" << q.lastError().text();
        return false;
    }

    // Ensure columns exist for migrated existing databases
    q.exec("ALTER TABLE books ADD COLUMN description TEXT;");
    q.exec("ALTER TABLE books ADD COLUMN publisher TEXT;");
    q.exec("ALTER TABLE books ADD COLUMN publish_year INTEGER DEFAULT 0;");
    q.exec("ALTER TABLE books ADD COLUMN isbn TEXT;");

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS annotations (
            id TEXT PRIMARY KEY,
            book_id TEXT NOT NULL REFERENCES books(id) ON DELETE CASCADE,
            chapter_index INTEGER NOT NULL,
            page_index INTEGER NOT NULL,
            character_offset INTEGER NOT NULL,
            selected_text TEXT,
            note TEXT,
            color INTEGER DEFAULT 4294961979,
            type TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            normalized_rect TEXT
        );
    )");
    if (!ok) {
        qWarning() << "Failed to create annotations table:" << q.lastError().text();
        return false;
    }
    q.exec("ALTER TABLE annotations ADD COLUMN normalized_rect TEXT;");

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS reading_stats (
            id TEXT PRIMARY KEY,
            book_id TEXT NOT NULL REFERENCES books(id) ON DELETE CASCADE,
            session_start INTEGER NOT NULL,
            session_end INTEGER NOT NULL,
            duration_seconds INTEGER NOT NULL,
            words_read INTEGER DEFAULT 0,
            pages_read INTEGER DEFAULT 0,
            average_wpm REAL DEFAULT 0.0
        );
    )");
    if (!ok) {
        qWarning() << "Failed to create reading_stats table:" << q.lastError().text();
        return false;
    }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS daily_reading_activity (
            date TEXT PRIMARY KEY,
            seconds_read INTEGER NOT NULL DEFAULT 0,
            pages_read INTEGER NOT NULL DEFAULT 0
        );
    )");
    if (!ok) {
        qWarning() << "Failed to create daily_reading_activity table:" << q.lastError().text();
        return false;
    }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");
    if (!ok) {
        qWarning() << "Failed to create app_settings table:" << q.lastError().text();
        return false;
    }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS vocabulary_words (
            id TEXT PRIMARY KEY,
            word TEXT NOT NULL,
            phonetic TEXT,
            definition TEXT NOT NULL,
            example TEXT,
            book_id TEXT,
            book_title TEXT,
            date_added INTEGER NOT NULL
        );
    )");
    if (!ok) {
        qWarning() << "Failed to create vocabulary_words table:" << q.lastError().text();
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_books_hash ON books(file_hash);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_books_last_read ON books(last_read_time DESC);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_annotations_book ON annotations(book_id);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_annotations_type ON annotations(type);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_stats_book ON reading_stats(book_id);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_activity_date ON daily_reading_activity(date);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_vocab_word ON vocabulary_words(word);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_vocab_date ON vocabulary_words(date_added DESC);");

    return true;
}

BookItem DatabaseManager::recordToBookItem(const QSqlQuery& q) {
    BookItem item;
    item.id = q.value("id").toString();
    item.title = q.value("title").toString();
    item.author = q.value("author").toString();
    item.series = q.value("series").toString();
    item.seriesIndex = q.value("series_index").toDouble();
    item.filePath = q.value("file_path").toString();
    item.fileSize = q.value("file_size").toLongLong();
    item.fileHash = q.value("file_hash").toString();
    item.format = q.value("format").toString();
    item.coverImagePath = q.value("cover_image_path").toString();
    item.totalChapters = q.value("total_chapters").toInt();
    item.totalPages = q.value("total_pages").toInt();
    item.currentChapterIndex = q.value("current_chapter_index").toInt();
    item.currentPageIndex = q.value("current_page_index").toInt();
    item.progressPercentage = q.value("progress_percentage").toDouble();
    item.lastReadTime = q.value("last_read_time").toLongLong();
    item.dateAdded = q.value("date_added").toLongLong();
    item.isFavorite = (q.value("is_favorite").toInt() == 1);
    item.rating = q.value("rating").toDouble();
    item.description = q.value("description").toString();
    item.publisher = q.value("publisher").toString();
    item.publishYear = q.value("publish_year").toInt();
    item.isbn = q.value("isbn").toString();
    return item;
}

AnnotationRecord DatabaseManager::recordToAnnotation(const QSqlQuery& q) {
    AnnotationRecord a;
    a.id = q.value("id").toString();
    a.bookId = q.value("book_id").toString();
    a.chapterIndex = q.value("chapter_index").toInt();
    a.pageIndex = q.value("page_index").toInt();
    a.characterOffset = q.value("character_offset").toInt();
    a.selectedText = q.value("selected_text").toString();
    a.note = q.value("note").toString();
    a.color = QColor::fromRgba(static_cast<QRgb>(q.value("color").toULongLong()));
    a.type = q.value("type").toString();
    a.createdAt = QDateTime::fromMSecsSinceEpoch(q.value("created_at").toLongLong());
    a.updatedAt = QDateTime::fromMSecsSinceEpoch(q.value("updated_at").toLongLong());
    QString nrStr = q.value("normalized_rect").toString();
    if (!nrStr.isEmpty()) {
        auto parts = nrStr.split(',');
        if (parts.size() == 4) {
            a.normalizedRect = QRectF(parts[0].toDouble(), parts[1].toDouble(), parts[2].toDouble(), parts[3].toDouble());
        }
    }
    return a;
}

ReadingStatEntry DatabaseManager::recordToReadingStat(const QSqlQuery& q) {
    ReadingStatEntry s;
    s.id = q.value("id").toString();
    s.bookId = q.value("book_id").toString();
    s.sessionStart = q.value("session_start").toLongLong();
    s.sessionEnd = q.value("session_end").toLongLong();
    s.durationSeconds = q.value("duration_seconds").toInt();
    s.wordsRead = q.value("words_read").toInt();
    s.pagesRead = q.value("pages_read").toInt();
    s.averageWpm = q.value("average_wpm").toDouble();
    return s;
}

// =========================================================================
// Books CRUD
// =========================================================================

bool DatabaseManager::insertBook(const BookItem& book) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT OR REPLACE INTO books (
            id, title, author, series, series_index, file_path, file_size, file_hash, format,
            cover_image_path, total_chapters, total_pages, current_chapter_index, current_page_index,
            progress_percentage, last_read_time, date_added, is_favorite, rating
        ) VALUES (
            ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
        );
    )");

    const QString id = book.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : book.id;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 dateAdded = (book.dateAdded > 0) ? book.dateAdded : now;
    const qint64 lastRead = (book.lastReadTime > 0) ? book.lastReadTime : dateAdded;

    q.addBindValue(id);
    q.addBindValue(book.title);
    q.addBindValue(book.author);
    q.addBindValue(book.series);
    q.addBindValue(book.seriesIndex);
    q.addBindValue(book.filePath);
    q.addBindValue(book.fileSize);
    q.addBindValue(book.fileHash);
    q.addBindValue(book.format.toLower());
    q.addBindValue(book.coverImagePath);
    q.addBindValue(book.totalChapters);
    q.addBindValue(book.totalPages);
    q.addBindValue(book.currentChapterIndex);
    q.addBindValue(book.currentPageIndex);
    q.addBindValue(book.progressPercentage);
    q.addBindValue(lastRead);
    q.addBindValue(dateAdded);
    q.addBindValue(book.isFavorite ? 1 : 0);
    q.addBindValue(book.rating);

    return q.exec();
}

bool DatabaseManager::updateBook(const BookItem& book) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE books SET
            title = ?,
            author = ?,
            series = ?,
            series_index = ?,
            file_path = ?,
            file_size = ?,
            file_hash = ?,
            format = ?,
            cover_image_path = ?,
            total_chapters = ?,
            total_pages = ?,
            current_chapter_index = ?,
            current_page_index = ?,
            progress_percentage = ?,
            last_read_time = ?,
            date_added = ?,
            is_favorite = ?,
            rating = ?
        WHERE id = ?
    )");

    q.addBindValue(book.title);
    q.addBindValue(book.author);
    q.addBindValue(book.series);
    q.addBindValue(book.seriesIndex);
    q.addBindValue(book.filePath);
    q.addBindValue(book.fileSize);
    q.addBindValue(book.fileHash);
    q.addBindValue(book.format.toLower());
    q.addBindValue(book.coverImagePath);
    q.addBindValue(book.totalChapters);
    q.addBindValue(book.totalPages);
    q.addBindValue(book.currentChapterIndex);
    q.addBindValue(book.currentPageIndex);
    q.addBindValue(book.progressPercentage);
    q.addBindValue(book.lastReadTime);
    q.addBindValue(book.dateAdded);
    q.addBindValue(book.isFavorite ? 1 : 0);
    q.addBindValue(book.rating);
    q.addBindValue(book.id);

    return q.exec() && q.numRowsAffected() > 0;
}

QList<BookItem> DatabaseManager::getBooks(const QString& formatFilter, bool favoritesOnly, const QString& search) {
    QList<BookItem> result;
    QString queryStr = QStringLiteral("SELECT * FROM books WHERE 1=1");

    if (!formatFilter.isEmpty()) {
        queryStr += QStringLiteral(" AND LOWER(format) = ?");
    }
    if (favoritesOnly) {
        queryStr += QStringLiteral(" AND is_favorite = 1");
    }
    if (!search.isEmpty()) {
        queryStr += QStringLiteral(" AND (title LIKE ? OR author LIKE ?)");
    }
    queryStr += QStringLiteral(" ORDER BY last_read_time DESC");

    QSqlQuery q(m_db);
    q.prepare(queryStr);

    if (!formatFilter.isEmpty()) {
        q.addBindValue(formatFilter.toLower());
    }
    if (!search.isEmpty()) {
        const QString pattern = QStringLiteral("%%1%").arg(search);
        q.addBindValue(pattern);
        q.addBindValue(pattern);
    }

    if (q.exec()) {
        while (q.next()) {
            result.append(recordToBookItem(q));
        }
    } else {
        qWarning() << "DatabaseManager::getBooks error:" << q.lastError().text();
    }
    return result;
}

QList<BookItem> DatabaseManager::getAllBooks() {
    return getBooks();
}

std::optional<BookItem> DatabaseManager::getBookById(const QString& id) {
    if (id.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM books WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        return recordToBookItem(q);
    }
    return std::nullopt;
}

std::optional<BookItem> DatabaseManager::getBookByHash(const QString& hash) {
    if (hash.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM books WHERE file_hash = ?"));
    q.addBindValue(hash);
    if (q.exec() && q.next()) {
        return recordToBookItem(q);
    }
    return std::nullopt;
}

std::optional<BookItem> DatabaseManager::getBookByFilePath(const QString& filePath) {
    if (filePath.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM books WHERE file_path = ? LIMIT 1"));
    q.addBindValue(filePath);
    if (q.exec() && q.next()) {
        return recordToBookItem(q);
    }
    return std::nullopt;
}

bool DatabaseManager::updateCoverImagePath(const QString& id, const QString& coverPath) {
    if (id.isEmpty()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE books SET cover_image_path = ? WHERE id = ?"));
    q.addBindValue(coverPath);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DatabaseManager::updateBookProgress(const QString& id, int chapter, int page, double percentage) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE books SET 
            current_chapter_index = ?,
            current_page_index = ?,
            progress_percentage = ?,
            last_read_time = ?
        WHERE id = ?
    )");
    q.addBindValue(chapter);
    q.addBindValue(page);
    q.addBindValue(percentage);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DatabaseManager::updateBookRating(const QString& id, double rating) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE books SET rating = ? WHERE id = ?"));
    q.addBindValue(rating);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DatabaseManager::toggleFavorite(const QString& id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE books SET is_favorite = CASE WHEN is_favorite = 1 THEN 0 ELSE 1 END WHERE id = ?"));
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DatabaseManager::setFavorite(const QString& id, bool isFavorite) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE books SET is_favorite = ? WHERE id = ?"));
    q.addBindValue(isFavorite ? 1 : 0);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DatabaseManager::deleteBook(const QString& id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM books WHERE id = ?"));
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

int DatabaseManager::getBookCount() const {
    QSqlQuery q(QStringLiteral("SELECT COUNT(*) FROM books"), m_db);
    if (q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

// =========================================================================
// Annotations CRUD
// =========================================================================

bool DatabaseManager::insertAnnotation(const AnnotationRecord& a) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT OR REPLACE INTO annotations (
            id, book_id, chapter_index, page_index, character_offset,
            selected_text, note, color, type, created_at, updated_at, normalized_rect
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    const QString id = a.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : a.id;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 createdAt = a.createdAt.isValid() ? a.createdAt.toMSecsSinceEpoch() : now;
    const qint64 updatedAt = a.updatedAt.isValid() ? a.updatedAt.toMSecsSinceEpoch() : now;
    const qint64 colorVal = a.color.isValid() ? static_cast<qint64>(a.color.rgba()) : 4294961979ULL;
    const QString typeStr = a.type.isEmpty() ? QStringLiteral("highlight") : a.type.toLower();

    QString nrStr;
    if (a.normalizedRect.isValid() && !a.normalizedRect.isNull()) {
        nrStr = QString("%1,%2,%3,%4")
            .arg(a.normalizedRect.x())
            .arg(a.normalizedRect.y())
            .arg(a.normalizedRect.width())
            .arg(a.normalizedRect.height());
    }

    q.addBindValue(id);
    q.addBindValue(a.bookId);
    q.addBindValue(a.chapterIndex);
    q.addBindValue(a.pageIndex);
    q.addBindValue(a.characterOffset);
    q.addBindValue(a.selectedText);
    q.addBindValue(a.note);
    q.addBindValue(colorVal);
    q.addBindValue(typeStr);
    q.addBindValue(createdAt);
    q.addBindValue(updatedAt);
    q.addBindValue(nrStr);

    return q.exec();
}

bool DatabaseManager::updateAnnotation(const AnnotationRecord& a) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE annotations SET
            chapter_index = ?,
            page_index = ?,
            character_offset = ?,
            selected_text = ?,
            note = ?,
            color = ?,
            type = ?,
            updated_at = ?,
            normalized_rect = ?
        WHERE id = ?
    )");

    const qint64 updatedAt = a.updatedAt.isValid() ? a.updatedAt.toMSecsSinceEpoch() : QDateTime::currentMSecsSinceEpoch();
    const qint64 colorVal = a.color.isValid() ? static_cast<qint64>(a.color.rgba()) : 4294961979ULL;

    QString nrStr;
    if (a.normalizedRect.isValid() && !a.normalizedRect.isNull()) {
        nrStr = QString("%1,%2,%3,%4")
            .arg(a.normalizedRect.x())
            .arg(a.normalizedRect.y())
            .arg(a.normalizedRect.width())
            .arg(a.normalizedRect.height());
    }

    q.addBindValue(a.chapterIndex);
    q.addBindValue(a.pageIndex);
    q.addBindValue(a.characterOffset);
    q.addBindValue(a.selectedText);
    q.addBindValue(a.note);
    q.addBindValue(colorVal);
    q.addBindValue(a.type.toLower());
    q.addBindValue(updatedAt);
    q.addBindValue(nrStr);
    q.addBindValue(a.id);

    return q.exec() && q.numRowsAffected() > 0;
}

QList<AnnotationRecord> DatabaseManager::getAnnotationsForBook(const QString& bookId, const QString& typeFilter) {
    QList<AnnotationRecord> result;
    QSqlQuery q(m_db);

    if (typeFilter.isEmpty()) {
        q.prepare(QStringLiteral("SELECT * FROM annotations WHERE book_id = ? ORDER BY chapter_index ASC, page_index ASC, character_offset ASC"));
        q.addBindValue(bookId);
    } else {
        q.prepare(QStringLiteral("SELECT * FROM annotations WHERE book_id = ? AND type = ? ORDER BY chapter_index ASC, page_index ASC, character_offset ASC"));
        q.addBindValue(bookId);
        q.addBindValue(typeFilter.toLower());
    }

    if (q.exec()) {
        while (q.next()) {
            result.append(recordToAnnotation(q));
        }
    } else {
        qWarning() << "DatabaseManager::getAnnotationsForBook error:" << q.lastError().text();
    }
    return result;
}

QList<AnnotationRecord> DatabaseManager::getAnnotationsByType(const QString& type) {
    QList<AnnotationRecord> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM annotations WHERE type = ? ORDER BY created_at DESC"));
    q.addBindValue(type.toLower());

    if (q.exec()) {
        while (q.next()) {
            result.append(recordToAnnotation(q));
        }
    }
    return result;
}

std::optional<AnnotationRecord> DatabaseManager::getAnnotationById(const QString& id) {
    if (id.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM annotations WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        return recordToAnnotation(q);
    }
    return std::nullopt;
}

bool DatabaseManager::deleteAnnotation(const QString& id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM annotations WHERE id = ?"));
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DatabaseManager::deleteAnnotationsForBook(const QString& bookId) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM annotations WHERE book_id = ?"));
    q.addBindValue(bookId);
    return q.exec();
}

int DatabaseManager::getAnnotationCount(const QString& bookId) const {
    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM annotations"));
    } else {
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM annotations WHERE book_id = ?"));
        q.addBindValue(bookId);
    }
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

// =========================================================================
// Reading Stats
// =========================================================================

bool DatabaseManager::recordReadingSession(const QString& bookId, qint64 start, qint64 end, int duration, int words, int pages) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO reading_stats (id, book_id, session_start, session_end, duration_seconds, words_read, pages_read, average_wpm)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
    q.addBindValue(bookId);
    q.addBindValue(start);
    q.addBindValue(end);
    q.addBindValue(duration);
    q.addBindValue(words);
    q.addBindValue(pages);

    double wpm = 0.0;
    if (duration > 0 && words > 0) {
        wpm = (static_cast<double>(words) / static_cast<double>(duration)) * 60.0;
    }
    q.addBindValue(wpm);
    const bool ok = q.exec();
    if (ok && (duration > 0 || pages > 0)) {
        QDateTime dt;
        if (start > 100'000'000'000LL) {
            dt = QDateTime::fromMSecsSinceEpoch(start);
        } else {
            dt = QDateTime::fromSecsSinceEpoch(start);
        }
        recordReadingActivity(dt.toString(QStringLiteral("yyyy-MM-dd")), duration, pages);
    }
    return ok;
}

QList<ReadingStatEntry> DatabaseManager::getStatsForBook(const QString& bookId) {
    QList<ReadingStatEntry> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM reading_stats WHERE book_id = ? ORDER BY session_start DESC"));
    q.addBindValue(bookId);

    if (q.exec()) {
        while (q.next()) {
            result.append(recordToReadingStat(q));
        }
    }
    return result;
}

QList<ReadingStatEntry> DatabaseManager::getRecentSessions(int limit, const QString& bookId) {
    QList<ReadingStatEntry> result;
    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT * FROM reading_stats ORDER BY session_start DESC LIMIT ?"));
        q.addBindValue(limit);
    } else {
        q.prepare(QStringLiteral("SELECT * FROM reading_stats WHERE book_id = ? ORDER BY session_start DESC LIMIT ?"));
        q.addBindValue(bookId);
        q.addBindValue(limit);
    }

    if (q.exec()) {
        while (q.next()) {
            result.append(recordToReadingStat(q));
        }
    }
    return result;
}

qint64 DatabaseManager::getTotalReadingTimeSeconds(const QString& bookId) {
    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(duration_seconds), 0) FROM reading_stats"));
    } else {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(duration_seconds), 0) FROM reading_stats WHERE book_id = ?"));
        q.addBindValue(bookId);
    }
    if (q.exec() && q.next()) {
        return q.value(0).toLongLong();
    }
    return 0;
}

qint64 DatabaseManager::getTodayReadingTimeSeconds(const QString& bookId) {
    const qint64 todaySecs = QDateTime(QDate::currentDate().startOfDay()).toSecsSinceEpoch();
    const qint64 todayMSecs = QDateTime(QDate::currentDate().startOfDay()).toMSecsSinceEpoch();

    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT session_start, duration_seconds FROM reading_stats"));
    } else {
        q.prepare(QStringLiteral("SELECT session_start, duration_seconds FROM reading_stats WHERE book_id = ?"));
        q.addBindValue(bookId);
    }
    qint64 total = 0;
    if (q.exec()) {
        while (q.next()) {
            const qint64 start = q.value(0).toLongLong();
            const qint64 duration = q.value(1).toLongLong();
            if (start > 100'000'000'000LL) {
                if (start >= todayMSecs) {
                    total += duration;
                }
            } else {
                if (start >= todaySecs) {
                    total += duration;
                }
            }
        }
    }
    return total;
}

qint64 DatabaseManager::getTotalWordsRead(const QString& bookId) {
    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(words_read), 0) FROM reading_stats"));
    } else {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(words_read), 0) FROM reading_stats WHERE book_id = ?"));
        q.addBindValue(bookId);
    }
    if (q.exec() && q.next()) {
        return q.value(0).toLongLong();
    }
    return 0;
}

qint64 DatabaseManager::getTotalPagesRead(const QString& bookId) {
    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(pages_read), 0) FROM reading_stats"));
    } else {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(pages_read), 0) FROM reading_stats WHERE book_id = ?"));
        q.addBindValue(bookId);
    }
    if (q.exec() && q.next()) {
        return q.value(0).toLongLong();
    }
    return 0;
}

double DatabaseManager::getAverageWpm(const QString& bookId) {
    const qint64 totalDuration = getTotalReadingTimeSeconds(bookId);
    const qint64 totalWords = getTotalWordsRead(bookId);
    if (totalDuration > 0 && totalWords > 0) {
        return (static_cast<double>(totalWords) / static_cast<double>(totalDuration)) * 60.0;
    }
    return 0.0;
}

int DatabaseManager::getSessionCount(const QString& bookId) const {
    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM reading_stats"));
    } else {
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM reading_stats WHERE book_id = ?"));
        q.addBindValue(bookId);
    }
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

bool DatabaseManager::deleteReadingSession(const QString& sessionId) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM reading_stats WHERE id = ?"));
    q.addBindValue(sessionId);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DatabaseManager::clearStatsForBook(const QString& bookId) {
    QSqlQuery q(m_db);
    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("DELETE FROM reading_stats"));
    } else {
        q.prepare(QStringLiteral("DELETE FROM reading_stats WHERE book_id = ?"));
        q.addBindValue(bookId);
    }
    return q.exec();
}

// =========================================================================
// Transactions & Database Maintenance
// =========================================================================

bool DatabaseManager::transaction() {
    return m_db.transaction();
}

bool DatabaseManager::commit() {
    return m_db.commit();
}

bool DatabaseManager::rollback() {
    return m_db.rollback();
}

// =========================================================================
// Daily Reading Activity & Streaks
// =========================================================================

void DatabaseManager::recordReadingActivity(const QString& dateStr, int additionalSeconds, int additionalPages) {
    if (dateStr.isEmpty() || (additionalSeconds <= 0 && additionalPages <= 0)) return;

    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO daily_reading_activity (date, seconds_read, pages_read)
        VALUES (?, ?, ?)
        ON CONFLICT(date) DO UPDATE SET
            seconds_read = seconds_read + excluded.seconds_read,
            pages_read = pages_read + excluded.pages_read;
    )");
    q.addBindValue(dateStr);
    q.addBindValue(additionalSeconds);
    q.addBindValue(additionalPages);
    q.exec();
}

int DatabaseManager::getSecondsReadForDate(const QString& dateStr) const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT seconds_read FROM daily_reading_activity WHERE date = ?"));
    q.addBindValue(dateStr);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

QMap<QString, int> DatabaseManager::getDailyReadingActivity(int daysPast) const {
    QMap<QString, int> activity;
    const QString cutoff = QDate::currentDate().addDays(-daysPast).toString(QStringLiteral("yyyy-MM-dd"));

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT date, seconds_read FROM daily_reading_activity WHERE date >= ? ORDER BY date ASC"));
    q.addBindValue(cutoff);

    if (q.exec()) {
        while (q.next()) {
            activity.insert(q.value(0).toString(), q.value(1).toInt());
        }
    }
    return activity;
}

int DatabaseManager::getCurrentReadingStreakDays() const {
    QDate today = QDate::currentDate();
    int streak = 0;

    QDate checkDate = today;
    if (getSecondsReadForDate(checkDate.toString(QStringLiteral("yyyy-MM-dd"))) < 60) {
        checkDate = today.addDays(-1);
    }

    while (true) {
        QString dStr = checkDate.toString(QStringLiteral("yyyy-MM-dd"));
        if (getSecondsReadForDate(dStr) >= 60) {
            streak++;
            checkDate = checkDate.addDays(-1);
        } else {
            break;
        }
    }
    return streak;
}

int DatabaseManager::getLongestReadingStreakDays() const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT date FROM daily_reading_activity WHERE seconds_read >= 60 ORDER BY date ASC"));
    if (!q.exec()) return 0;

    int maxStreak = 0;
    int currentStreak = 0;
    QDate prevDate;

    while (q.next()) {
        QDate date = QDate::fromString(q.value(0).toString(), QStringLiteral("yyyy-MM-dd"));
        if (!date.isValid()) continue;

        if (prevDate.isValid() && prevDate.daysTo(date) == 1) {
            currentStreak++;
        } else {
            currentStreak = 1;
        }
        maxStreak = std::max(maxStreak, currentStreak);
        prevDate = date;
    }
    return maxStreak;
}

int DatabaseManager::getDailyReadingGoalMinutes() const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key = 'daily_reading_goal_minutes'"));
    if (q.exec() && q.next()) {
        bool ok = false;
        int val = q.value(0).toInt(&ok);
        if (ok && val > 0) return val;
    }
    return 30;
}

void DatabaseManager::setDailyReadingGoalMinutes(int mins) {
    if (mins <= 0) mins = 30;
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO app_settings (key, value)
        VALUES ('daily_reading_goal_minutes', ?)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value;
    )");
    q.addBindValue(QString::number(mins));
    q.exec();
}

bool DatabaseManager::insertVocabularyWord(const VocabularyWord& v) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO vocabulary_words (id, word, phonetic, definition, example, book_id, book_title, date_added)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            phonetic = excluded.phonetic,
            definition = excluded.definition,
            example = excluded.example;
    )");
    q.addBindValue(v.id.isEmpty() ? QString("vocab_%1_%2").arg(v.word).arg(QDateTime::currentMSecsSinceEpoch()) : v.id);
    q.addBindValue(v.word);
    q.addBindValue(v.phonetic);
    q.addBindValue(v.definition);
    q.addBindValue(v.example);
    q.addBindValue(v.bookId);
    q.addBindValue(v.bookTitle);
    q.addBindValue(v.dateAdded > 0 ? v.dateAdded : QDateTime::currentSecsSinceEpoch());
    return q.exec();
}

QList<VocabularyWord> DatabaseManager::getAllVocabularyWords() {
    QList<VocabularyWord> list;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, word, phonetic, definition, example, book_id, book_title, date_added FROM vocabulary_words ORDER BY date_added DESC;")) {
        return list;
    }
    while (q.next()) {
        VocabularyWord w;
        w.id = q.value("id").toString();
        w.word = q.value("word").toString();
        w.phonetic = q.value("phonetic").toString();
        w.definition = q.value("definition").toString();
        w.example = q.value("example").toString();
        w.bookId = q.value("book_id").toString();
        w.bookTitle = q.value("book_title").toString();
        w.dateAdded = q.value("date_added").toLongLong();
        list.append(w);
    }
    return list;
}

bool DatabaseManager::deleteVocabularyWord(const QString& id) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM vocabulary_words WHERE id = ? OR word = ?;");
    q.addBindValue(id);
    q.addBindValue(id);
    return q.exec();
}

bool DatabaseManager::clearVocabularyWords() {
    QSqlQuery q(m_db);
    return q.exec("DELETE FROM vocabulary_words;");
}

int DatabaseManager::getVocabularyWordCount() const {
    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM vocabulary_words;") && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

bool DatabaseManager::clearDatabase() {
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral("DELETE FROM books;"));
}

} // namespace LunaRead
