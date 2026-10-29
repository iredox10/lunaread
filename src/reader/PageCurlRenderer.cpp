#include "PageCurlRenderer.h"
#include "Typesetter.h"
#include "../features/ImageCropService.h"
#include <QPainterPath>
#include <QLinearGradient>
#include <QTextLayout>
#include <QTextCharFormat>
#include <cmath>

namespace LunaRead {

void PageCurlRenderer::paint(
    QPainter* painter,
    const QRectF& bounds,
    const ParsedPage& currentPage,
    const std::optional<ParsedPage>& nextPage,
    qreal progress,
    const QPointF& origin,
    const QPointF& dragPos,
    const ReaderThemeTokens& tokens,
    bool isDualPage,
    const QString& highlightedSentence,
    Typesetter* typesetter,
    const QString& bookTitle,
    const QString& chapterTitle,
    const QVector<AnnotationRecord>& annotations
) {
    if (progress <= 0.001) {
        // Flat page: render directly
        renderPageSurface(painter, bounds, currentPage, tokens, typesetter, bookTitle, chapterTitle, false, highlightedSentence, annotations);
        return;
    }

    if (progress >= 0.999) {
        // Full turn: render next page
        if (nextPage.has_value()) {
            renderPageSurface(painter, bounds, *nextPage, tokens, typesetter, bookTitle, chapterTitle, false, highlightedSentence, annotations);
        }
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Step A: Underneath next page
    if (nextPage.has_value()) {
        renderPageSurface(painter, bounds, *nextPage, tokens, typesetter, bookTitle, chapterTitle, true, QString(), annotations);
    }

    // Step B: Calculate fold bisector line
    QPointF start = origin;
    QPointF current = dragPos;
    if (current.isNull()) {
        current = QPointF(bounds.width() * (1.0 - progress), bounds.height());
    }

    qreal midX = (start.x() + current.x()) / 2.0;
    qreal midY = (start.y() + current.y()) / 2.0;
    qreal dx = current.x() - start.x();
    qreal dy = current.y() - start.y();
    qreal angle = std::atan2(dy, dx);
    qreal normalAngle = angle + M_PI / 2.0;
    Q_UNUSED(midY);
    Q_UNUSED(normalAngle);

    // Build uncurled polygon clip
    QPainterPath uncurledPath;
    uncurledPath.moveTo(0, 0);
    uncurledPath.lineTo(midX, 0);
    uncurledPath.lineTo(midX, bounds.height());
    uncurledPath.lineTo(0, bounds.height());
    uncurledPath.closeSubpath();

    painter->save();
    painter->setClipPath(uncurledPath);
    renderPageSurface(painter, bounds, currentPage, tokens, typesetter, bookTitle, chapterTitle, false, highlightedSentence, annotations);
    painter->restore();

    // Step C: 3D Cylinder curl flap & reflection
    qreal flapWidth = std::abs(bounds.width() - midX);
    qreal cylinderRadius = 20.0 + (60.0 - 20.0) * (1.0 - progress);

    QPainterPath curlPath;
    curlPath.moveTo(midX, 0);
    curlPath.cubicTo(
        midX + cylinderRadius, bounds.height() * 0.25,
        current.x() + cylinderRadius, bounds.height() * 0.75,
        current.x(), bounds.height()
    );
    curlPath.lineTo(midX, bounds.height());
    curlPath.closeSubpath();

    // Ambient drop shadow onto next page
    QLinearGradient dropShadow(midX, 0, midX + flapWidth * 0.4, 0);
    dropShadow.setColorAt(0.0, QColor(0, 0, 0, 160));
    dropShadow.setColorAt(0.5, QColor(0, 0, 0, 60));
    dropShadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter->fillPath(curlPath, dropShadow);

    // Back-face of page (mirrored with translucency)
    QColor backColor = tokens.backgroundColor.darker(108);
    painter->fillPath(curlPath, backColor);

    // Specular highlight ridge
    QLinearGradient ridgeGrad(midX, 0, midX + 15.0, 0);
    ridgeGrad.setColorAt(0.0, QColor(255, 255, 255, 80));
    ridgeGrad.setColorAt(0.7, QColor(255, 255, 255, 20));
    ridgeGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->fillPath(curlPath, ridgeGrad);

    // Step D: Book spine / gutter shadow
    qreal spineX = isDualPage ? bounds.width() / 2.0 : 0.0;
    QLinearGradient spineGrad(spineX, 0, spineX + 24.0, 0);
    spineGrad.setColorAt(0.0, QColor(0, 0, 0, tokens.isDark ? 80 : 45));
    spineGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter->fillRect(QRectF(spineX, 0, 24.0, bounds.height()), spineGrad);

    painter->restore();
}

void PageCurlRenderer::renderPageSurface(
    QPainter* painter,
    const QRectF& rect,
    const ParsedPage& page,
    const ReaderThemeTokens& tokens,
    Typesetter* typesetter,
    const QString& bookTitle,
    const QString& chapterTitle,
    bool isVerso,
    const QString& highlightedSentence,
    const QVector<AnnotationRecord>& annotations
) {
    if (typesetter) {
        typesetter->renderPage(painter, rect, page, tokens, bookTitle, chapterTitle, isVerso, highlightedSentence, annotations);
        return;
    }

    // Fallback if no typesetter
    painter->fillRect(rect, tokens.backgroundColor);

    if (page.isImagePage && !page.imageBytes.isEmpty()) {
        QPixmap pixmap;
        pixmap.loadFromData(page.imageBytes);
        if (!pixmap.isNull()) {
            pixmap = ImageCropService::instance().cropMargins(pixmap);
            QRectF contentRect = rect.adjusted(8, 8, -8, -8);
            qreal scale = std::min(contentRect.width() / pixmap.width(), contentRect.height() / pixmap.height());
            qreal drawW = pixmap.width() * scale;
            qreal drawH = pixmap.height() * scale;
            qreal drawX = contentRect.left() + (contentRect.width() - drawW) / 2.0;
            qreal drawY = contentRect.top() + (contentRect.height() - drawH) / 2.0;
            QRectF targetRect(drawX, drawY, drawW, drawH);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter->fillRect(targetRect, Qt::white);
            painter->drawPixmap(targetRect, pixmap, pixmap.rect());
            painter->restore();
        }
        return;
    }

    // Render body text
    painter->save();
    painter->setPen(tokens.textColor);
    QFont font("Noto Serif", 14);
    painter->setFont(font);

    QRectF textRect = rect.adjusted(36, 36, -36, -36);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, page.text);

    // Running footer
    painter->setPen(tokens.headerFooterColor);
    QFont footerFont("Noto Serif", 10);
    painter->setFont(footerFont);
    QRectF footerRect(rect.left() + 36, rect.bottom() - 28, rect.width() - 72, 20);
    QString footerStr = QString("— %1 —").arg(page.pageIndexInChapter + 1);
    painter->drawText(footerRect, Qt::AlignCenter, footerStr);

    painter->restore();
}

} // namespace LunaRead
