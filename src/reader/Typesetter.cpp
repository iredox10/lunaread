#include "Typesetter.h"
#include "../features/TextFilterManager.h"
#include "../features/ImageCropService.h"
#include <QRegularExpression>
#include <QTime>
#include <QUrl>
#include <QAbstractTextDocumentLayout>
#include <QTextCursor>
#include <algorithm>

namespace LunaRead {

Typesetter::Typesetter() {
    m_font = QFont(QStringLiteral("Noto Serif"), 14);
}

void Typesetter::setFont(const QFont& font) {
    m_font = font;
    invalidateCache();
}

void Typesetter::setLineHeight(qreal multiplier) {
    m_lineHeight = std::clamp(multiplier, 1.1, 2.5);
    invalidateCache();
}

void Typesetter::setParagraphSpacing(qreal spacing) {
    m_paragraphSpacing = std::clamp(spacing, 0.0, 32.0);
    invalidateCache();
}

void Typesetter::setMargins(const QMarginsF& margins) {
    m_margins = margins;
    invalidateCache();
}

void Typesetter::setParagraphIndent(qreal indentPx) {
    m_paragraphIndent = std::clamp(indentPx, 0.0, 64.0);
    invalidateCache();
}

void Typesetter::setAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
    invalidateCache();
}

void Typesetter::invalidateCache() {
    m_cachedDoc.reset();
    m_cachedChapterIndex = -1;
    m_cachedContentHtml.clear();
}

std::shared_ptr<QTextDocument> Typesetter::getOrCreateDocument(
    const ChapterContent& chapter,
    const QSizeF& printableSize,
    const ReaderThemeTokens& tokens
) {
    QString tokensKey = tokens.backgroundColor.name() + tokens.textColor.name() + tokens.accentColor.name();

    if (m_cachedDoc &&
        m_cachedChapterIndex == chapter.index &&
        std::abs(m_cachedPrintableSize.width() - printableSize.width()) < 1.0 &&
        std::abs(m_cachedPrintableSize.height() - printableSize.height()) < 1.0 &&
        m_cachedTokensKey == tokensKey) {
        return m_cachedDoc;
    }

    auto doc = std::make_shared<QTextDocument>();
    doc->setDefaultFont(m_font);
    doc->setPageSize(printableSize);

    // Register embedded chapter images
    for (auto it = chapter.images.begin(); it != chapter.images.end(); ++it) {
        QImage img;
        img.loadFromData(it.value());
        if (!img.isNull()) {
            doc->addResource(QTextDocument::ImageResource, QUrl(it.key()), img);
        }
    }

    doc->setDefaultStyleSheet(generateBookStyleSheet(tokens));

    // Choose rich HTML if present and structured, otherwise format raw text into book HTML
    QString contentHtml;
    bool isTrivialHtml = chapter.htmlContent.trimmed().isEmpty() ||
        (!chapter.htmlContent.contains("<p", Qt::CaseInsensitive) && !chapter.htmlContent.contains("<div", Qt::CaseInsensitive)) ||
        (chapter.htmlContent.startsWith("<p>") && chapter.htmlContent.endsWith("</p>") && !chapter.htmlContent.mid(3, chapter.htmlContent.length() - 7).contains("<p>"));

    if (!isTrivialHtml) {
        contentHtml = TextFilterManager::instance().applyFilters(chapter.htmlContent);
    } else {
        QString filteredText = TextFilterManager::instance().applyFilters(chapter.textContent);
        contentHtml = formatChapterToBookHtml(filteredText, chapter.title, m_dropCaps);
    }

    doc->setHtml(contentHtml);

    m_cachedDoc = doc;
    m_cachedChapterIndex = chapter.index;
    m_cachedPrintableSize = printableSize;
    m_cachedTokensKey = tokensKey;
    m_cachedContentHtml = contentHtml;

    return m_cachedDoc;
}

QVector<ParsedPage> Typesetter::paginate(
    const ChapterContent& chapter,
    const QSizeF& viewportSize,
    int startGlobalPageIndex,
    const ReaderThemeTokens& tokens
) {
    QVector<ParsedPage> pages;

    if (chapter.isImagePage) {
        ParsedPage p;
        p.chapterIndex = chapter.index;
        p.pageIndexInChapter = 0;
        p.globalPageIndex = startGlobalPageIndex;
        p.totalPagesInChapter = 1;
        p.isImagePage = true;
        p.imageBytes = chapter.imageBytes;
        pages.append(p);
        return pages;
    }

    qreal sideMargin = std::max(36.0, viewportSize.width() * 0.07);
    qreal topMargin = std::max(36.0, viewportSize.height() * 0.06);
    qreal btmMargin = std::max(40.0, viewportSize.height() * 0.07);

    qreal printableWidth = viewportSize.width() - 2.0 * sideMargin;
    qreal printableHeight = viewportSize.height() - topMargin - btmMargin;

    if (printableWidth <= 50.0 || printableHeight <= 50.0) {
        printableWidth = 600.0;
        printableHeight = 800.0;
    }

    QSizeF printableSize(printableWidth, printableHeight);
    auto doc = getOrCreateDocument(chapter, printableSize, tokens);

    int totalPages = std::max(1, doc->pageCount());
    QString fullPlainText = doc->toPlainText();
    int charsPerPage = (totalPages > 0) ? static_cast<int>(fullPlainText.length() / totalPages) : 0;

    for (int i = 0; i < totalPages; ++i) {
        ParsedPage p;
        p.chapterIndex = chapter.index;
        p.pageIndexInChapter = i;
        p.globalPageIndex = startGlobalPageIndex + i;
        p.totalPagesInChapter = totalPages;
        p.characterStartIndex = i * charsPerPage;
        p.characterEndIndex = std::min(static_cast<int>(fullPlainText.length()), (i + 1) * charsPerPage);
        p.text = fullPlainText.mid(p.characterStartIndex, p.characterEndIndex - p.characterStartIndex).trimmed();
        pages.append(p);
    }

    return pages;
}

void Typesetter::renderPage(
    QPainter* painter,
    const QRectF& pageBounds,
    const ParsedPage& page,
    const ReaderThemeTokens& tokens,
    const QString& bookTitle,
    const QString& chapterTitle,
    bool isVerso,
    const QString& highlightedSentence
) {
    Q_UNUSED(highlightedSentence);
    // 1. Page Background
    painter->fillRect(pageBounds, tokens.backgroundColor);

    // 2. Comic / Scanned Page Rendering
    if (page.isImagePage && !page.imageBytes.isEmpty()) {
        QPixmap pm;
        pm.loadFromData(page.imageBytes);
        if (!pm.isNull()) {
            pm = ImageCropService::instance().cropMargins(pm);
            QRectF contentRect = pageBounds.adjusted(16, 16, -16, -16);
            QPixmap scaled = pm.scaled(contentRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            qreal x = contentRect.left() + (contentRect.width() - scaled.width()) / 2.0;
            qreal y = contentRect.top() + (contentRect.height() - scaled.height()) / 2.0;
            painter->drawPixmap(QPointF(x, y), scaled);
        }
        return;
    }

    // 3. Classical Publishing Page Margins
    qreal sideMargin = std::max(36.0, pageBounds.width() * 0.07);
    qreal topMargin = std::max(36.0, pageBounds.height() * 0.06);
    qreal btmMargin = std::max(40.0, pageBounds.height() * 0.07);

    QRectF textRect(
        pageBounds.left() + sideMargin,
        pageBounds.top() + topMargin,
        pageBounds.width() - 2.0 * sideMargin,
        pageBounds.height() - topMargin - btmMargin
    );

    // 4. Running Header (Top of Page)
    painter->save();
    painter->setPen(tokens.headerFooterColor);
    QFont headerFont(m_font.family(), 9);
    headerFont.setCapitalization(QFont::SmallCaps);
    headerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    painter->setFont(headerFont);

    QRectF headerRect(textRect.left(), pageBounds.top() + 10.0, textRect.width(), 20.0);
    if (isVerso) {
        // Left Page: Book Title
        painter->drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, bookTitle.toUpper());
    } else {
        // Right Page or Single Page: Chapter Title
        painter->drawText(headerRect, Qt::AlignRight | Qt::AlignVCenter, chapterTitle);
    }

    // Subtle header hairline rule
    painter->setPen(QPen(tokens.headerFooterColor, 0.5));
    qreal headerLineY = pageBounds.top() + topMargin - 8.0;
    painter->drawLine(QPointF(textRect.left(), headerLineY), QPointF(textRect.right(), headerLineY));
    painter->restore();

    // Re-style document if theme changed
    QString tokensKey = tokens.backgroundColor.name() + tokens.textColor.name() + tokens.accentColor.name();
    if (m_cachedDoc && m_cachedTokensKey != tokensKey && !m_cachedContentHtml.isEmpty()) {
        m_cachedDoc->setDefaultStyleSheet(generateBookStyleSheet(tokens));
        m_cachedDoc->setHtml(m_cachedContentHtml);
        m_cachedTokensKey = tokensKey;
    }

    // 5. Typeset Document Body Content
    if (m_cachedDoc && m_cachedChapterIndex == page.chapterIndex) {
        painter->save();
        painter->setClipRect(textRect);
        painter->translate(textRect.left(), textRect.top() - (page.pageIndexInChapter * textRect.height()));
        QRectF clip(0, page.pageIndexInChapter * textRect.height(), textRect.width(), textRect.height());
        m_cachedDoc->drawContents(painter, clip);
        painter->restore();
    } else if (!page.text.isEmpty()) {
        // Direct typography fallback
        painter->save();
        painter->setPen(tokens.textColor);
        painter->setFont(m_font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, page.text);
        painter->restore();
    }

    // 6. Running Footer (Bottom of Page)
    painter->save();
    painter->setPen(tokens.headerFooterColor);
    QFont footerFont(m_font.family(), 9);
    footerFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    painter->setFont(footerFont);

    // Subtle footer hairline rule
    painter->setPen(QPen(tokens.headerFooterColor, 0.5));
    qreal footerLineY = pageBounds.bottom() - btmMargin + 8.0;
    painter->drawLine(QPointF(textRect.left(), footerLineY), QPointF(textRect.right(), footerLineY));

    // Footer text bounds
    QRectF footerRect(textRect.left(), pageBounds.bottom() - btmMargin + 14.0, textRect.width(), 20.0);

    // Left: Reading clock
    QString clockStr = QTime::currentTime().toString(QStringLiteral("hh:mm"));
    painter->drawText(footerRect, Qt::AlignLeft | Qt::AlignVCenter, clockStr);

    // Center: Page Number
    int total = std::max(1, page.totalPagesInChapter);
    QString pageStr = QString("— %1 / %2 —").arg(page.pageIndexInChapter + 1).arg(total);
    painter->drawText(footerRect, Qt::AlignCenter, pageStr);

    // Right: Progress %
    double pct = (total > 0) ? ((static_cast<double>(page.pageIndexInChapter + 1) / total) * 100.0) : 0.0;
    QString pctStr = QString("%1%").arg(pct, 0, 'f', 1);
    painter->drawText(footerRect, Qt::AlignRight | Qt::AlignVCenter, pctStr);

    painter->restore();
}

QString Typesetter::formatChapterToBookHtml(const QString& rawText, const QString& title, bool dropCap) {
    if (rawText.isEmpty()) return QString();

    QString html;
    html.append(QStringLiteral("<div class=\"book-chapter\">"));

    if (!title.isEmpty()) {
        html.append(QStringLiteral(
            "<div class=\"chapter-header\">"
            "<h1 class=\"chapter-title\">%1</h1>"
            "<div class=\"header-flourish\">❧ &nbsp; ❖ &nbsp; ❧</div>"
            "</div>"
        ).arg(title.toHtmlEscaped()));
    }

    const QStringList paras = rawText.split(QRegularExpression(R"(\r?\n\s*\r?\n)"), Qt::SkipEmptyParts);
    bool first = true;

    for (const auto& rawPara : paras) {
        QString p = rawPara.trimmed();
        if (p.isEmpty()) continue;

        if (p.startsWith("###") || p.startsWith("---") || p == "***" || p == "* * *") {
            html.append(QStringLiteral("<hr class=\"scene-break\"/>"));
            first = true;
            continue;
        }

        if (p.startsWith("## ") || (p.length() < 50 && (p.startsWith("Chapter", Qt::CaseInsensitive) || p.startsWith("Part", Qt::CaseInsensitive)))) {
            html.append(QStringLiteral("<h2 class=\"sub-title\">%1</h2>").arg(p.remove('#').trimmed().toHtmlEscaped()));
            first = true;
            continue;
        }

        p = p.simplified();

        if (first && dropCap && p.length() > 20) {
            QChar firstChar = p.at(0);
            QString rest = p.mid(1).toHtmlEscaped();
            html.append(QStringLiteral("<p class=\"first-para\"><span class=\"dropcap\">%1</span>%2</p>")
                .arg(QString(firstChar).toHtmlEscaped(), rest));
            first = false;
        } else if (first) {
            html.append(QStringLiteral("<p class=\"first-para\">%1</p>").arg(p.toHtmlEscaped()));
            first = false;
        } else {
            html.append(QStringLiteral("<p>%1</p>").arg(p.toHtmlEscaped()));
        }
    }

    html.append(QStringLiteral("</div>"));
    return html;
}

QString Typesetter::generateBookStyleSheet(const ReaderThemeTokens& tokens) const {
    QString alignStr = QStringLiteral("justify");
    if (m_alignment == Qt::AlignLeft) alignStr = QStringLiteral("left");
    else if (m_alignment == Qt::AlignCenter) alignStr = QStringLiteral("center");
    else if (m_alignment == Qt::AlignRight) alignStr = QStringLiteral("right");

    int ptSize = m_font.pointSize() > 0 ? m_font.pointSize() : 14;
    QString family = m_font.family().isEmpty() ? QStringLiteral("Noto Serif") : m_font.family();

    return QStringLiteral(
        "body, .book-chapter {"
        "  font-family: '%1', 'Noto Serif', 'Liberation Serif', 'Georgia', 'Merriweather', 'Nimbus Roman', serif;"
        "  font-size: %2pt;"
        "  color: %3;"
        "  line-height: %4;"
        "  background: transparent;"
        "  margin: 0;"
        "  padding: 0;"
        "}"
        "p {"
        "  text-align: %5;"
        "  text-indent: %6px;"
        "  margin-top: 0;"
        "  margin-bottom: %7px;"
        "  line-height: %4;"
        "}"
        "p.first-para, p.chapter-first, h1 + p, h2 + p, h3 + p, hr + p {"
        "  text-indent: 0 !important;"
        "}"
        ".dropcap {"
        "  font-size: 2.8em;"
        "  float: left;"
        "  line-height: 0.85em;"
        "  font-weight: bold;"
        "  color: %8;"
        "  padding-right: 6px;"
        "  padding-bottom: 2px;"
        "  font-family: '%1', 'Noto Serif', 'Liberation Serif', 'Georgia', serif;"
        "}"
        "h1, h2, h3, h4 {"
        "  color: %8;"
        "  text-align: center;"
        "  font-weight: bold;"
        "  text-indent: 0;"
        "  margin-top: 1.2em;"
        "  margin-bottom: 0.6em;"
        "  font-family: '%1', 'Noto Serif', 'Liberation Serif', 'Georgia', serif;"
        "}"
        "h1.chapter-title {"
        "  font-size: 1.65em;"
        "  letter-spacing: 0.06em;"
        "  margin-top: 0.5em;"
        "  margin-bottom: 0.3em;"
        "}"
        ".header-flourish {"
        "  text-align: center;"
        "  color: %8;"
        "  font-size: 0.9em;"
        "  opacity: 0.65;"
        "  margin-bottom: 1.5em;"
        "}"
        "blockquote {"
        "  margin: 1.0em 1.6em;"
        "  padding: 0.4em 0.8em;"
        "  border-left: 3px solid %8;"
        "  font-style: italic;"
        "  opacity: 0.92;"
        "  text-indent: 0;"
        "}"
        "hr, .scene-break {"
        "  border: 0;"
        "  border-top: 1px solid %8;"
        "  width: 30%;"
        "  margin: 1.8em auto;"
        "  opacity: 0.4;"
        "}"
        "b, strong {"
        "  font-weight: bold;"
        "}"
        "i, em {"
        "  font-style: italic;"
        "}"
        "img {"
        "  max-width: 95%;"
        "  height: auto;"
        "  display: block;"
        "  margin: 0.8em auto;"
        "}"
    ).arg(family)
     .arg(ptSize)
     .arg(tokens.textColor.name())
     .arg(m_lineHeight)
     .arg(alignStr)
     .arg(m_paragraphIndent)
     .arg(m_paragraphSpacing)
     .arg(tokens.accentColor.name());
}

} // namespace LunaRead
