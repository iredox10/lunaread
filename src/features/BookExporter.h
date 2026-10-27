#pragma once

#include <QString>
#include <QMap>
#include <QByteArray>
#include <functional>
#include "../core/Types.h"

namespace LunaRead {

/**
 * @brief Options configuring book export behavior.
 */
struct BookExportOptions {
    bool includeChapterTitles = true;
    bool includeMetadataAndToc = true;
    int startChapter = 0;       // 0-based chapter index
    int endChapter = -1;        // inclusive end index, -1 means up to the last chapter
    QString themeCss = "";      // Custom CSS for HTML export
    std::function<void(int current, int total)> progressCallback = nullptr;
};

/**
 * @brief Multi-format Book Exporter & Converter for LunaRead.
 * 
 * Supports exporting entire books or chapter ranges to:
 * - Plain Text (.txt)
 * - Markdown (.md)
 * - Clean, self-contained HTML (.html)
 */
class BookExporter {
public:
    // Core export methods required by specifications
    static bool exportToTxt(const BookDocument& doc, const QString& outputPath, bool includeChapterTitles = true);
    static bool exportToMarkdown(const BookDocument& doc, const QString& outputPath);
    static bool exportToHtml(const BookDocument& doc, const QString& outputPath, const QString& themeCss = "");
    static bool exportChapter(const BookDocument& doc, int chapterIndex, const QString& outputPath, const QString& format = "txt");

    // Overloads with extended options
    static bool exportToTxt(const BookDocument& doc, const QString& outputPath, const BookExportOptions& options);
    static bool exportToMarkdown(const BookDocument& doc, const QString& outputPath, const BookExportOptions& options);
    static bool exportToHtml(const BookDocument& doc, const QString& outputPath, const BookExportOptions& options);
    static bool exportBook(const BookDocument& doc, const QString& outputPath, const QString& format, const BookExportOptions& options = {});

    // In-memory string content generators
    static QString generateTxt(const BookDocument& doc, const BookExportOptions& options = {});
    static QString generateMarkdown(const BookDocument& doc, const BookExportOptions& options = {});
    static QString generateHtml(const BookDocument& doc, const BookExportOptions& options = {});
    static QString generateChapterContent(const BookDocument& doc, int chapterIndex, const QString& format = "txt");

    // Helpers
    static QString stripHtmlTags(const QString& html);
    static QString decodeHtmlEntities(const QString& text);
    static QString htmlToMarkdown(const QString& html);
    static QString cleanHtmlFragment(const QString& html, const QMap<QString, QByteArray>& images = {});
    static QString mimeTypeFromExtension(const QString& ext);

private:
    static bool writeToFile(const QString& filePath, const QString& content);
    static void normalizeChapterRange(const BookDocument& doc, int& startChapter, int& endChapter);
};

} // namespace LunaRead
