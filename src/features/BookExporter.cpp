#include "BookExporter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace LunaRead {

void BookExporter::normalizeChapterRange(const BookDocument& doc, int& startChapter, int& endChapter) {
    const int total = static_cast<int>(doc.chapters.size());
    if (total == 0) {
        startChapter = 0;
        endChapter = -1;
        return;
    }
    if (startChapter < 0) {
        startChapter = 0;
    }
    if (endChapter < 0 || endChapter >= total) {
        endChapter = total - 1;
    }
    if (startChapter > endChapter) {
        std::swap(startChapter, endChapter);
    }
}

template <typename Fn>
static QString regexReplaceAll(const QString& str, const QRegularExpression& re, Fn&& fn) {
    QString result;
    result.reserve(str.size());
    qsizetype lastPos = 0;
    auto it = re.globalMatch(str);
    while (it.hasNext()) {
        auto m = it.next();
        result.append(str.mid(lastPos, m.capturedStart() - lastPos));
        result.append(fn(m));
        lastPos = m.capturedEnd();
    }
    result.append(str.mid(lastPos));
    return result;
}

QString BookExporter::decodeHtmlEntities(const QString& text) {
    QString out = text;
    out.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    out.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    out.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    out.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    out.replace(QStringLiteral("&mdash;"), QString::fromUtf8("—"));
    out.replace(QStringLiteral("&ndash;"), QString::fromUtf8("–"));
    out.replace(QStringLiteral("&hellip;"), QString::fromUtf8("…"));
    out.replace(QStringLiteral("&copy;"), QString::fromUtf8("©"));
    out.replace(QStringLiteral("&reg;"), QString::fromUtf8("®"));

    // Handle decimal entities &#123;
    static const QRegularExpression decRe(QStringLiteral("&#([0-9]{1,7});"));
    out = regexReplaceAll(out, decRe, [](const QRegularExpressionMatch& m) {
        bool ok = false;
        char32_t cp = m.captured(1).toUInt(&ok);
        if (ok && cp > 0 && cp < 0x110000) {
            return QString::fromUcs4(&cp, 1);
        }
        return m.captured(0);
    });

    // Handle hex entities &#x1F600;
    static const QRegularExpression hexRe(QStringLiteral("&#[xX]([0-9a-fA-F]{1,6});"));
    out = regexReplaceAll(out, hexRe, [](const QRegularExpressionMatch& m) {
        bool ok = false;
        char32_t cp = m.captured(1).toUInt(&ok, 16);
        if (ok && cp > 0 && cp < 0x110000) {
            return QString::fromUcs4(&cp, 1);
        }
        return m.captured(0);
    });

    return out;
}

QString BookExporter::stripHtmlTags(const QString& html) {
    if (html.isEmpty()) return QString();

    QString result = html;
    // Strip scripts, styles, and head
    static const QRegularExpression scriptRe(QStringLiteral("<script\\b[^<]*(?:(?!<\\/script>)<[^<]*)*<\\/script>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression styleRe(QStringLiteral("<style\\b[^<]*(?:(?!<\\/style>)<[^<]*)*<\\/style>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression headRe(QStringLiteral("<head\\b[^<]*(?:(?!<\\/head>)<[^<]*)*<\\/head>"), QRegularExpression::CaseInsensitiveOption);
    result.remove(scriptRe);
    result.remove(styleRe);
    result.remove(headRe);

    // Line breaks and block elements to newlines
    static const QRegularExpression brRe(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    result.replace(brRe, QStringLiteral("\n"));

    static const QRegularExpression blockRe(QStringLiteral("</(?:p|div|h[1-6]|li|blockquote|section|article|tr)>"), QRegularExpression::CaseInsensitiveOption);
    result.replace(blockRe, QStringLiteral("\n\n"));

    // Remove remaining HTML tags
    static const QRegularExpression anyTag(QStringLiteral("<[^>]+>"));
    result.remove(anyTag);

    // Decode entities
    result = decodeHtmlEntities(result);

    // Normalize whitespace and consecutive newlines
    QStringList lines = result.split(QLatin1Char('\n'));
    QString normalized;
    int consecutiveEmpty = 0;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            if (consecutiveEmpty < 1 && !normalized.isEmpty()) {
                normalized += QLatin1Char('\n');
                consecutiveEmpty++;
            }
        } else {
            normalized += trimmed + QLatin1Char('\n');
            consecutiveEmpty = 0;
        }
    }
    return normalized.trimmed();
}

QString BookExporter::htmlToMarkdown(const QString& html) {
    if (html.isEmpty()) return QString();

    QString md = html;

    // Strip scripts, styles, head
    static const QRegularExpression scriptRe(QStringLiteral("<script\\b[^<]*(?:(?!<\\/script>)<[^<]*)*<\\/script>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression styleRe(QStringLiteral("<style\\b[^<]*(?:(?!<\\/style>)<[^<]*)*<\\/style>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression headRe(QStringLiteral("<head\\b[^<]*(?:(?!<\\/head>)<[^<]*)*<\\/head>"), QRegularExpression::CaseInsensitiveOption);
    md.remove(scriptRe);
    md.remove(styleRe);
    md.remove(headRe);

    // Headings
    static const QRegularExpression h1Re(QStringLiteral("<h1[^>]*>(.*?)<\\/h1>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression h2Re(QStringLiteral("<h2[^>]*>(.*?)<\\/h2>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression h3Re(QStringLiteral("<h3[^>]*>(.*?)<\\/h3>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression h4Re(QStringLiteral("<h4[^>]*>(.*?)<\\/h4>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression h5Re(QStringLiteral("<h5[^>]*>(.*?)<\\/h5>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression h6Re(QStringLiteral("<h6[^>]*>(.*?)<\\/h6>"), QRegularExpression::CaseInsensitiveOption);

    md.replace(h1Re, QStringLiteral("\n\n# \\1\n\n"));
    md.replace(h2Re, QStringLiteral("\n\n## \\1\n\n"));
    md.replace(h3Re, QStringLiteral("\n\n### \\1\n\n"));
    md.replace(h4Re, QStringLiteral("\n\n#### \\1\n\n"));
    md.replace(h5Re, QStringLiteral("\n\n##### \\1\n\n"));
    md.replace(h6Re, QStringLiteral("\n\n###### \\1\n\n"));

    // Bold
    static const QRegularExpression boldRe(QStringLiteral("<(?:strong|b)[^>]*>(.*?)<\\/(?:strong|b)>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(boldRe, QStringLiteral("**\\1**"));

    // Italic
    static const QRegularExpression emRe(QStringLiteral("<(?:em|i)[^>]*>(.*?)<\\/(?:em|i)>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(emRe, QStringLiteral("*\\1*"));

    // Code
    static const QRegularExpression codeRe(QStringLiteral("<code[^>]*>(.*?)<\\/code>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(codeRe, QStringLiteral("`\\1`"));

    // Preformatted blocks
    static const QRegularExpression preRe(QStringLiteral("<pre[^>]*>(.*?)<\\/pre>"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    md.replace(preRe, QStringLiteral("\n\n```\n\\1\n```\n\n"));

    // Images
    static const QRegularExpression imgRe(QStringLiteral("<img\\s+[^>]*src=[\"']([^\"']+)[\"'][^>]*(?:alt=[\"']([^\"']*)[\"'])?[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(imgRe, QStringLiteral("\n\n![\\2](\\1)\n\n"));

    // Links
    static const QRegularExpression aRe(QStringLiteral("<a\\s+[^>]*href=[\"']([^\"']+)[\"'][^>]*>(.*?)<\\/a>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(aRe, QStringLiteral("[\\2](\\1)"));

    // Blockquotes
    static const QRegularExpression bqRe(QStringLiteral("<blockquote[^>]*>(.*?)<\\/blockquote>"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    md = regexReplaceAll(md, bqRe, [](const QRegularExpressionMatch& m) {
        QString inner = m.captured(1).trimmed();
        QStringList lines = inner.split(QLatin1Char('\n'));
        QString quoted;
        for (const QString& line : lines) {
            quoted += QStringLiteral("> ") + line.trimmed() + QLatin1Char('\n');
        }
        return QStringLiteral("\n\n") + quoted + QStringLiteral("\n\n");
    });

    // Lists
    static const QRegularExpression liRe(QStringLiteral("<li[^>]*>(.*?)<\\/li>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(liRe, QStringLiteral("- \\1\n"));

    // Paragraphs and breaks
    static const QRegularExpression brRe(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(brRe, QStringLiteral("  \n"));

    static const QRegularExpression pRe(QStringLiteral("<p[^>]*>(.*?)<\\/p>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(pRe, QStringLiteral("\n\n\\1\n\n"));

    static const QRegularExpression divRe(QStringLiteral("<div[^>]*>(.*?)<\\/div>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(divRe, QStringLiteral("\n\n\\1\n\n"));

    static const QRegularExpression hrRe(QStringLiteral("<hr\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    md.replace(hrRe, QStringLiteral("\n\n---\n\n"));

    // Remove remaining tags
    static const QRegularExpression anyTag(QStringLiteral("<[^>]+>"));
    md.remove(anyTag);

    // Decode entities
    md = decodeHtmlEntities(md);

    // Normalize multiple newlines
    static const QRegularExpression multiNewlines(QStringLiteral("\n{3,}"));
    md.replace(multiNewlines, QStringLiteral("\n\n"));

    return md.trimmed();
}

QString BookExporter::mimeTypeFromExtension(const QString& ext) {
    const QString e = ext.toLower();
    if (e == "jpg" || e == "jpeg") return QStringLiteral("image/jpeg");
    if (e == "png") return QStringLiteral("image/png");
    if (e == "gif") return QStringLiteral("image/gif");
    if (e == "webp") return QStringLiteral("image/webp");
    if (e == "svg") return QStringLiteral("image/svg+xml");
    if (e == "bmp") return QStringLiteral("image/bmp");
    return QStringLiteral("image/jpeg");
}

QString BookExporter::cleanHtmlFragment(const QString& html, const QMap<QString, QByteArray>& images) {
    if (html.isEmpty()) return QString();

    QString content = html;

    // If whole document, extract <body> content
    static const QRegularExpression bodyRe(QStringLiteral("<body[^>]*>(.*?)<\\/body>"),
                                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto bodyMatch = bodyRe.match(content);
    if (bodyMatch.hasMatch()) {
        content = bodyMatch.captured(1);
    }

    // Strip scripts and styles
    static const QRegularExpression scriptRe(QStringLiteral("<script\\b[^<]*(?:(?!<\\/script>)<[^<]*)*<\\/script>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression styleRe(QStringLiteral("<style\\b[^<]*(?:(?!<\\/style>)<[^<]*)*<\\/style>"), QRegularExpression::CaseInsensitiveOption);
    content.remove(scriptRe);
    content.remove(styleRe);

    // Inline images as data URLs if present in image map
    if (!images.isEmpty()) {
        for (auto it = images.cbegin(); it != images.cend(); ++it) {
            const QString& key = it.key();
            const QByteArray& data = it.value();
            if (data.isEmpty()) continue;

            const QString ext = QFileInfo(key).suffix().toLower();
            const QString mime = mimeTypeFromExtension(ext);
            const QString dataUri = QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(data.toBase64()));

            const QString keyBase = QFileInfo(key).fileName();
            content.replace(QStringLiteral("\"%1\"").arg(key), QStringLiteral("\"%1\"").arg(dataUri));
            content.replace(QStringLiteral("'%1'").arg(key), QStringLiteral("\"%1\"").arg(dataUri));
            if (keyBase != key) {
                content.replace(QStringLiteral("\"%1\"").arg(keyBase), QStringLiteral("\"%1\"").arg(dataUri));
                content.replace(QStringLiteral("'%1'").arg(keyBase), QStringLiteral("\"%1\"").arg(dataUri));
            }
        }
    }

    return content.trimmed();
}

bool BookExporter::writeToFile(const QString& filePath, const QString& content) {
    if (filePath.isEmpty()) return false;

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QByteArray utf8 = content.toUtf8();
    qint64 bytesWritten = file.write(utf8);
    file.close();

    return bytesWritten == utf8.size();
}

QString BookExporter::generateTxt(const BookDocument& doc, const BookExportOptions& options) {
    int start = options.startChapter;
    int end = options.endChapter;
    normalizeChapterRange(doc, start, end);

    QString result;
    const QString bookTitle = doc.title.isEmpty() ? QStringLiteral("Untitled Document") : doc.title;
    const QString bookAuthor = doc.author.isEmpty() ? QStringLiteral("Unknown Author") : doc.author;

    if (options.includeMetadataAndToc) {
        result += QStringLiteral("================================================================================\n");
        result += bookTitle + QStringLiteral("\n");
        result += QStringLiteral("Author: ") + bookAuthor + QStringLiteral("\n");
        if (!doc.format.isEmpty()) {
            result += QStringLiteral("Original Format: ") + doc.format.toUpper() + QStringLiteral("\n");
        }
        result += QStringLiteral("Exported with LunaRead\n");
        result += QStringLiteral("================================================================================\n\n");

        if (start <= end && end < doc.chapters.size()) {
            result += QStringLiteral("TABLE OF CONTENTS\n");
            result += QStringLiteral("--------------------------------------------------------------------------------\n");
            for (int i = start; i <= end; ++i) {
                const auto& ch = doc.chapters[i];
                QString title = ch.title.trimmed().isEmpty() ? QStringLiteral("Chapter %1").arg(i + 1) : ch.title.trimmed();
                result += QStringLiteral("%1. %2\n").arg(i + 1).arg(title);
            }
            result += QStringLiteral("================================================================================\n\n\n");
        }
    }

    const int totalSteps = (end >= start) ? (end - start + 1) : 1;
    for (int i = start; i <= end && i < doc.chapters.size(); ++i) {
        const auto& ch = doc.chapters[i];
        const QString chTitle = ch.title.trimmed().isEmpty() ? QStringLiteral("Chapter %1").arg(i + 1) : ch.title.trimmed();

        if (options.includeChapterTitles) {
            result += QStringLiteral("--------------------------------------------------------------------------------\n");
            result += chTitle + QStringLiteral("\n");
            result += QStringLiteral("--------------------------------------------------------------------------------\n\n");
        }

        QString body;
        if (!ch.textContent.trimmed().isEmpty()) {
            body = ch.textContent.trimmed();
        } else if (!ch.htmlContent.trimmed().isEmpty()) {
            body = stripHtmlTags(ch.htmlContent);
        } else if (ch.isImagePage) {
            body = QStringLiteral("[Image Page]");
        }

        result += body + QStringLiteral("\n\n\n");

        if (options.progressCallback) {
            options.progressCallback(i - start + 1, totalSteps);
        }
    }

    return result.trimmed() + QStringLiteral("\n");
}

QString BookExporter::generateMarkdown(const BookDocument& doc, const BookExportOptions& options) {
    int start = options.startChapter;
    int end = options.endChapter;
    normalizeChapterRange(doc, start, end);

    QString result;
    const QString bookTitle = doc.title.isEmpty() ? QStringLiteral("Untitled Document") : doc.title;
    const QString bookAuthor = doc.author.isEmpty() ? QStringLiteral("Unknown Author") : doc.author;

    if (options.includeMetadataAndToc) {
        result += QStringLiteral("# %1\n\n").arg(bookTitle);
        result += QStringLiteral("**Author:** %1  \n").arg(bookAuthor);
        if (!doc.format.isEmpty()) {
            result += QStringLiteral("**Format:** %1  \n").arg(doc.format.toUpper());
        }
        result += QStringLiteral("*Exported with LunaRead*\n\n");
        result += QStringLiteral("---\n\n");

        if (start <= end && end < doc.chapters.size()) {
            result += QStringLiteral("## Table of Contents\n\n");
            for (int i = start; i <= end; ++i) {
                const auto& ch = doc.chapters[i];
                QString title = ch.title.trimmed().isEmpty() ? QStringLiteral("Chapter %1").arg(i + 1) : ch.title.trimmed();
                result += QStringLiteral("- [%1](#chapter-%2)\n").arg(title).arg(i + 1);
            }
            result += QStringLiteral("\n---\n\n");
        }
    }

    const int totalSteps = (end >= start) ? (end - start + 1) : 1;
    for (int i = start; i <= end && i < doc.chapters.size(); ++i) {
        const auto& ch = doc.chapters[i];
        const QString chTitle = ch.title.trimmed().isEmpty() ? QStringLiteral("Chapter %1").arg(i + 1) : ch.title.trimmed();

        if (options.includeChapterTitles) {
            result += QStringLiteral("<a id=\"chapter-%1\"></a>\n\n").arg(i + 1);
            result += QStringLiteral("## %1\n\n").arg(chTitle);
        }

        QString body;
        if (!ch.htmlContent.trimmed().isEmpty()) {
            body = htmlToMarkdown(ch.htmlContent);
        } else if (!ch.textContent.trimmed().isEmpty()) {
            body = ch.textContent.trimmed();
        } else if (ch.isImagePage) {
            body = QStringLiteral("*[Image Page]*");
        }

        result += body + QStringLiteral("\n\n");

        if (options.progressCallback) {
            options.progressCallback(i - start + 1, totalSteps);
        }
    }

    return result.trimmed() + QStringLiteral("\n");
}

QString BookExporter::generateHtml(const BookDocument& doc, const BookExportOptions& options) {
    int start = options.startChapter;
    int end = options.endChapter;
    normalizeChapterRange(doc, start, end);

    const QString bookTitle = doc.title.isEmpty() ? QStringLiteral("Untitled Document") : doc.title;
    const QString bookAuthor = doc.author.isEmpty() ? QStringLiteral("Unknown Author") : doc.author;

    QString defaultCss = QStringLiteral(R"(
    :root {
      --bg-color: #ffffff;
      --text-color: #24292f;
      --heading-color: #0f172a;
      --link-color: #2563eb;
      --border-color: #e2e8f0;
      --panel-bg: #f8fafc;
      --quote-border: #3b82f6;
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg-color: #0f172a;
        --text-color: #cbd5e1;
        --heading-color: #f8fafc;
        --link-color: #60a5fa;
        --border-color: #334155;
        --panel-bg: #1e293b;
        --quote-border: #60a5fa;
      }
    }
    html {
      box-sizing: border-box;
    }
    *, *:before, *:after {
      box-sizing: inherit;
    }
    body {
      background-color: var(--bg-color);
      color: var(--text-color);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      font-size: 1.125rem;
      line-height: 1.8;
      margin: 0;
      padding: 0;
    }
    .book-container {
      max-width: 820px;
      margin: 0 auto;
      padding: 48px 24px 96px 24px;
    }
    header.book-header {
      margin-bottom: 48px;
      padding-bottom: 32px;
      border-bottom: 2px solid var(--border-color);
      text-align: center;
    }
    h1.book-title {
      font-size: 2.5rem;
      font-weight: 700;
      color: var(--heading-color);
      margin: 0 0 12px 0;
      line-height: 1.25;
    }
    p.book-author {
      font-size: 1.25rem;
      color: #64748b;
      margin: 0;
      font-style: italic;
    }
    nav.book-toc {
      background-color: var(--panel-bg);
      border: 1px solid var(--border-color);
      border-radius: 8px;
      padding: 24px 32px;
      margin-bottom: 48px;
    }
    nav.book-toc h2 {
      margin-top: 0;
      font-size: 1.4rem;
      color: var(--heading-color);
      border-bottom: 1px solid var(--border-color);
      padding-bottom: 10px;
      margin-bottom: 16px;
    }
    nav.book-toc ol {
      margin: 0;
      padding-left: 24px;
    }
    nav.book-toc li {
      margin-bottom: 8px;
    }
    a {
      color: var(--link-color);
      text-decoration: none;
    }
    a:hover {
      text-decoration: underline;
    }
    section.book-chapter {
      margin-bottom: 64px;
      padding-top: 24px;
      border-bottom: 1px solid var(--border-color);
      padding-bottom: 48px;
    }
    h2.chapter-title {
      font-size: 1.85rem;
      font-weight: 600;
      color: var(--heading-color);
      margin: 0 0 24px 0;
      padding-bottom: 8px;
      border-bottom: 2px solid var(--border-color);
    }
    p {
      margin: 1.2em 0;
      text-align: justify;
      hyphens: auto;
    }
    blockquote {
      border-left: 4px solid var(--quote-border);
      background-color: var(--panel-bg);
      margin: 24px 0;
      padding: 14px 20px;
      font-style: italic;
      border-radius: 0 6px 6px 0;
    }
    img {
      max-width: 100%;
      height: auto;
      display: block;
      margin: 28px auto;
      border-radius: 6px;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
    }
    footer.book-footer {
      margin-top: 64px;
      padding-top: 24px;
      border-top: 1px solid var(--border-color);
      text-align: center;
      font-size: 0.875rem;
      color: #94a3b8;
    }
)");

    if (!options.themeCss.trimmed().isEmpty()) {
        defaultCss += QStringLiteral("\n/* Custom Theme CSS */\n") + options.themeCss.trimmed() + QStringLiteral("\n");
    }

    QString html;
    html += QStringLiteral("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
    html += QStringLiteral("  <meta charset=\"utf-8\">\n");
    html += QStringLiteral("  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    html += QStringLiteral("  <meta name=\"author\" content=\"%1\">\n").arg(bookAuthor.toHtmlEscaped());
    html += QStringLiteral("  <meta name=\"generator\" content=\"LunaRead Desktop\">\n");
    html += QStringLiteral("  <title>%1</title>\n").arg(bookTitle.toHtmlEscaped());
    html += QStringLiteral("  <style>\n%1\n  </style>\n").arg(defaultCss);
    html += QStringLiteral("</head>\n<body>\n");
    html += QStringLiteral("  <article class=\"book-container\">\n");

    if (options.includeMetadataAndToc) {
        html += QStringLiteral("    <header class=\"book-header\">\n");
        html += QStringLiteral("      <h1 class=\"book-title\">%1</h1>\n").arg(bookTitle.toHtmlEscaped());
        html += QStringLiteral("      <p class=\"book-author\">By %1</p>\n").arg(bookAuthor.toHtmlEscaped());
        html += QStringLiteral("    </header>\n\n");

        if (start <= end && end < doc.chapters.size()) {
            html += QStringLiteral("    <nav class=\"book-toc\">\n");
            html += QStringLiteral("      <h2>Table of Contents</h2>\n");
            html += QStringLiteral("      <ol>\n");
            for (int i = start; i <= end; ++i) {
                const auto& ch = doc.chapters[i];
                QString title = ch.title.trimmed().isEmpty() ? QStringLiteral("Chapter %1").arg(i + 1) : ch.title.trimmed();
                html += QStringLiteral("        <li><a href=\"#chapter-%1\">%2</a></li>\n").arg(i + 1).arg(title.toHtmlEscaped());
            }
            html += QStringLiteral("      </ol>\n");
            html += QStringLiteral("    </nav>\n\n");
        }
    }

    const int totalSteps = (end >= start) ? (end - start + 1) : 1;
    for (int i = start; i <= end && i < doc.chapters.size(); ++i) {
        const auto& ch = doc.chapters[i];
        const QString chTitle = ch.title.trimmed().isEmpty() ? QStringLiteral("Chapter %1").arg(i + 1) : ch.title.trimmed();

        html += QStringLiteral("    <section class=\"book-chapter\" id=\"chapter-%1\">\n").arg(i + 1);

        if (options.includeChapterTitles) {
            html += QStringLiteral("      <h2 class=\"chapter-title\">%1</h2>\n").arg(chTitle.toHtmlEscaped());
        }

        if (!ch.htmlContent.trimmed().isEmpty()) {
            html += QStringLiteral("      <div class=\"chapter-content\">\n");
            html += cleanHtmlFragment(ch.htmlContent, ch.images) + QStringLiteral("\n");
            html += QStringLiteral("      </div>\n");
        } else if (!ch.textContent.trimmed().isEmpty()) {
            html += QStringLiteral("      <div class=\"chapter-content\">\n");
            QStringList paras = ch.textContent.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
            for (const QString& para : paras) {
                QString cleaned = para.trimmed().toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
                if (!cleaned.isEmpty()) {
                    html += QStringLiteral("        <p>%1</p>\n").arg(cleaned);
                }
            }
            html += QStringLiteral("      </div>\n");
        } else if (ch.isImagePage && !ch.imageBytes.isEmpty()) {
            const QString mime = QStringLiteral("image/jpeg");
            html += QStringLiteral("      <img src=\"data:%1;base64,%2\" alt=\"%3\" />\n")
                        .arg(mime, QString::fromLatin1(ch.imageBytes.toBase64()), chTitle.toHtmlEscaped());
        }

        html += QStringLiteral("    </section>\n\n");

        if (options.progressCallback) {
            options.progressCallback(i - start + 1, totalSteps);
        }
    }

    html += QStringLiteral("    <footer class=\"book-footer\">\n");
    html += QStringLiteral("      <p>Generated by LunaRead Desktop Reader</p>\n");
    html += QStringLiteral("    </footer>\n");
    html += QStringLiteral("  </article>\n");
    html += QStringLiteral("</body>\n</html>\n");

    return html;
}

QString BookExporter::generateChapterContent(const BookDocument& doc, int chapterIndex, const QString& format) {
    if (chapterIndex < 0 || chapterIndex >= doc.chapters.size()) {
        return QString();
    }
    BookExportOptions options;
    options.startChapter = chapterIndex;
    options.endChapter = chapterIndex;
    options.includeMetadataAndToc = false;
    options.includeChapterTitles = false;

    QString fmt = format.toLower().trimmed();
    if (fmt.startsWith(QLatin1Char('.'))) fmt.remove(0, 1);

    if (fmt == QLatin1String("md") || fmt == QLatin1String("markdown")) {
        return generateMarkdown(doc, options);
    } else if (fmt == QLatin1String("html") || fmt == QLatin1String("htm")) {
        return generateHtml(doc, options);
    } else {
        return generateTxt(doc, options);
    }
}

bool BookExporter::exportToTxt(const BookDocument& doc, const QString& outputPath, bool includeChapterTitles) {
    BookExportOptions options;
    options.includeChapterTitles = includeChapterTitles;
    options.includeMetadataAndToc = true;
    return exportToTxt(doc, outputPath, options);
}

bool BookExporter::exportToTxt(const BookDocument& doc, const QString& outputPath, const BookExportOptions& options) {
    QString content = generateTxt(doc, options);
    return writeToFile(outputPath, content);
}

bool BookExporter::exportToMarkdown(const BookDocument& doc, const QString& outputPath) {
    BookExportOptions options;
    return exportToMarkdown(doc, outputPath, options);
}

bool BookExporter::exportToMarkdown(const BookDocument& doc, const QString& outputPath, const BookExportOptions& options) {
    QString content = generateMarkdown(doc, options);
    return writeToFile(outputPath, content);
}

bool BookExporter::exportToHtml(const BookDocument& doc, const QString& outputPath, const QString& themeCss) {
    BookExportOptions options;
    options.themeCss = themeCss;
    return exportToHtml(doc, outputPath, options);
}

bool BookExporter::exportToHtml(const BookDocument& doc, const QString& outputPath, const BookExportOptions& options) {
    QString content = generateHtml(doc, options);
    return writeToFile(outputPath, content);
}

bool BookExporter::exportChapter(const BookDocument& doc, int chapterIndex, const QString& outputPath, const QString& format) {
    if (chapterIndex < 0 || chapterIndex >= doc.chapters.size()) {
        return false;
    }
    BookExportOptions options;
    options.startChapter = chapterIndex;
    options.endChapter = chapterIndex;
    options.includeMetadataAndToc = false;
    options.includeChapterTitles = true;

    return exportBook(doc, outputPath, format, options);
}

bool BookExporter::exportBook(const BookDocument& doc, const QString& outputPath, const QString& format, const BookExportOptions& options) {
    QString fmt = format.toLower().trimmed();
    if (fmt.startsWith(QLatin1Char('.'))) fmt.remove(0, 1);

    if (fmt == QLatin1String("md") || fmt == QLatin1String("markdown")) {
        return exportToMarkdown(doc, outputPath, options);
    } else if (fmt == QLatin1String("html") || fmt == QLatin1String("htm")) {
        return exportToHtml(doc, outputPath, options);
    } else {
        return exportToTxt(doc, outputPath, options);
    }
}

} // namespace LunaRead
