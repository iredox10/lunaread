#include "AnnotationExporter.h"

namespace LunaRead {

QString AnnotationExporter::exportToMarkdown(const QString& bookTitle, const QList<AnnotationRecord>& annotations) {
    QString md = QString("# Annotations for %1\n\n").arg(bookTitle);
    for (const auto& a : annotations) {
        if (a.type == "highlight") {
            md += QString("> ==%1==\n").arg(a.selectedText);
            if (!a.note.isEmpty()) {
                md += QString("\n**Note:** %1\n").arg(a.note);
            }
            md += QString("\n*Chapter %1, Page %2*\n\n---\n\n").arg(a.chapterIndex + 1).arg(a.pageIndex + 1);
        } else if (a.type == "bookmark") {
            md += QString("### 🔖 Bookmark: %1 (Chapter %2, Page %3)\n\n").arg(a.selectedText).arg(a.chapterIndex + 1).arg(a.pageIndex + 1);
        }
    }
    return md;
}

QString AnnotationExporter::exportToHtml(const QString& bookTitle, const QList<AnnotationRecord>& annotations) {
    QString html = QString(R"(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Annotations for %1</title>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; max-width: 800px; margin: 40px auto; padding: 0 20px; color: #222; }
blockquote { border-left: 4px solid #F59E0B; margin: 0 0 16px 0; padding: 12px 16px; background: #FEF3C7; }
.meta { font-size: 0.85em; color: #666; }
</style>
</head>
<body>
<h1>Annotations for %1</h1>
)").arg(bookTitle);

    for (const auto& a : annotations) {
        html += QString("<blockquote><p>%1</p>").arg(a.selectedText.toHtmlEscaped());
        if (!a.note.isEmpty()) {
            html += QString("<p><strong>Note:</strong> %1</p>").arg(a.note.toHtmlEscaped());
        }
        html += QString("<div class=\"meta\">Chapter %1, Page %2</div></blockquote>\n").arg(a.chapterIndex + 1).arg(a.pageIndex + 1);
    }

    html += "</body>\n</html>";
    return html;
}

} // namespace LunaRead
