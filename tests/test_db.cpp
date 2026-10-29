#include <QObject>
#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDateTime>
#include <cmath>

#include "../src/core/Types.h"
#include "../src/db/DatabaseManager.h"

using namespace LunaRead;

/**
 * @brief Unit test suite for LunaRead-Qt DatabaseManager.
 *
 * Validates SQLite initialization (in-memory mode), table creation, schema integrity,
 * Book CRUD operations, Annotation persistence, query by book and type, cascade deletion,
 * reading session recording and statistical aggregations (duration, words, pages, WPM),
 * transaction support, and edge cases (special characters, quotes, unicode, emojis, SQL injection, null values).
 */
class TestDb : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testInitializeInMemory();
    void testBookCrud();
    void testBookUpdate();
    void testBookQueryAndFiltering();
    void testAnnotationCrud();
    void testAnnotationCascade();
    void testReadingStatsAggregation();
    void testStatsCascade();
    void testEdgeCasesAndSpecialCharacters();
    void testTransactions();

    void cleanupTestCase();
};

void TestDb::init() {
    DatabaseManager& db = DatabaseManager::instance();
    if (!db.isOpen()) {
        QVERIFY2(db.initialize(":memory:"), "Failed to initialize in-memory SQLite database");
    }
    db.clearDatabase();
}

void TestDb::cleanup() {
    DatabaseManager& db = DatabaseManager::instance();
    if (db.isOpen()) {
        db.clearDatabase();
    }
}

void TestDb::testInitializeInMemory() {
    DatabaseManager& db = DatabaseManager::instance();
    QVERIFY2(db.isOpen(), "Database must be open after initialization");

    QSqlDatabase sqlDb = QSqlDatabase::database("LunaReadConnection");
    QVERIFY2(sqlDb.isValid(), "LunaReadConnection QSqlDatabase must be valid");
    QVERIFY2(sqlDb.isOpen(), "LunaReadConnection QSqlDatabase must be open");

    // Verify all core tables are created
    const QStringList tables = sqlDb.tables();
    QVERIFY2(tables.contains("books"), "Table 'books' was not created");
    QVERIFY2(tables.contains("annotations"), "Table 'annotations' was not created");
    QVERIFY2(tables.contains("reading_stats"), "Table 'reading_stats' was not created");

    // Verify foreign key pragma is enabled in SQLite
    QSqlQuery pragmaQuery("PRAGMA foreign_keys;", sqlDb);
    QVERIFY2(pragmaQuery.next(), "Failed to query PRAGMA foreign_keys");
    QCOMPARE(pragmaQuery.value(0).toInt(), 1);

    // Verify column schema of 'books'
    const QSqlRecord booksRecord = sqlDb.record("books");
    QVERIFY(booksRecord.contains("id"));
    QVERIFY(booksRecord.contains("title"));
    QVERIFY(booksRecord.contains("author"));
    QVERIFY(booksRecord.contains("series"));
    QVERIFY(booksRecord.contains("series_index"));
    QVERIFY(booksRecord.contains("file_path"));
    QVERIFY(booksRecord.contains("file_size"));
    QVERIFY(booksRecord.contains("file_hash"));
    QVERIFY(booksRecord.contains("format"));
    QVERIFY(booksRecord.contains("cover_image_path"));
    QVERIFY(booksRecord.contains("total_chapters"));
    QVERIFY(booksRecord.contains("total_pages"));
    QVERIFY(booksRecord.contains("current_chapter_index"));
    QVERIFY(booksRecord.contains("current_page_index"));
    QVERIFY(booksRecord.contains("progress_percentage"));
    QVERIFY(booksRecord.contains("last_read_time"));
    QVERIFY(booksRecord.contains("date_added"));
    QVERIFY(booksRecord.contains("is_favorite"));
    QVERIFY(booksRecord.contains("rating"));

    // Verify column schema of 'annotations'
    const QSqlRecord annotationsRecord = sqlDb.record("annotations");
    QVERIFY(annotationsRecord.contains("id"));
    QVERIFY(annotationsRecord.contains("book_id"));
    QVERIFY(annotationsRecord.contains("chapter_index"));
    QVERIFY(annotationsRecord.contains("page_index"));
    QVERIFY(annotationsRecord.contains("character_offset"));
    QVERIFY(annotationsRecord.contains("selected_text"));
    QVERIFY(annotationsRecord.contains("note"));
    QVERIFY(annotationsRecord.contains("color"));
    QVERIFY(annotationsRecord.contains("type"));
    QVERIFY(annotationsRecord.contains("created_at"));
    QVERIFY(annotationsRecord.contains("updated_at"));

    // Verify column schema of 'reading_stats'
    const QSqlRecord statsRecord = sqlDb.record("reading_stats");
    QVERIFY(statsRecord.contains("id"));
    QVERIFY(statsRecord.contains("book_id"));
    QVERIFY(statsRecord.contains("session_start"));
    QVERIFY(statsRecord.contains("session_end"));
    QVERIFY(statsRecord.contains("duration_seconds"));
    QVERIFY(statsRecord.contains("words_read"));
    QVERIFY(statsRecord.contains("pages_read"));
    QVERIFY(statsRecord.contains("average_wpm"));
}

void TestDb::testBookCrud() {
    DatabaseManager& db = DatabaseManager::instance();

    BookItem book;
    book.id = "book-uuid-1001";
    book.title = "The Pragmatic Programmer";
    book.author = "David Thomas, Andrew Hunt";
    book.series = "Software Craftsmanship";
    book.seriesIndex = 1.0;
    book.filePath = "/path/to/pragmatic_programmer.epub";
    book.fileSize = 5242880;
    book.fileHash = "e10adc3949ba59abbe56e057f20f883e";
    book.format = "epub";
    book.coverImagePath = "/covers/pragmatic.png";
    book.totalChapters = 9;
    book.totalPages = 352;
    book.currentChapterIndex = 0;
    book.currentPageIndex = 0;
    book.progressPercentage = 0.0;
    book.lastReadTime = 1672531199000;
    book.dateAdded = 1672531199000;
    book.isFavorite = false;
    book.rating = 4.8;

    // 1. Insert book
    QVERIFY2(db.insertBook(book), "Inserting BookItem must succeed");
    QCOMPARE(db.getBookCount(), 1);

    // 2. Retrieve by ID - verify all 19 fields match
    const auto bookById = db.getBookById(book.id);
    QVERIFY2(bookById.has_value(), "BookItem must be retrievable by ID");
    QCOMPARE(bookById->id, book.id);
    QCOMPARE(bookById->title, book.title);
    QCOMPARE(bookById->author, book.author);
    QCOMPARE(bookById->series, book.series);
    QCOMPARE(bookById->seriesIndex, book.seriesIndex);
    QCOMPARE(bookById->filePath, book.filePath);
    QCOMPARE(bookById->fileSize, book.fileSize);
    QCOMPARE(bookById->fileHash, book.fileHash);
    QCOMPARE(bookById->format, book.format);
    QCOMPARE(bookById->coverImagePath, book.coverImagePath);
    QCOMPARE(bookById->totalChapters, book.totalChapters);
    QCOMPARE(bookById->totalPages, book.totalPages);
    QCOMPARE(bookById->currentChapterIndex, 0);
    QCOMPARE(bookById->currentPageIndex, 0);
    QCOMPARE(bookById->progressPercentage, 0.0);
    QCOMPARE(bookById->lastReadTime, book.lastReadTime);
    QCOMPARE(bookById->dateAdded, book.dateAdded);
    QCOMPARE(bookById->isFavorite, false);
    QVERIFY(std::abs(bookById->rating - 4.8) < 0.001);

    // 3. Retrieve by file hash - verify all 19 fields match
    const auto bookByHash = db.getBookByHash(book.fileHash);
    QVERIFY2(bookByHash.has_value(), "BookItem must be retrievable by file hash");
    QCOMPARE(bookByHash->id, book.id);
    QCOMPARE(bookByHash->title, book.title);
    QCOMPARE(bookByHash->author, book.author);
    QCOMPARE(bookByHash->series, book.series);
    QCOMPARE(bookByHash->filePath, book.filePath);
    QCOMPARE(bookByHash->fileHash, book.fileHash);
    QCOMPARE(bookByHash->format, book.format);
    QCOMPARE(bookByHash->totalChapters, book.totalChapters);
    QCOMPARE(bookByHash->totalPages, book.totalPages);

    // Negative retrieval checks
    QVERIFY(!db.getBookById("non-existent-uuid").has_value());
    QVERIFY(!db.getBookById("").has_value());
    QVERIFY(!db.getBookByHash("non-existent-hash-value").has_value());
    QVERIFY(!db.getBookByHash("").has_value());

    // 4. Delete the book
    QVERIFY2(db.deleteBook(book.id), "deleteBook must succeed");
    QVERIFY(!db.getBookById(book.id).has_value());
    QVERIFY(!db.getBookByHash(book.fileHash).has_value());
    QCOMPARE(db.getBookCount(), 0);
    QCOMPARE(db.getBooks().size(), 0);
}

void TestDb::testBookUpdate() {
    DatabaseManager& db = DatabaseManager::instance();

    BookItem book;
    book.id = "book-update-test";
    book.title = "Original Title";
    book.author = "Original Author";
    book.series = "Original Series";
    book.seriesIndex = 1.0;
    book.filePath = "/books/original.epub";
    book.fileSize = 1000000;
    book.fileHash = "hash123456";
    book.format = "epub";
    book.coverImagePath = "/covers/original.png";
    book.totalChapters = 10;
    book.totalPages = 200;
    book.currentChapterIndex = 0;
    book.currentPageIndex = 0;
    book.progressPercentage = 0.0;
    book.lastReadTime = 1000000;
    book.dateAdded = 1000000;
    book.isFavorite = false;
    book.rating = 3.0;

    QVERIFY(db.insertBook(book));

    // 1. updateBookProgress
    const int newChapter = 5;
    const int newPage = 12;
    const double newProgress = 52.5;
    QVERIFY(db.updateBookProgress(book.id, newChapter, newPage, newProgress));

    auto updatedOpt = db.getBookById(book.id);
    QVERIFY(updatedOpt.has_value());
    QCOMPARE(updatedOpt->currentChapterIndex, newChapter);
    QCOMPARE(updatedOpt->currentPageIndex, newPage);
    QVERIFY(std::abs(updatedOpt->progressPercentage - newProgress) < 0.001);
    QVERIFY(updatedOpt->lastReadTime > 1000000); // Updated to current time

    // 2. updateBookRating
    QVERIFY(db.updateBookRating(book.id, 4.75));
    updatedOpt = db.getBookById(book.id);
    QVERIFY(updatedOpt.has_value());
    QVERIFY(std::abs(updatedOpt->rating - 4.75) < 0.001);

    // 3. toggleFavorite
    QCOMPARE(updatedOpt->isFavorite, false);
    QVERIFY(db.toggleFavorite(book.id));
    updatedOpt = db.getBookById(book.id);
    QVERIFY(updatedOpt.has_value());
    QCOMPARE(updatedOpt->isFavorite, true);

    QVERIFY(db.toggleFavorite(book.id));
    updatedOpt = db.getBookById(book.id);
    QVERIFY(updatedOpt.has_value());
    QCOMPARE(updatedOpt->isFavorite, false);

    // 4. setFavorite
    QVERIFY(db.setFavorite(book.id, true));
    updatedOpt = db.getBookById(book.id);
    QVERIFY(updatedOpt.has_value());
    QCOMPARE(updatedOpt->isFavorite, true);

    QVERIFY(db.setFavorite(book.id, false));
    updatedOpt = db.getBookById(book.id);
    QVERIFY(updatedOpt.has_value());
    QCOMPARE(updatedOpt->isFavorite, false);

    // 5. updateBook (full entity update)
    BookItem modified = *updatedOpt;
    modified.title = "Revised Title 2nd Edition";
    modified.author = "Revised Author & Co";
    modified.series = "Advanced Series";
    modified.seriesIndex = 2.5;
    modified.totalChapters = 15;
    modified.totalPages = 320;
    modified.rating = 5.0;
    modified.isFavorite = true;
    QVERIFY(db.updateBook(modified));

    updatedOpt = db.getBookById(book.id);
    QVERIFY(updatedOpt.has_value());
    QCOMPARE(updatedOpt->title, QString("Revised Title 2nd Edition"));
    QCOMPARE(updatedOpt->author, QString("Revised Author & Co"));
    QCOMPARE(updatedOpt->series, QString("Advanced Series"));
    QCOMPARE(updatedOpt->seriesIndex, 2.5);
    QCOMPARE(updatedOpt->totalChapters, 15);
    QCOMPARE(updatedOpt->totalPages, 320);
    QVERIFY(std::abs(updatedOpt->rating - 5.0) < 0.001);
    QCOMPARE(updatedOpt->isFavorite, true);
}

void TestDb::testBookQueryAndFiltering() {
    DatabaseManager& db = DatabaseManager::instance();

    BookItem b1;
    b1.id = "b1";
    b1.title = "The Hobbit";
    b1.author = "J.R.R. Tolkien";
    b1.format = "epub";
    b1.filePath = "/books/hobbit.epub";
    b1.fileHash = "h1";
    b1.fileSize = 1000;
    b1.isFavorite = true;
    b1.lastReadTime = 50000;
    QVERIFY(db.insertBook(b1));

    BookItem b2;
    b2.id = "b2";
    b2.title = "The Fellowship of the Ring";
    b2.author = "J.R.R. Tolkien";
    b2.format = "epub";
    b2.filePath = "/books/fellowship.epub";
    b2.fileHash = "h2";
    b2.fileSize = 2000;
    b2.isFavorite = false;
    b2.lastReadTime = 40000;
    QVERIFY(db.insertBook(b2));

    BookItem b3;
    b3.id = "b3";
    b3.title = "Dune";
    b3.author = "Frank Herbert";
    b3.format = "pdf";
    b3.filePath = "/books/dune.pdf";
    b3.fileHash = "h3";
    b3.fileSize = 3000;
    b3.isFavorite = true;
    b3.lastReadTime = 30000;
    QVERIFY(db.insertBook(b3));

    BookItem b4;
    b4.id = "b4";
    b4.title = "Neuromancer";
    b4.author = "William Gibson";
    b4.format = "mobi";
    b4.filePath = "/books/neuromancer.mobi";
    b4.fileHash = "h4";
    b4.fileSize = 4000;
    b4.isFavorite = false;
    b4.lastReadTime = 20000;
    QVERIFY(db.insertBook(b4));

    BookItem b5;
    b5.id = "b5";
    b5.title = "Snow Crash";
    b5.author = "Neal Stephenson";
    b5.format = "pdf";
    b5.filePath = "/books/snow_crash.pdf";
    b5.fileHash = "h5";
    b5.fileSize = 5000;
    b5.isFavorite = true;
    b5.lastReadTime = 10000;
    QVERIFY(db.insertBook(b5));

    QCOMPARE(db.getBookCount(), 5);

    // 1. getAllBooks: returns all 5 books ordered by lastReadTime DESC
    const auto allBooks = db.getAllBooks();
    QCOMPARE(allBooks.size(), 5);
    QCOMPARE(allBooks[0].id, QString("b1"));
    QCOMPARE(allBooks[1].id, QString("b2"));
    QCOMPARE(allBooks[2].id, QString("b3"));
    QCOMPARE(allBooks[3].id, QString("b4"));
    QCOMPARE(allBooks[4].id, QString("b5"));

    // 2. Format filter
    const auto epubBooks = db.getBooks("epub");
    QCOMPARE(epubBooks.size(), 2);
    QCOMPARE(epubBooks[0].id, QString("b1"));
    QCOMPARE(epubBooks[1].id, QString("b2"));

    // Case-insensitivity for format filter
    const auto epubUpper = db.getBooks("EPUB");
    QCOMPARE(epubUpper.size(), 2);

    const auto pdfBooks = db.getBooks("pdf");
    QCOMPARE(pdfBooks.size(), 2);
    QCOMPARE(pdfBooks[0].id, QString("b3"));
    QCOMPARE(pdfBooks[1].id, QString("b5"));

    const auto mobiBooks = db.getBooks("mobi");
    QCOMPARE(mobiBooks.size(), 1);
    QCOMPARE(mobiBooks[0].id, QString("b4"));

    const auto txtBooks = db.getBooks("txt");
    QCOMPARE(txtBooks.size(), 0);

    // 3. Favorites only filter
    const auto favorites = db.getBooks("", true);
    QCOMPARE(favorites.size(), 3);
    QCOMPARE(favorites[0].id, QString("b1"));
    QCOMPARE(favorites[1].id, QString("b3"));
    QCOMPARE(favorites[2].id, QString("b5"));

    // 4. Search filter by title
    const auto searchThe = db.getBooks("", false, "The");
    QCOMPARE(searchThe.size(), 2);
    QCOMPARE(searchThe[0].id, QString("b1"));
    QCOMPARE(searchThe[1].id, QString("b2"));

    // Search filter by author
    const auto searchTolkien = db.getBooks("", false, "Tolkien");
    QCOMPARE(searchTolkien.size(), 2);

    const auto searchHerbert = db.getBooks("", false, "Herbert");
    QCOMPARE(searchHerbert.size(), 1);
    QCOMPARE(searchHerbert[0].id, QString("b3"));

    // 5. Combined filters: format + favorites
    const auto favoritePdfs = db.getBooks("pdf", true);
    QCOMPARE(favoritePdfs.size(), 2);
    QCOMPARE(favoritePdfs[0].id, QString("b3"));
    QCOMPARE(favoritePdfs[1].id, QString("b5"));

    // Combined: format + favorites + search
    const auto specificSearch = db.getBooks("epub", true, "Hobbit");
    QCOMPARE(specificSearch.size(), 1);
    QCOMPARE(specificSearch[0].id, QString("b1"));
}

void TestDb::testAnnotationCrud() {
    DatabaseManager& db = DatabaseManager::instance();

    // Insert parent book required by foreign key constraint
    BookItem book;
    book.id = "book-annotation-crud";
    book.title = "Refactoring";
    book.author = "Martin Fowler";
    book.filePath = "/books/refactoring.epub";
    book.fileHash = "hash-refactor";
    book.format = "epub";
    book.fileSize = 1000;
    QVERIFY(db.insertBook(book));

    // 1. Insert annotations of different types
    AnnotationRecord a1;
    a1.id = "ann-1";
    a1.bookId = book.id;
    a1.chapterIndex = 0;
    a1.pageIndex = 5;
    a1.characterOffset = 20;
    a1.selectedText = "Extract Method";
    a1.note = "First core refactoring";
    a1.color = QColor(255, 245, 157); // yellow
    a1.type = "highlight";
    a1.normalizedRect = QRectF(0.1, 0.2, 0.35, 0.08);
    a1.createdAt = QDateTime::fromMSecsSinceEpoch(1000000);
    a1.updatedAt = a1.createdAt;
    QVERIFY(db.insertAnnotation(a1));

    AnnotationRecord a2;
    a2.id = "ann-2";
    a2.bookId = book.id;
    a2.chapterIndex = 1;
    a2.pageIndex = 0;
    a2.characterOffset = 0;
    a2.selectedText = "Principles in Refactoring";
    a2.note = "";
    a2.color = QColor(33, 150, 243); // blue
    a2.type = "bookmark";
    a2.createdAt = QDateTime::fromMSecsSinceEpoch(2000000);
    a2.updatedAt = a2.createdAt;
    QVERIFY(db.insertAnnotation(a2));

    AnnotationRecord a3;
    a3.id = "ann-3";
    a3.bookId = book.id;
    a3.chapterIndex = 1;
    a3.pageIndex = 3;
    a3.characterOffset = 15;
    a3.selectedText = "Code smells";
    a3.note = "Long method and large class are key smells";
    a3.color = QColor(76, 175, 80); // green
    a3.type = "note";
    a3.createdAt = QDateTime::fromMSecsSinceEpoch(3000000);
    a3.updatedAt = a3.createdAt;
    QVERIFY(db.insertAnnotation(a3));

    AnnotationRecord a4;
    a4.id = "ann-4";
    a4.bookId = book.id;
    a4.chapterIndex = 1;
    a4.pageIndex = 3;
    a4.characterOffset = 80;
    a4.selectedText = "Duplicated code";
    a4.note = "";
    a4.color = QColor(255, 152, 0); // orange
    a4.type = "underline";
    a4.createdAt = QDateTime::fromMSecsSinceEpoch(4000000);
    a4.updatedAt = a4.createdAt;
    QVERIFY(db.insertAnnotation(a4));

    QCOMPARE(db.getAnnotationCount(book.id), 4);
    QCOMPARE(db.getAnnotationCount(), 4);

    // 2. Retrieve all annotations for book (sorted by chapter, page, offset)
    const auto allAnn = db.getAnnotationsForBook(book.id);
    QCOMPARE(allAnn.size(), 4);
    QCOMPARE(allAnn[0].id, QString("ann-1"));
    QCOMPARE(allAnn[1].id, QString("ann-2"));
    QCOMPARE(allAnn[2].id, QString("ann-3"));
    QCOMPARE(allAnn[3].id, QString("ann-4"));

    // 3. Query annotations by type for book
    const auto highlights = db.getAnnotationsForBook(book.id, "highlight");
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights[0].id, QString("ann-1"));
    QCOMPARE(highlights[0].normalizedRect, QRectF(0.1, 0.2, 0.35, 0.08));

    const auto bookmarks = db.getAnnotationsForBook(book.id, "bookmark");
    QCOMPARE(bookmarks.size(), 1);
    QCOMPARE(bookmarks[0].id, QString("ann-2"));

    const auto notes = db.getAnnotationsForBook(book.id, "note");
    QCOMPARE(notes.size(), 1);
    QCOMPARE(notes[0].id, QString("ann-3"));

    const auto underlines = db.getAnnotationsForBook(book.id, "underline");
    QCOMPARE(underlines.size(), 1);
    QCOMPARE(underlines[0].id, QString("ann-4"));

    // 4. Query annotations globally by type
    const auto globalHighlights = db.getAnnotationsByType("highlight");
    QCOMPARE(globalHighlights.size(), 1);
    QCOMPARE(globalHighlights[0].id, QString("ann-1"));

    // 5. Retrieve by ID
    const auto annOpt = db.getAnnotationById("ann-3");
    QVERIFY(annOpt.has_value());
    QCOMPARE(annOpt->id, QString("ann-3"));
    QCOMPARE(annOpt->bookId, book.id);
    QCOMPARE(annOpt->chapterIndex, 1);
    QCOMPARE(annOpt->pageIndex, 3);
    QCOMPARE(annOpt->characterOffset, 15);
    QCOMPARE(annOpt->selectedText, QString("Code smells"));
    QCOMPARE(annOpt->note, QString("Long method and large class are key smells"));
    QCOMPARE(annOpt->type, QString("note"));

    QVERIFY(!db.getAnnotationById("non-existent-ann").has_value());
    QVERIFY(!db.getAnnotationById("").has_value());

    // 6. Update annotation
    AnnotationRecord updatedAnn = *annOpt;
    updatedAnn.note = "Updated note: Remember Primitive Obsession smell as well!";
    updatedAnn.color = QColor(156, 39, 176); // purple
    updatedAnn.updatedAt = QDateTime::currentDateTime();
    QVERIFY(db.updateAnnotation(updatedAnn));

    const auto checkUpdated = db.getAnnotationById("ann-3");
    QVERIFY(checkUpdated.has_value());
    QCOMPARE(checkUpdated->note, QString("Updated note: Remember Primitive Obsession smell as well!"));
    QCOMPARE(checkUpdated->color.rgb(), QColor(156, 39, 176).rgb());

    // 7. Delete single annotation
    QVERIFY(db.deleteAnnotation("ann-4"));
    QCOMPARE(db.getAnnotationCount(book.id), 3);
    QVERIFY(!db.getAnnotationById("ann-4").has_value());

    // 8. Delete all annotations for book
    QVERIFY(db.deleteAnnotationsForBook(book.id));
    QCOMPARE(db.getAnnotationCount(book.id), 0);
    QCOMPARE(db.getAnnotationsForBook(book.id).size(), 0);
}

void TestDb::testAnnotationCascade() {
    DatabaseManager& db = DatabaseManager::instance();

    // Insert parent book required by foreign key constraint
    BookItem book;
    book.id = "book-annotation-cascade-test";
    book.title = "Design Patterns: Elements of Reusable Object-Oriented Software";
    book.author = "Erich Gamma, Richard Helm, Ralph Johnson, John Vlissides";
    book.filePath = "/books/design_patterns.epub";
    book.fileHash = "5d41402abc4b2a76b9719d911017c592";
    book.format = "epub";
    book.fileSize = 1024000;
    book.dateAdded = QDateTime::currentMSecsSinceEpoch();
    book.lastReadTime = QDateTime::currentMSecsSinceEpoch();
    QVERIFY2(db.insertBook(book), "Parent book insertion must succeed");

    // Prepare annotations with deliberate non-sorted order
    AnnotationRecord a1;
    a1.id = "ann-order-3";
    a1.bookId = book.id;
    a1.chapterIndex = 2;
    a1.pageIndex = 0;
    a1.characterOffset = 100;
    a1.selectedText = "Adapter Pattern";
    a1.note = "Converts interface of a class";
    a1.color = QColor(255, 235, 59);
    a1.type = "note";
    a1.createdAt = QDateTime::currentDateTime();

    AnnotationRecord a2;
    a2.id = "ann-order-1";
    a2.bookId = book.id;
    a2.chapterIndex = 0;
    a2.pageIndex = 2;
    a2.characterOffset = 15;
    a2.selectedText = "Introduction to Design Patterns";
    a2.note = "Foundation reading";
    a2.color = QColor(76, 175, 80);
    a2.type = "highlight";
    a2.createdAt = QDateTime::currentDateTime();

    AnnotationRecord a3;
    a3.id = "ann-order-5";
    a3.bookId = book.id;
    a3.chapterIndex = 2;
    a3.pageIndex = 3;
    a3.characterOffset = 250;
    a3.selectedText = "Bridge Pattern";
    a3.note = "Decouples abstraction from implementation";
    a3.color = QColor(33, 150, 243);
    a3.type = "bookmark";
    a3.createdAt = QDateTime::currentDateTime();

    AnnotationRecord a4;
    a4.id = "ann-order-2";
    a4.bookId = book.id;
    a4.chapterIndex = 1;
    a4.pageIndex = 0;
    a4.characterOffset = 50;
    a4.selectedText = "A Case Study: Designing a Document Editor";
    a4.note = "Lexi editor design";
    a4.color = QColor(255, 152, 0);
    a4.type = "underline";
    a4.createdAt = QDateTime::currentDateTime();

    AnnotationRecord a5;
    a5.id = "ann-order-4";
    a5.bookId = book.id;
    a5.chapterIndex = 2;
    a5.pageIndex = 3;
    a5.characterOffset = 50;
    a5.selectedText = "Composite Pattern";
    a5.note = "Tree structures representing part-whole hierarchies";
    a5.color = QColor(156, 39, 176);
    a5.type = "highlight";
    a5.createdAt = QDateTime::currentDateTime();

    // Insert all annotations in scrambled order
    QVERIFY(db.insertAnnotation(a1));
    QVERIFY(db.insertAnnotation(a2));
    QVERIFY(db.insertAnnotation(a3));
    QVERIFY(db.insertAnnotation(a4));
    QVERIFY(db.insertAnnotation(a5));

    // Retrieve annotations and verify sorted reading order:
    // (chapter_index ASC, page_index ASC, character_offset ASC)
    const QList<AnnotationRecord> retrieved = db.getAnnotationsForBook(book.id);
    QCOMPARE(retrieved.size(), 5);

    QCOMPARE(retrieved[0].id, QString("ann-order-1"));
    QCOMPARE(retrieved[0].chapterIndex, 0);
    QCOMPARE(retrieved[0].pageIndex, 2);
    QCOMPARE(retrieved[0].characterOffset, 15);
    QCOMPARE(retrieved[0].type, QString("highlight"));

    QCOMPARE(retrieved[1].id, QString("ann-order-2"));
    QCOMPARE(retrieved[1].chapterIndex, 1);
    QCOMPARE(retrieved[1].pageIndex, 0);
    QCOMPARE(retrieved[1].characterOffset, 50);
    QCOMPARE(retrieved[1].type, QString("underline"));

    QCOMPARE(retrieved[2].id, QString("ann-order-3"));
    QCOMPARE(retrieved[2].chapterIndex, 2);
    QCOMPARE(retrieved[2].pageIndex, 0);
    QCOMPARE(retrieved[2].characterOffset, 100);
    QCOMPARE(retrieved[2].type, QString("note"));

    QCOMPARE(retrieved[3].id, QString("ann-order-4"));
    QCOMPARE(retrieved[3].chapterIndex, 2);
    QCOMPARE(retrieved[3].pageIndex, 3);
    QCOMPARE(retrieved[3].characterOffset, 50);
    QCOMPARE(retrieved[3].type, QString("highlight"));

    QCOMPARE(retrieved[4].id, QString("ann-order-5"));
    QCOMPARE(retrieved[4].chapterIndex, 2);
    QCOMPARE(retrieved[4].pageIndex, 3);
    QCOMPARE(retrieved[4].characterOffset, 250);
    QCOMPARE(retrieved[4].type, QString("bookmark"));

    // Delete the parent book
    QVERIFY2(db.deleteBook(book.id), "Deleting parent book must succeed");

    // Verify cascade deletion of annotations via DatabaseManager API
    const QList<AnnotationRecord> annotationsAfterDelete = db.getAnnotationsForBook(book.id);
    QVERIFY2(annotationsAfterDelete.isEmpty(), "Annotations must be empty after parent book deletion");

    // Verify directly in SQLite engine that foreign key ON DELETE CASCADE deleted records
    QSqlQuery directCheck(QSqlDatabase::database("LunaReadConnection"));
    directCheck.prepare("SELECT COUNT(*) FROM annotations WHERE book_id = ?");
    directCheck.addBindValue(book.id);
    QVERIFY(directCheck.exec());
    QVERIFY(directCheck.next());
    QCOMPARE(directCheck.value(0).toInt(), 0);

    QSqlQuery totalCheck("SELECT COUNT(*) FROM annotations;", QSqlDatabase::database("LunaReadConnection"));
    QVERIFY(totalCheck.next());
    QCOMPARE(totalCheck.value(0).toInt(), 0);
}

void TestDb::testReadingStatsAggregation() {
    DatabaseManager& db = DatabaseManager::instance();

    // Verify initial total reading time is 0
    QCOMPARE(db.getTotalReadingTimeSeconds(), 0);
    QCOMPARE(db.getTotalWordsRead(), 0);
    QCOMPARE(db.getTotalPagesRead(), 0);
    QCOMPARE(db.getAverageWpm(), 0.0);
    QCOMPARE(db.getSessionCount(), 0);

    // Insert parent book required by foreign key constraint
    BookItem book;
    book.id = "book-stats-aggregation-test";
    book.title = "Structure and Interpretation of Computer Programs";
    book.author = "Harold Abelson, Gerald Jay Sussman";
    book.filePath = "/books/sicp.epub";
    book.fileHash = "7d793037a0760186574b0282f2f435e7";
    book.format = "epub";
    book.fileSize = 2048000;
    book.dateAdded = QDateTime::currentMSecsSinceEpoch();
    book.lastReadTime = QDateTime::currentMSecsSinceEpoch();
    QVERIFY2(db.insertBook(book), "Parent book insertion must succeed");

    // Record session 1:
    // duration: 300 seconds (5 min), words: 1500 -> WPM = (1500 / 300) * 60 = 300.0
    const qint64 start1 = 1000000;
    const qint64 end1 = 1000300;
    const int duration1 = 300;
    const int words1 = 1500;
    const int pages1 = 6;
    QVERIFY2(db.recordReadingSession(book.id, start1, end1, duration1, words1, pages1),
             "Recording session 1 must succeed");

    // Record session 2:
    // duration: 600 seconds (10 min), words: 2400 -> WPM = (2400 / 600) * 60 = 240.0
    const qint64 start2 = 2000000;
    const qint64 end2 = 2000600;
    const int duration2 = 600;
    const int words2 = 2400;
    const int pages2 = 10;
    QVERIFY2(db.recordReadingSession(book.id, start2, end2, duration2, words2, pages2),
             "Recording session 2 must succeed");

    // Record session 3:
    // duration: 900 seconds (15 min), words: 3150 -> WPM = (3150 / 900) * 60 = 210.0
    const qint64 start3 = 3000000;
    const qint64 end3 = 3000900;
    const int duration3 = 900;
    const int words3 = 3150;
    const int pages3 = 12;
    QVERIFY2(db.recordReadingSession(book.id, start3, end3, duration3, words3, pages3),
             "Recording session 3 must succeed");

    // Verify aggregations across all books and for specific book
    const qint64 totalExpectedTime = duration1 + duration2 + duration3; // 1800
    const qint64 totalExpectedWords = words1 + words2 + words3;        // 7050
    const qint64 totalExpectedPages = pages1 + pages2 + pages3;        // 28

    QCOMPARE(db.getTotalReadingTimeSeconds(), totalExpectedTime);
    QCOMPARE(db.getTotalReadingTimeSeconds(book.id), totalExpectedTime);

    QCOMPARE(db.getTotalWordsRead(), totalExpectedWords);
    QCOMPARE(db.getTotalWordsRead(book.id), totalExpectedWords);

    QCOMPARE(db.getTotalPagesRead(), totalExpectedPages);
    QCOMPARE(db.getTotalPagesRead(book.id), totalExpectedPages);

    QCOMPARE(db.getSessionCount(), 3);
    QCOMPARE(db.getSessionCount(book.id), 3);

    // Weighted average WPM: (7050 / 1800) * 60 = 235.0
    const double expectedAverageWpm = (static_cast<double>(totalExpectedWords) / static_cast<double>(totalExpectedTime)) * 60.0;
    QVERIFY(qFuzzyCompare(db.getAverageWpm(), expectedAverageWpm));
    QVERIFY(qFuzzyCompare(db.getAverageWpm(book.id), 235.0));

    // Retrieve stats (ordered by session_start DESC)
    const QList<ReadingStatEntry> stats = db.getStatsForBook(book.id);
    QCOMPARE(stats.size(), 3);

    // Session 3 (start: 3000000)
    QCOMPARE(stats[0].sessionStart, start3);
    QCOMPARE(stats[0].sessionEnd, end3);
    QCOMPARE(stats[0].durationSeconds, duration3);
    QCOMPARE(stats[0].wordsRead, words3);
    QCOMPARE(stats[0].pagesRead, pages3);
    QVERIFY(qFuzzyCompare(stats[0].averageWpm, 210.0));

    // Session 2 (start: 2000000)
    QCOMPARE(stats[1].sessionStart, start2);
    QCOMPARE(stats[1].sessionEnd, end2);
    QCOMPARE(stats[1].durationSeconds, duration2);
    QCOMPARE(stats[1].wordsRead, words2);
    QCOMPARE(stats[1].pagesRead, pages2);
    QVERIFY(qFuzzyCompare(stats[1].averageWpm, 240.0));

    // Session 1 (start: 1000000)
    QCOMPARE(stats[2].sessionStart, start1);
    QCOMPARE(stats[2].sessionEnd, end1);
    QCOMPARE(stats[2].durationSeconds, duration1);
    QCOMPARE(stats[2].wordsRead, words1);
    QCOMPARE(stats[2].pagesRead, pages1);
    QVERIFY(qFuzzyCompare(stats[2].averageWpm, 300.0));

    // Edge case: session with 0 duration or 0 words should result in 0.0 WPM without division by zero
    const qint64 startZero = 4000000;
    QVERIFY(db.recordReadingSession(book.id, startZero, startZero, 0, 0, 0));
    const auto statsWithZero = db.getStatsForBook(book.id);
    QCOMPARE(statsWithZero.size(), 4);
    QCOMPARE(statsWithZero[0].sessionStart, startZero);
    QCOMPARE(statsWithZero[0].durationSeconds, 0);
    QCOMPARE(statsWithZero[0].wordsRead, 0);
    QCOMPARE(statsWithZero[0].averageWpm, 0.0);

    // Delete single session
    const QString sessionToDeleteId = statsWithZero[0].id;
    QVERIFY(db.deleteReadingSession(sessionToDeleteId));
    QCOMPARE(db.getSessionCount(book.id), 3);

    // Clear stats for book
    QVERIFY(db.clearStatsForBook(book.id));
    QCOMPARE(db.getStatsForBook(book.id).size(), 0);
    QCOMPARE(db.getTotalReadingTimeSeconds(book.id), 0);
    QCOMPARE(db.getTotalWordsRead(book.id), 0);
    QCOMPARE(db.getAverageWpm(book.id), 0.0);
}

void TestDb::testStatsCascade() {
    DatabaseManager& db = DatabaseManager::instance();

    BookItem book;
    book.id = "book-stats-cascade";
    book.title = "Clean Code";
    book.author = "Robert C. Martin";
    book.filePath = "/books/clean_code.epub";
    book.fileHash = "hash-clean";
    book.format = "epub";
    book.fileSize = 1500;
    QVERIFY(db.insertBook(book));

    QVERIFY(db.recordReadingSession(book.id, 1000, 1500, 500, 1000, 4));
    QVERIFY(db.recordReadingSession(book.id, 2000, 2400, 400, 800, 3));
    QCOMPARE(db.getSessionCount(book.id), 2);
    QCOMPARE(db.getTotalReadingTimeSeconds(), 900);

    // Delete parent book
    QVERIFY(db.deleteBook(book.id));

    // Reading stats must be cascade deleted
    QCOMPARE(db.getStatsForBook(book.id).size(), 0);
    QCOMPARE(db.getTotalReadingTimeSeconds(), 0);
    QCOMPARE(db.getTotalWordsRead(), 0);
    QCOMPARE(db.getSessionCount(), 0);

    QSqlQuery directCheck("SELECT COUNT(*) FROM reading_stats;", QSqlDatabase::database("LunaReadConnection"));
    QVERIFY(directCheck.next());
    QCOMPARE(directCheck.value(0).toInt(), 0);
}

void TestDb::testEdgeCasesAndSpecialCharacters() {
    DatabaseManager& db = DatabaseManager::instance();

    // 1. Book with apostrophe / single quote in title and author
    BookItem bQuote;
    bQuote.id = "book-quote-1";
    bQuote.title = "The Hitchhiker's Guide to the Galaxy";
    bQuote.author = "Eoin Colfer, Douglas Adams' Estate";
    bQuote.series = "Hitchhiker's Series";
    bQuote.seriesIndex = 6.0;
    bQuote.filePath = "/books/hitchhiker's_guide.epub";
    bQuote.fileHash = "hash-quote-1";
    bQuote.format = "epub";
    bQuote.fileSize = 1048576;
    QVERIFY2(db.insertBook(bQuote), "Book with single quotes in title must insert cleanly");

    const auto bQuoteRetrieved = db.getBookById(bQuote.id);
    QVERIFY(bQuoteRetrieved.has_value());
    QCOMPARE(bQuoteRetrieved->title, QString("The Hitchhiker's Guide to the Galaxy"));
    QCOMPARE(bQuoteRetrieved->author, QString("Eoin Colfer, Douglas Adams' Estate"));

    // Verify search with single quote works without SQL error
    const auto searchQuote = db.getBooks("", false, "Hitchhiker's");
    QCOMPARE(searchQuote.size(), 1);
    QCOMPARE(searchQuote[0].id, bQuote.id);

    // 2. Book with SQL injection payload in title and search term
    BookItem bSql;
    bSql.id = "book-sql-inject";
    bSql.title = "Robert'); DROP TABLE books; --";
    bSql.author = "Bobby Tables";
    bSql.filePath = "/books/injection.epub";
    bSql.fileHash = "hash-inject";
    bSql.format = "epub";
    bSql.fileSize = 500;
    QVERIFY2(db.insertBook(bSql), "Book with SQL injection string must insert cleanly");

    // Table books must still exist!
    QCOMPARE(db.getBookCount(), 2);

    // Query with SQL injection payload in search string
    const auto searchInject = db.getBooks("", false, "'); DROP TABLE books; --");
    QCOMPARE(searchInject.size(), 1);
    QCOMPARE(searchInject[0].id, bSql.id);
    QCOMPARE(db.getBookCount(), 2);

    // 3. Book with unicode, Chinese, Russian, Arabic, and emojis
    BookItem bUnicode;
    bUnicode.id = "book-unicode-multi";
    bUnicode.title = "世界和平 🚀 Война и мир • ألف ليلة وليلة (Vol. 1 & 2)";
    bUnicode.author = "罗贯中 & Лев Толстой";
    bUnicode.series = "文学经典 📚";
    bUnicode.seriesIndex = 1.0;
    bUnicode.filePath = "/books/мир_🚀.epub";
    bUnicode.fileHash = "hash-unicode";
    bUnicode.format = "epub";
    bUnicode.fileSize = 888888;
    QVERIFY2(db.insertBook(bUnicode), "Book with unicode and emojis must insert cleanly");

    const auto bUniRetrieved = db.getBookById(bUnicode.id);
    QVERIFY(bUniRetrieved.has_value());
    QCOMPARE(bUniRetrieved->title, QString("世界和平 🚀 Война и мир • ألف ليلة وليلة (Vol. 1 & 2)"));
    QCOMPARE(bUniRetrieved->author, QString("罗贯中 & Лев Толстой"));
    QCOMPARE(bUniRetrieved->series, QString("文学经典 📚"));

    const auto searchEmoji = db.getBooks("", false, "🚀");
    QCOMPARE(searchEmoji.size(), 1);
    QCOMPARE(searchEmoji[0].id, bUnicode.id);

    // 4. Null / empty optional values
    BookItem bEmpty;
    bEmpty.id = "book-empty-fields";
    bEmpty.title = "Minimal Book";
    bEmpty.author = "";
    bEmpty.series = "";
    bEmpty.coverImagePath = "";
    bEmpty.filePath = "/books/minimal.txt";
    bEmpty.fileHash = "hash-minimal";
    bEmpty.format = "txt";
    bEmpty.fileSize = 100;
    bEmpty.seriesIndex = 0.0;
    bEmpty.rating = 0.0;
    QVERIFY(db.insertBook(bEmpty));

    const auto bEmptyRetrieved = db.getBookById(bEmpty.id);
    QVERIFY(bEmptyRetrieved.has_value());
    QCOMPARE(bEmptyRetrieved->author, QString(""));
    QCOMPARE(bEmptyRetrieved->series, QString(""));
    QCOMPARE(bEmptyRetrieved->coverImagePath, QString(""));
    QCOMPARE(bEmptyRetrieved->rating, 0.0);

    // 5. Annotation with multiline text, XML/HTML tags, quotes
    AnnotationRecord aSpecial;
    aSpecial.id = "ann-special-chars";
    aSpecial.bookId = bQuote.id;
    aSpecial.chapterIndex = 3;
    aSpecial.pageIndex = 7;
    aSpecial.characterOffset = 42;
    aSpecial.selectedText = "\"Don't Panic!\"\n\t— Printed in large, friendly letters.";
    aSpecial.note = "<annotation>\n\t<b>Important reminder:</b> Always know where your towel is!\n</annotation>";
    aSpecial.color = QColor(100, 200, 255);
    aSpecial.type = "note";
    aSpecial.createdAt = QDateTime::currentDateTime();
    aSpecial.updatedAt = aSpecial.createdAt;
    QVERIFY(db.insertAnnotation(aSpecial));

    const auto aSpecialRetrieved = db.getAnnotationById(aSpecial.id);
    QVERIFY(aSpecialRetrieved.has_value());
    QCOMPARE(aSpecialRetrieved->selectedText, QString("\"Don't Panic!\"\n\t— Printed in large, friendly letters."));
    QCOMPARE(aSpecialRetrieved->note, QString("<annotation>\n\t<b>Important reminder:</b> Always know where your towel is!\n</annotation>"));

    // 6. Foreign key enforcement: inserting annotation with nonexistent book must fail
    AnnotationRecord aOrphan;
    aOrphan.id = "ann-orphan";
    aOrphan.bookId = "non-existent-book-uuid-9999";
    aOrphan.chapterIndex = 0;
    aOrphan.pageIndex = 0;
    aOrphan.characterOffset = 0;
    aOrphan.selectedText = "Orphan text";
    aOrphan.type = "highlight";
    QVERIFY2(!db.insertAnnotation(aOrphan), "Inserting annotation with invalid foreign key must fail");

    // 7. Foreign key enforcement: recording reading session with nonexistent book must fail
    QVERIFY2(!db.recordReadingSession("non-existent-book-uuid-9999", 1000, 2000, 1000, 500, 2),
             "Recording reading session with invalid foreign key must fail");
}

void TestDb::testTransactions() {
    DatabaseManager& db = DatabaseManager::instance();

    // 1. Transaction rollback
    QVERIFY(db.transaction());

    BookItem bRollback;
    bRollback.id = "book-rollback";
    bRollback.title = "Temporary Ghost Book";
    bRollback.filePath = "/books/ghost.epub";
    bRollback.fileHash = "ghost-hash";
    bRollback.format = "epub";
    bRollback.fileSize = 100;
    QVERIFY(db.insertBook(bRollback));
    QVERIFY(db.getBookById(bRollback.id).has_value());

    QVERIFY(db.rollback());
    QVERIFY(!db.getBookById(bRollback.id).has_value());

    // 2. Transaction commit
    QVERIFY(db.transaction());

    BookItem bCommit1;
    bCommit1.id = "book-commit-1";
    bCommit1.title = "Committed Book One";
    bCommit1.filePath = "/books/commit1.epub";
    bCommit1.fileHash = "commit1-hash";
    bCommit1.format = "epub";
    bCommit1.fileSize = 200;
    QVERIFY(db.insertBook(bCommit1));

    BookItem bCommit2;
    bCommit2.id = "book-commit-2";
    bCommit2.title = "Committed Book Two";
    bCommit2.filePath = "/books/commit2.epub";
    bCommit2.fileHash = "commit2-hash";
    bCommit2.format = "epub";
    bCommit2.fileSize = 300;
    QVERIFY(db.insertBook(bCommit2));

    QVERIFY(db.commit());

    QVERIFY(db.getBookById(bCommit1.id).has_value());
    QVERIFY(db.getBookById(bCommit2.id).has_value());
    QCOMPARE(db.getBookCount(), 2);

    // 3. Clear database
    QVERIFY(db.clearDatabase());
    QCOMPARE(db.getBookCount(), 0);
}

void TestDb::cleanupTestCase() {
    DatabaseManager::instance().close();
}

int runDbTests() {
    TestDb tc;
    return QTest::qExec(&tc);
}

#include "test_db.moc"
