#include "BookCardWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>

namespace LunaRead {

BookCardWidget::BookCardWidget(const BookItem& book, QWidget* parent)
    : QWidget(parent), m_book(book) {
    setFixedSize(160, 240);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(180);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        setElevation(val.toReal());
        setShadowIntensity(val.toReal() / 6.0);
    });

    if (!m_book.coverImagePath.isEmpty() && QFile::exists(m_book.coverImagePath)) {
        m_cachedCover.load(m_book.coverImagePath);
        m_coverLoaded = !m_cachedCover.isNull();
    }
}

void BookCardWidget::updateBook(const BookItem& book) {
    m_book = book;
    m_coverLoaded = false;
    if (!m_book.coverImagePath.isEmpty() && QFile::exists(m_book.coverImagePath)) {
        m_cachedCover.load(m_book.coverImagePath);
        m_coverLoaded = !m_cachedCover.isNull();
    }
    update();
}

void BookCardWidget::setPresentationMode(ShelfPresentationMode mode) {
    m_mode = mode;
    update();
}

void BookCardWidget::setElevation(qreal val) {
    m_elevation = val;
    update();
}

void BookCardWidget::setShadowIntensity(qreal val) {
    m_shadowIntensity = val;
    update();
}

void BookCardWidget::startHoverAnimation(bool hovering) {
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_elevation);
    m_hoverAnim->setEndValue(hovering ? 6.0 : 0.0);
    m_hoverAnim->start();
}

void BookCardWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    m_isHovered = true;
    startHoverAnimation(true);
}

void BookCardWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    m_isHovered = false;
    m_starHovered = false;
    m_infoHovered = false;
    startHoverAnimation(false);
}

void BookCardWidget::mouseMoveEvent(QMouseEvent* event) {
    bool starHover = m_starRect.contains(event->position());
    bool infoHover = m_isHovered && m_infoRect.contains(event->position());
    if (starHover != m_starHovered || infoHover != m_infoHovered) {
        m_starHovered = starHover;
        m_infoHovered = infoHover;
        update();
    }
}

void BookCardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_starRect.contains(event->position())) {
            m_book.isFavorite = !m_book.isFavorite;
            emit favoriteToggled(m_book.id, m_book.isFavorite);
            update();
            return;
        }
        if (m_isHovered && m_infoRect.contains(event->position())) {
            emit detailsRequested(m_book.id);
            return;
        }
        emit clicked(m_book.id);
    }
}

void BookCardWidget::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    QAction* openAct = menu.addAction(tr("📖 Open Book"));
    QAction* detailsAct = menu.addAction(tr("ℹ️ Book Details & Metadata..."));
    QAction* exportAct = menu.addAction(tr("📤 Export Annotations..."));
    menu.addSeparator();
    QAction* doneAct = menu.addAction(tr("✓ Mark as Finished (100%)"));
    QAction* resetAct = menu.addAction(tr("↺ Reset Reading Progress"));
    menu.addSeparator();
    QAction* favAct = menu.addAction(m_book.isFavorite ? tr("⭐ Remove from Favorites") : tr("⭐ Add to Favorites"));
    QAction* folderAct = menu.addAction(tr("📁 Show in File Manager"));
    menu.addSeparator();
    QAction* deleteAct = menu.addAction(tr("🗑️ Delete Book"));

    QAction* selected = menu.exec(event->globalPos());
    if (selected == openAct) {
        emit clicked(m_book.id);
    } else if (selected == detailsAct) {
        emit detailsRequested(m_book.id);
    } else if (selected == exportAct) {
        emit exportRequested(m_book.id);
    } else if (selected == doneAct) {
        emit markFinishedRequested(m_book.id);
    } else if (selected == resetAct) {
        emit resetProgressRequested(m_book.id);
    } else if (selected == folderAct) {
        emit showInFileManagerRequested(m_book.id);
    } else if (selected == favAct) {
        m_book.isFavorite = !m_book.isFavorite;
        emit favoriteToggled(m_book.id, m_book.isFavorite);
        update();
    } else if (selected == deleteAct) {
        emit deleteRequested(m_book.id);
    }
}

QColor BookCardWidget::formatBadgeColor(const QString& format) const {
    const QString f = format.toLower();
    if (f == "epub") return QColor(16, 185, 129);      // Emerald green
    if (f == "pdf") return QColor(239, 68, 68);        // Crimson red
    if (f == "mobi" || f == "azw" || f == "azw3") return QColor(245, 158, 11); // Amber
    if (f == "fb2") return QColor(139, 92, 246);       // Purple
    if (f == "djvu") return QColor(14, 165, 233);      // Cyan
    if (f == "cbz" || f == "cbr") return QColor(236, 72, 153); // Pink
    if (f == "docx" || f == "odt") return QColor(59, 130, 246); // Blue
    if (f == "txt" || f == "md") return QColor(107, 114, 128); // Slate
    return QColor(99, 102, 241);                       // Indigo
}

void BookCardWidget::paintCover(QPainter& p, const QRectF& coverRect) {
    if (m_coverLoaded && !m_cachedCover.isNull()) {
        QPainterPath clipPath;
        clipPath.addRoundedRect(coverRect, 6.0, 6.0);
        p.save();
        p.setClipPath(clipPath);
        p.drawPixmap(coverRect.toRect(), m_cachedCover.scaled(coverRect.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        p.restore();
    } else {
        paintCoverFallback(p, coverRect);
    }
}

void BookCardWidget::paintCoverFallback(QPainter& p, const QRectF& coverRect) {
    QPainterPath path;
    path.addRoundedRect(coverRect, 6.0, 6.0);

    // Procedural gradient based on title hash
    uint hash = qHash(m_book.title);
    int hue = static_cast<int>(hash % 360);
    QColor c1 = QColor::fromHsv(hue, 140, 70);
    QColor c2 = QColor::fromHsv((hue + 35) % 360, 180, 40);

    QLinearGradient grad(coverRect.topLeft(), coverRect.bottomRight());
    grad.setColorAt(0.0, c1);
    grad.setColorAt(1.0, c2);

    p.fillPath(path, grad);

    // Subtle spine crease line
    p.setPen(QColor(0, 0, 0, 40));
    p.drawLine(QPointF(coverRect.left() + 7, coverRect.top()), QPointF(coverRect.left() + 7, coverRect.bottom()));
    p.setPen(QColor(255, 255, 255, 30));
    p.drawLine(QPointF(coverRect.left() + 8, coverRect.top()), QPointF(coverRect.left() + 8, coverRect.bottom()));

    // Title and author text
    p.setPen(Qt::white);
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    p.setFont(titleFont);

    QRectF textRect = coverRect.adjusted(14, 14, -10, -30);
    QFontMetrics fm(titleFont);
    QString elidedTitle = fm.elidedText(m_book.title, Qt::ElideRight, static_cast<int>(textRect.width() * 2));
    p.drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, elidedTitle);

    if (!m_book.author.isEmpty()) {
        QFont authorFont = font();
        authorFont.setPointSize(8);
        p.setFont(authorFont);
        p.setPen(QColor(255, 255, 255, 200));
        QRectF authorRect = coverRect.adjusted(14, coverRect.height() - 40, -10, -10);
        p.drawText(authorRect, Qt::AlignBottom | Qt::AlignLeft, fm.elidedText(m_book.author, Qt::ElideRight, static_cast<int>(authorRect.width())));
    }
}

void BookCardWidget::paintFormatBadge(QPainter& p, const QRectF& coverRect) {
    if (m_book.format.isEmpty()) return;

    QString badgeText = m_book.format.toUpper();
    QFont badgeFont = font();
    badgeFont.setBold(true);
    badgeFont.setPixelSize(9);
    p.setFont(badgeFont);

    QFontMetrics fm(badgeFont);
    int badgeWidth = fm.horizontalAdvance(badgeText) + 8;
    int badgeHeight = 16;
    QRectF badgeRect(coverRect.right() - badgeWidth - 6, coverRect.top() + 6, badgeWidth, badgeHeight);

    QPainterPath badgePath;
    badgePath.addRoundedRect(badgeRect, 4.0, 4.0);
    p.fillPath(badgePath, formatBadgeColor(m_book.format));

    p.setPen(Qt::white);
    p.drawText(badgeRect, Qt::AlignCenter, badgeText);
}

void BookCardWidget::paintFavoriteStar(QPainter& p, const QRectF& coverRect) {
    m_starRect = QRectF(coverRect.left() + 10, coverRect.top() + 6, 18, 18);

    p.save();
    p.setRenderHint(QPainter::Antialiasing);

    QColor starColor = m_book.isFavorite ? QColor(251, 191, 36) : (m_starHovered ? QColor(255, 255, 255, 220) : QColor(255, 255, 255, 120));
    p.setPen(m_book.isFavorite ? QColor(217, 119, 6) : Qt::transparent);
    p.setBrush(starColor);

    // Draw 5-pointed star
    QPolygonF starPoly;
    const QPointF center = m_starRect.center();
    const qreal rOuter = 8.0;
    const qreal rInner = 3.5;
    for (int i = 0; i < 10; ++i) {
        qreal angle = -M_PI / 2.0 + (i * M_PI / 5.0);
        qreal r = (i % 2 == 0) ? rOuter : rInner;
        starPoly << QPointF(center.x() + r * std::cos(angle), center.y() + r * std::sin(angle));
    }
    p.drawPolygon(starPoly);
    p.restore();
}

void BookCardWidget::paintInfoButton(QPainter& p, const QRectF& coverRect) {
    if (!m_isHovered) return;
    m_infoRect = QRectF(coverRect.left() + 32, coverRect.top() + 6, 18, 18);

    p.save();
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = m_infoHovered ? QColor(0, 0, 0, 180) : QColor(0, 0, 0, 110);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(m_infoRect);

    // Draw 'i' symbol inside circle
    p.setPen(Qt::white);
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(10);
    p.setFont(f);
    p.drawText(m_infoRect, Qt::AlignCenter, QStringLiteral("i"));
    p.restore();
}

void BookCardWidget::paintProgressBar(QPainter& p, const QRectF& coverRect) {
    if (m_book.progressPercentage <= 0.0) return;

    QRectF barRect(coverRect.left() + 4, coverRect.bottom() - 6, coverRect.width() - 8, 3);
    QPainterPath bgPath;
    bgPath.addRoundedRect(barRect, 1.5, 1.5);
    p.fillPath(bgPath, QColor(0, 0, 0, 90));

    qreal filledWidth = barRect.width() * std::clamp(m_book.progressPercentage / 100.0, 0.0, 1.0);
    QRectF fillRect(barRect.left(), barRect.top(), filledWidth, barRect.height());
    QPainterPath fillPath;
    fillPath.addRoundedRect(fillRect, 1.5, 1.5);
    p.fillPath(fillPath, QColor(59, 130, 246));
}

void BookCardWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Apply elevation lift
    const qreal yOffset = -m_elevation;
    QRectF cardRect = rect().adjusted(6, 6, -6, -6);
    cardRect.translate(0, yOffset);

    // Draw drop shadow based on elevation
    if (m_elevation > 0.01) {
        p.save();
        QColor shadowColor(0, 0, 0, static_cast<int>(35 + 40 * m_shadowIntensity));
        for (int i = 1; i <= 4; ++i) {
            QRectF sRect = cardRect.adjusted(-i * 0.8, -i * 0.4 + i * 1.5, i * 0.8, i * 1.5);
            QPainterPath sPath;
            sPath.addRoundedRect(sRect, 7.0, 7.0);
            p.fillPath(sPath, QColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() / (i * 2)));
        }
        p.restore();
    }

    // Cover rectangle occupies top portion
    QRectF coverRect(cardRect.left(), cardRect.top(), cardRect.width(), cardRect.height() - 48);
    paintCover(p, coverRect);
    paintFormatBadge(p, coverRect);
    paintFavoriteStar(p, coverRect);
    paintProgressBar(p, coverRect);

    // Metadata title & progress label below cover
    QRectF metaRect(cardRect.left(), coverRect.bottom() + 6, cardRect.width(), 40);
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(9);
    p.setFont(titleFont);
    p.setPen(m_isHovered ? QColor(37, 99, 235) : QColor(30, 41, 59));

    QFontMetrics fm(titleFont);
    QString elidedTitle = fm.elidedText(m_book.title, Qt::ElideRight, static_cast<int>(metaRect.width()));
    p.drawText(metaRect.left(), metaRect.top() + fm.ascent(), elidedTitle);

    // Progress text
    QFont progFont = font();
    progFont.setPointSize(8);
    p.setFont(progFont);
    p.setPen(QColor(100, 116, 139));
    QString progText = QString("%1%").arg(static_cast<int>(m_book.progressPercentage));
    p.drawText(metaRect.left(), metaRect.top() + fm.height() + 12, progText);
}

} // namespace LunaRead
