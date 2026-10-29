#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include <QColor>
#include <QDateTime>
#include <QRectF>
#include <optional>

namespace LunaRead {

/// All document formats supported by Moon+ Reader
enum class BookFormat {
    Epub,
    Pdf,
    Txt,
    Cbz,
    Cbr,
    Mobi,
    Azw,
    Azw3,
    Prc,
    Fb2,
    Docx,
    Odt,
    Rtf,
    Html,
    Htm,
    Chm,
    Djvu,
    Umd,
    Markdown,
    Unknown
};

/// Reading modes
enum class ReadingMode {
    PageCurl,
    HorizontalSlide,
    ContinuousScroll,
    DualPage,
    MangaRtl,
    WebtoonVertical
};

/// The 5 signature Moon+ Reader themes
enum class ReaderThemeMode {
    Parchment,
    OledBlack,
    Sepia,
    Night,
    ModernLight,
    PureBlack = OledBlack,
    NightCharcoal = Night
};

/// Moon+ Reader 9-zone gesture actions
enum class TapAction {
    None,
    PrevPage,
    NextPage,
    OpenMenu,
    ToggleBookmark,
    ToggleTheme,
    OpenToc,
    ShowStats,
    ToggleTts,
    OpenFontSettings,
    OpenSearch
};

/// Hierarchical Table of Contents entry
struct TocItem {
    QString id;
    QString title;
    int chapterIndex = 0;
    int pageIndex = 0;
    QString href;
    int level = 0;
    QVector<TocItem> children;
};

/// Chapter content with text, markup, and embedded images
struct ChapterContent {
    int index = 0;
    QString title;
    QString textContent;
    QString htmlContent;
    QMap<QString, QByteArray> images;
    bool isImagePage = false;
    QByteArray imageBytes;
};

/// A single rendered page calculated by the typesetter
struct ParsedPage {
    int chapterIndex = 0;
    int pageIndexInChapter = 0;
    int globalPageIndex = 0;
    int totalPagesInChapter = 0;
    QString text;
    QString html;
    bool isImagePage = false;
    QByteArray imageBytes;
    int characterStartIndex = 0;
    int characterEndIndex = 0;
};

/// The top-level document model representing an e-book
struct BookDocument {
    QString id;
    QString title;
    QString author;
    QString format;
    QString filePath;
    int totalChapters = 0;
    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;
    QByteArray coverImageBytes;
};

/// Annotation record for bookmarks, highlights, and notes
struct AnnotationRecord {
    QString id;
    QString bookId;
    int chapterIndex = 0;
    int pageIndex = 0;
    int characterOffset = 0;
    QString selectedText;
    QString note;
    QColor color{255, 245, 157}; // Default yellow highlight
    QString type;                // "bookmark", "highlight", "note", "underline", "strikethrough"
    QRectF normalizedRect;       // Normalized bounds (0..1, 0..1) for image/PDF page highlights
    QDateTime createdAt;
    QDateTime updatedAt;
};

/// Reading statistic entry
struct ReadingStatEntry {
    QString id;
    QString bookId;
    qint64 sessionStart = 0;
    qint64 sessionEnd = 0;
    int durationSeconds = 0;
    int wordsRead = 0;
    int pagesRead = 0;
    double averageWpm = 0.0;
};

/// Book metadata item for the library bookshelf
struct BookItem {
    QString id;
    QString title;
    QString author;
    QString series;
    double seriesIndex = 0.0;
    QString filePath;
    qint64 fileSize = 0;
    QString fileHash;
    QString format;
    QString coverImagePath;
    int totalChapters = 0;
    int totalPages = 0;
    int currentChapterIndex = 0;
    int currentPageIndex = 0;
    double progressPercentage = 0.0;
    qint64 lastReadTime = 0;
    qint64 dateAdded = 0;
    bool isFavorite = false;
    double rating = 0.0;
    QString description;
    QString publisher;
    int publishYear = 0;
    QString isbn;
};

/// Vocabulary Notebook entry for language learners
struct VocabularyWord {
    QString id;
    QString word;
    QString phonetic;
    QString definition;
    QString example;
    QString bookId;
    QString bookTitle;
    qint64 dateAdded = 0;
};

} // namespace LunaRead
