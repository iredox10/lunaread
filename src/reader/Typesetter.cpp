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
    m_docCache.clear();
    m_docCacheOrder.clear();
}

std::shared_ptr<QTextDocument> Typesetter::documentFor(int chapterIndex) const {
    auto it = m_docCache.constFind(chapterIndex);
    return it != m_docCache.constEnd() ? it->doc : nullptr;
}

QRectF Typesetter::calculateImageTargetRect(const QRectF& pageBounds, const QSize& imgSize, PageFitMode fitMode) {
    if (imgSize.isEmpty()) return pageBounds;
    QRectF contentRect = (fitMode == PageFitMode::FitWidth || fitMode == PageFitMode::FillScreen)
        ? pageBounds
        : pageBounds.adjusted(8, 8, -8, -8);

    qreal scale = 1.0;
    if (fitMode == PageFitMode::FitWidth) {
        scale = contentRect.width() / static_cast<qreal>(imgSize.width());
    } else if (fitMode == PageFitMode::FillScreen) {
        scale = std::max(contentRect.width() / static_cast<qreal>(imgSize.width()),
                         contentRect.height() / static_cast<qreal>(imgSize.height()));
    } else {
        scale = std::min(contentRect.width() / static_cast<qreal>(imgSize.width()),
                         contentRect.height() / static_cast<qreal>(imgSize.height()));
    }

    qreal drawW = imgSize.width() * scale;
    qreal drawH = imgSize.height() * scale;
    qreal drawX = contentRect.left() + (contentRect.width() - drawW) / 2.0;
    qreal drawY = contentRect.top() + (contentRect.height() - drawH) / 2.0;
    return QRectF(drawX, drawY, drawW, drawH);
}

QRectF Typesetter::imageTargetRect(const QRectF& pageBounds, const QSize& imgSize) const {
    if (imgSize.isEmpty()) return pageBounds;
    if (m_fitMode == PageFitMode::FitPage) {
        return calculateImageTargetRect(pageBounds, imgSize, PageFitMode::FitPage);
    }

    // Width-filling modes start at the top of the page and pan downward instead
    // of cropping the overflow symmetrically.
    const qreal widthScale = pageBounds.width() / static_cast<qreal>(imgSize.width());
    const qreal scale = (m_fitMode == PageFitMode::FitWidth)
        ? widthScale
        : std::max(widthScale, pageBounds.height() / static_cast<qreal>(imgSize.height()));

    const qreal drawW = imgSize.width() * scale;
    const qreal drawH = imgSize.height() * scale;
    const qreal drawX = pageBounds.left() + (pageBounds.width() - drawW) / 2.0;
    const qreal maxPan = std::max(0.0, drawH - pageBounds.height());
    const qreal drawY = pageBounds.top() - std::clamp(m_imagePanY, 0.0, maxPan);
    return QRectF(drawX, drawY, drawW, drawH);
}

qreal Typesetter::imagePanRange(const ParsedPage& page, const QRectF& pageBounds) {
    if (!page.isImagePage || page.imageBytes.isEmpty()) return 0.0;
    if (m_fitMode == PageFitMode::FitPage) return 0.0;

    const QPixmap pm = preparedPagePixmap(page.chapterIndex, page.imageBytes);
    if (pm.isNull()) return 0.0;
    return std::max(0.0, imageTargetRect(pageBounds, pm.size()).height() - pageBounds.height());
}

QPixmap Typesetter::preparedPagePixmap(int chapterIndex, const QByteArray& imageBytes) {
    auto cached = m_pixmapCache.find(chapterIndex);
    if (cached != m_pixmapCache.end() &&
        cached->dataKey == imageBytes.constData() &&
        cached->dataSize == imageBytes.size()) {
        return cached->pixmap;
    }

    QPixmap pm;
    if (!pm.loadFromData(imageBytes)) return pm;
    pm = ImageCropService::instance().cropMargins(pm);

    m_pixmapCache.insert(chapterIndex, CachedPagePixmap{pm, imageBytes.constData(), imageBytes.size()});
    m_pixmapCacheOrder.removeAll(chapterIndex);
    m_pixmapCacheOrder.append(chapterIndex);
    // Each 300 DPI page raster is tens of megabytes, so keep only the current
    // page and its spread neighbour cacheable.
    while (m_pixmapCacheOrder.size() > 2) {
        m_pixmapCache.remove(m_pixmapCacheOrder.takeFirst());
    }
    return pm;
}

void Typesetter::applyPageTint(QPainter* painter, const QRectF& rect, const ReaderThemeTokens& tokens) {
    if (tokens.id == QLatin1String("modernLight")) return;

    painter->save();
    if (tokens.isDark) {
        painter->setCompositionMode(QPainter::CompositionMode_Difference);
        painter->fillRect(rect, Qt::white);
        painter->setCompositionMode(QPainter::CompositionMode_Screen);
        painter->fillRect(rect, tokens.backgroundColor);
    } else {
        painter->setCompositionMode(QPainter::CompositionMode_Multiply);
        painter->fillRect(rect, tokens.backgroundColor);
    }
    painter->restore();
}

QString Typesetter::selectedTextForRange(const QPointF& startPos, const QPointF& endPos, const QRectF& pageBounds, const ParsedPage& page) const {
    auto doc = documentFor(page.chapterIndex);
    if (page.isImagePage || !doc) {
        if (!page.text.isEmpty()) {
            return page.text.left(120);
        }
        return QString();
    }

    qreal sideMargin = std::max(36.0, pageBounds.width() * 0.07);
    qreal topMargin = std::max(36.0, pageBounds.height() * 0.06);
    qreal btmMargin = std::max(40.0, pageBounds.height() * 0.07);
    QRectF textRect(
        pageBounds.left() + sideMargin,
        pageBounds.top() + topMargin,
        pageBounds.width() - 2.0 * sideMargin,
        pageBounds.height() - topMargin - btmMargin
    );

    qreal docYOffset = page.pageIndexInChapter * textRect.height();
    QPointF pt1(std::clamp(startPos.x() - textRect.left(), 0.0, textRect.width()),
                std::clamp(startPos.y() - textRect.top(), 0.0, textRect.height()) + docYOffset);
    QPointF pt2(std::clamp(endPos.x() - textRect.left(), 0.0, textRect.width()),
                std::clamp(endPos.y() - textRect.top(), 0.0, textRect.height()) + docYOffset);

    if (!doc->documentLayout()) {
        return QString();
    }

    int pos1 = doc->documentLayout()->hitTest(pt1, Qt::FuzzyHit);
    int pos2 = doc->documentLayout()->hitTest(pt2, Qt::FuzzyHit);

    if (pos1 < 0 || pos2 < 0 || pos1 == pos2) {
        return QString();
    }

    QTextCursor cursor(doc.get());
    cursor.setPosition(std::min(pos1, pos2));
    cursor.setPosition(std::max(pos1, pos2), QTextCursor::KeepAnchor);
    return cursor.selectedText();
}

QString Typesetter::anchorAt(const QPointF& pos, const QRectF& pageBounds, const ParsedPage& page) const {
    auto doc = documentFor(page.chapterIndex);
    if (page.isImagePage || !doc) {
        return QString();
    }

    qreal sideMargin = std::max(36.0, pageBounds.width() * 0.07);
    qreal topMargin = std::max(36.0, pageBounds.height() * 0.06);
    qreal btmMargin = std::max(40.0, pageBounds.height() * 0.07);
    QRectF textRect(
        pageBounds.left() + sideMargin,
        pageBounds.top() + topMargin,
        pageBounds.width() - 2.0 * sideMargin,
        pageBounds.height() - topMargin - btmMargin
    );

    if (!textRect.contains(pos)) {
        return QString();
    }

    qreal docX = pos.x() - textRect.left();
    qreal docY = pos.y() - textRect.top() + (page.pageIndexInChapter * textRect.height());

    if (!doc->documentLayout()) {
        return QString();
    }

    return doc->documentLayout()->anchorAt(QPointF(docX, docY));
}

std::shared_ptr<QTextDocument> Typesetter::getOrCreateDocument(
    const ChapterContent& chapter,
    const QSizeF& printableSize,
    const ReaderThemeTokens& tokens
) {
    QString tokensKey = tokens.backgroundColor.name() + tokens.textColor.name() + tokens.accentColor.name();

    auto cached = m_docCache.find(chapter.index);
    if (cached != m_docCache.end() &&
        std::abs(cached->printableSize.width() - printableSize.width()) < 1.0 &&
        std::abs(cached->printableSize.height() - printableSize.height()) < 1.0 &&
        cached->tokensKey == tokensKey) {
        return cached->doc;
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

    CachedDocument entry{doc, printableSize, tokensKey, contentHtml};
    m_docCache.insert(chapter.index, entry);
    m_docCacheOrder.removeAll(chapter.index);
    m_docCacheOrder.append(chapter.index);
    while (m_docCacheOrder.size() > 6) {
        const int evicted = m_docCacheOrder.takeFirst();
        m_docCache.remove(evicted);
    }

    return doc;
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
    const QString& highlightedSentence,
    const QVector<AnnotationRecord>& annotations
) {
    // 1. Page Background
    painter->fillRect(pageBounds, tokens.backgroundColor);

    // 2. Comic / Scanned Page Rendering
    if (page.isImagePage) {
        if (!page.imageBytes.isEmpty()) {
            QPixmap pm = preparedPagePixmap(page.chapterIndex, page.imageBytes);
            if (!pm.isNull()) {
                const QRectF targetRect = imageTargetRect(pageBounds, pm.size());

                painter->save();
                painter->setRenderHint(QPainter::Antialiasing, true);
                painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

                // Subtle soft paper elevation shadow (light paper only)
                if (!tokens.isDark) {
                    painter->fillRect(targetRect.adjusted(2, 3, 2, 3), QColor(0, 0, 0, 30));
                }
                painter->fillRect(targetRect, tokens.isDark ? tokens.backgroundColor : QColor(Qt::white));
                // Draw high-resolution page directly to target sub-pixel rect
                painter->drawPixmap(targetRect, pm, pm.rect());
                // Blend the scanned paper into the active reading theme
                applyPageTint(painter, targetRect, tokens);
                // Subtle book page outline
                painter->setPen(QPen(tokens.isDark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 40), 1.0));
                painter->drawRect(targetRect);

                // Moon+ Reader style live annotations & colored highlights on PDF / image page
                for (const auto& ann : annotations) {
                    if (ann.chapterIndex != page.chapterIndex) continue;
                    if (ann.normalizedRect.isValid() && !ann.normalizedRect.isNull()) {
                        qreal ax = targetRect.left() + ann.normalizedRect.x() * targetRect.width();
                        qreal ay = targetRect.top() + ann.normalizedRect.y() * targetRect.height();
                        qreal aw = ann.normalizedRect.width() * targetRect.width();
                        qreal ah = ann.normalizedRect.height() * targetRect.height();
                        QRectF annRect(ax, ay, aw, ah);

                        if (ann.type == QStringLiteral("underline")) {
                            painter->save();
                            painter->setPen(QPen(ann.color, 2.5, Qt::SolidLine, Qt::RoundCap));
                            painter->drawLine(QPointF(annRect.left(), annRect.bottom()), QPointF(annRect.right(), annRect.bottom()));
                            painter->restore();
                        } else if (ann.type == QStringLiteral("strikethrough")) {
                            painter->save();
                            painter->setPen(QPen(ann.color, 2.0, Qt::SolidLine, Qt::RoundCap));
                            painter->drawLine(QPointF(annRect.left(), annRect.center().y()), QPointF(annRect.right(), annRect.center().y()));
                            painter->restore();
                        } else {
                            // "highlight" or "note": Authentic Moon+ Reader pastel translucent highlighter
                            painter->save();
                            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
                            QColor hlColor = ann.color;
                            hlColor.setAlpha(140);
                            painter->fillRect(annRect, hlColor);
                            painter->restore();

                            painter->save();
                            QColor borderCol = ann.color.darker(120);
                            borderCol.setAlpha(180);
                            painter->setPen(QPen(borderCol, 1.0));
                            painter->drawRoundedRect(annRect, 2.0, 2.0);

                            if (ann.type == QStringLiteral("note")) {
                                // Draw small elegant note indicator badge in corner
                                painter->setBrush(ann.color.darker(130));
                                painter->setPen(Qt::NoPen);
                                painter->drawEllipse(QPointF(annRect.right() - 2, annRect.top() + 2), 3.5, 3.5);
                            }
                            painter->restore();
                        }
                    }
                }

                painter->restore();
                return;
            }
        }
        painter->setPen(tokens.headerFooterColor);
        painter->setFont(QFont(m_font.family(), 12));
        painter->drawText(pageBounds, Qt::AlignCenter, QObject::tr("Rendering high-resolution page %1...").arg(page.chapterIndex + 1));
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
    auto cacheIt = m_docCache.find(page.chapterIndex);
    if (cacheIt != m_docCache.end() && cacheIt->tokensKey != tokensKey && !cacheIt->contentHtml.isEmpty()) {
        cacheIt->doc->setDefaultStyleSheet(generateBookStyleSheet(tokens));
        cacheIt->doc->setHtml(cacheIt->contentHtml);
        cacheIt->tokensKey = tokensKey;
    }

    // 5. Typeset Document Body Content
    std::shared_ptr<QTextDocument> doc = (cacheIt != m_docCache.end()) ? cacheIt->doc : nullptr;
    if (doc) {
        painter->save();
        painter->setClipRect(textRect);
        painter->translate(textRect.left(), textRect.top() - (page.pageIndexInChapter * textRect.height()));
        QRectF clip(0, page.pageIndexInChapter * textRect.height(), textRect.width(), textRect.height());

        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip = clip;

        for (const auto& ann : annotations) {
            if (ann.chapterIndex != page.chapterIndex || ann.selectedText.trimmed().isEmpty()) continue;
            QTextCursor cur = doc->find(ann.selectedText);
            while (!cur.isNull()) {
                QAbstractTextDocumentLayout::Selection sel;
                sel.cursor = cur;
                if (ann.type == "underline") {
                    QTextCharFormat fmt;
                    fmt.setFontUnderline(true);
                    fmt.setUnderlineColor(ann.color);
                    sel.format = fmt;
                } else if (ann.type == "strikethrough") {
                    QTextCharFormat fmt;
                    fmt.setFontStrikeOut(true);
                    fmt.setUnderlineColor(ann.color);
                    sel.format = fmt;
                } else {
                    QColor bg = ann.color;
                    bg.setAlpha(170);
                    sel.format.setBackground(bg);
                    sel.format.setForeground(QColor(15, 23, 42));
                }
                ctx.selections.append(sel);
                cur = doc->find(ann.selectedText, cur);
            }
        }

        if (!highlightedSentence.isEmpty()) {
            QTextCursor cur = doc->find(highlightedSentence);
            while (!cur.isNull()) {
                QAbstractTextDocumentLayout::Selection sel;
                sel.cursor = cur;
                sel.format.setBackground(QColor(254, 240, 138, 200)); // Soft amber highlight
                sel.format.setForeground(QColor(15, 23, 42)); // High-contrast text
                ctx.selections.append(sel);
                cur = doc->find(highlightedSentence, cur);
            }
        }
        doc->documentLayout()->draw(painter, ctx);
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
