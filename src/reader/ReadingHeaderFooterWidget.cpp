#include "ReadingHeaderFooterWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QTime>
#include <QDir>
#include <QFile>
#include <QFontMetrics>
#include <QPolygonF>
#include <cmath>
#include <algorithm>

namespace LunaRead {

// ============================================================================
// Battery Detection Helper
// ============================================================================

BatteryStatusInfo BatteryStatusInfo::querySystem() {
    BatteryStatusInfo result;
    QDir powerSupplyDir(QStringLiteral("/sys/class/power_supply"));

    if (!powerSupplyDir.exists()) {
        result.percentage = 100;
        result.isAcConnected = true;
        result.hasBattery = false;
        result.isCharging = false;
        return result;
    }

    const QFileInfoList entries = powerSupplyDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    bool batteryFound = false;

    // Check battery entries
    for (const QFileInfo& info : entries) {
        const QString path = info.absoluteFilePath();
        QString type;
        QFile typeFile(path + QStringLiteral("/type"));
        if (typeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            type = QString::fromUtf8(typeFile.readAll()).trimmed();
            typeFile.close();
        }

        const QString baseName = info.fileName();
        if (baseName.startsWith(QStringLiteral("BAT"), Qt::CaseInsensitive) ||
            type.compare(QStringLiteral("Battery"), Qt::CaseInsensitive) == 0) {
            
            // Read capacity
            QFile capFile(path + QStringLiteral("/capacity"));
            if (capFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                bool ok = false;
                int cap = QString::fromUtf8(capFile.readAll()).trimmed().toInt(&ok);
                capFile.close();
                if (ok) {
                    result.percentage = std::clamp(cap, 0, 100);
                    result.hasBattery = true;
                    batteryFound = true;
                }
            }

            // Read status
            QFile statusFile(path + QStringLiteral("/status"));
            if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString status = QString::fromUtf8(statusFile.readAll()).trimmed();
                statusFile.close();
                if (status.compare(QStringLiteral("Charging"), Qt::CaseInsensitive) == 0 ||
                    status.compare(QStringLiteral("Full"), Qt::CaseInsensitive) == 0) {
                    result.isCharging = true;
                }
            }

            if (batteryFound) {
                break;
            }
        }
    }

    // Check AC / Mains adapter entries
    for (const QFileInfo& info : entries) {
        const QString path = info.absoluteFilePath();
        QString type;
        QFile typeFile(path + QStringLiteral("/type"));
        if (typeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            type = QString::fromUtf8(typeFile.readAll()).trimmed();
            typeFile.close();
        }

        const QString baseName = info.fileName();
        if (baseName.startsWith(QStringLiteral("AC"), Qt::CaseInsensitive) ||
            baseName.startsWith(QStringLiteral("ADP"), Qt::CaseInsensitive) ||
            type.compare(QStringLiteral("Mains"), Qt::CaseInsensitive) == 0) {
            
            QFile onlineFile(path + QStringLiteral("/online"));
            if (onlineFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                int online = QString::fromUtf8(onlineFile.readAll()).trimmed().toInt();
                onlineFile.close();
                if (online == 1) {
                    result.isAcConnected = true;
                }
            }
        }
    }

    if (!batteryFound) {
        result.percentage = 100;
        result.isAcConnected = true;
        result.hasBattery = false;
        result.isCharging = false;
    }

    return result;
}

// ============================================================================
// ReadingHeaderWidget Implementation
// ============================================================================

ReadingHeaderWidget::ReadingHeaderWidget(QWidget* parent)
    : QWidget(parent)
    , m_tokens(ReaderThemeTokens::parchment()) {
    setFixedHeight(28);
    m_timeLeftText = formatTimeLeft(m_wpm, m_wordsLeft);
}

void ReadingHeaderWidget::setBookTitle(const QString& title) {
    if (m_bookTitle != title) {
        m_bookTitle = title;
        update();
    }
}

void ReadingHeaderWidget::setChapterTitle(const QString& title) {
    if (m_chapterTitle != title) {
        m_chapterTitle = title;
        update();
    }
}

void ReadingHeaderWidget::setTimeLeft(int wpm, int wordsLeftInChapter) {
    m_wpm = wpm;
    m_wordsLeft = wordsLeftInChapter;
    m_timeLeftText = formatTimeLeft(wpm, wordsLeftInChapter);
    update();
}

void ReadingHeaderWidget::setTimeLeftText(const QString& text) {
    if (m_timeLeftText != text) {
        m_timeLeftText = text;
        update();
    }
}

void ReadingHeaderWidget::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    update();
}

void ReadingHeaderWidget::setDrawBackground(bool draw) {
    if (m_drawBackground != draw) {
        m_drawBackground = draw;
        update();
    }
}

QSize ReadingHeaderWidget::sizeHint() const {
    return QSize(400, 28);
}

void ReadingHeaderWidget::mousePressEvent(QMouseEvent* event) {
    emit clicked();
    QWidget::mousePressEvent(event);
}

QString ReadingHeaderWidget::formatTimeLeft(int wpm, int wordsLeft) const {
    if (wordsLeft <= 0) {
        return QStringLiteral("0 mins left");
    }

    const int effectiveWpm = (wpm > 0) ? wpm : 200;
    const int mins = static_cast<int>(std::round(static_cast<double>(wordsLeft) / static_cast<double>(effectiveWpm)));

    if (mins <= 0) {
        return QStringLiteral("< 1 min left");
    }
    if (mins == 1) {
        return QStringLiteral("1 min left");
    }
    if (mins < 60) {
        return QStringLiteral("%1 mins left").arg(mins);
    }

    const int hours = mins / 60;
    const int remMins = mins % 60;
    if (remMins == 0) {
        return QStringLiteral("%1 hr%2 left").arg(hours).arg(hours > 1 ? QStringLiteral("s") : QStringLiteral(""));
    }
    return QStringLiteral("%1h %2m left").arg(hours).arg(remMins);
}

void ReadingHeaderWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_drawBackground) {
        painter.fillRect(rect(), m_tokens.backgroundColor);
    }

    QFont statusFont = font();
    statusFont.setPointSize(9);
    painter.setFont(statusFont);
    painter.setPen(m_tokens.headerFooterColor);

    const QFontMetrics fm(statusFont);
    const int w = width();
    const int h = height();
    const int margin = 16;
    const int spacing = 12;

    // 1. Right: Time left in current chapter
    const int rightTextWidth = fm.horizontalAdvance(m_timeLeftText);
    const QRectF rightRect(w - margin - rightTextWidth, 0, rightTextWidth, h);
    painter.drawText(rightRect, Qt::AlignRight | Qt::AlignVCenter, m_timeLeftText);

    // 2. Center: Chapter title
    // Determine bounds for chapter title to remain centered without overlapping
    const int maxCenterWidth = std::max(0, w - 2 * (margin + rightTextWidth + spacing));
    const int fullCenterWidth = fm.horizontalAdvance(m_chapterTitle);
    QString drawnChapter = m_chapterTitle;

    QRectF centerRect;
    if (fullCenterWidth <= maxCenterWidth && maxCenterWidth > 0) {
        const qreal cx = (w - fullCenterWidth) / 2.0;
        centerRect = QRectF(cx, 0, fullCenterWidth, h);
    } else {
        const int availableCenterWidth = std::max(40, w - (2 * margin + rightTextWidth + 2 * spacing + 100));
        drawnChapter = fm.elidedText(m_chapterTitle, Qt::ElideRight, availableCenterWidth);
        const int elidedCenterW = fm.horizontalAdvance(drawnChapter);
        const qreal cx = std::clamp((w - elidedCenterW) / 2.0, static_cast<qreal>(margin + 100 + spacing), static_cast<qreal>(w - margin - rightTextWidth - spacing - elidedCenterW));
        centerRect = QRectF(cx, 0, elidedCenterW, h);
    }
    painter.drawText(centerRect, Qt::AlignCenter | Qt::AlignVCenter, drawnChapter);

    // 3. Left: Book title
    const qreal leftAvailableWidth = std::max(0.0, centerRect.left() - margin - spacing);
    const QString drawnBook = fm.elidedText(m_bookTitle, Qt::ElideRight, static_cast<int>(leftAvailableWidth));
    const QRectF leftRect(margin, 0, leftAvailableWidth, h);
    painter.drawText(leftRect, Qt::AlignLeft | Qt::AlignVCenter, drawnBook);
}

// ============================================================================
// ReadingFooterWidget Implementation
// ============================================================================

ReadingFooterWidget::ReadingFooterWidget(QWidget* parent)
    : QWidget(parent)
    , m_tokens(ReaderThemeTokens::parchment()) {
    setFixedHeight(28);

    updateClock();
    updateBattery();

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        updateClock();
        static int batteryCounter = 0;
        if (++batteryCounter >= 15) { // update battery every 15s
            batteryCounter = 0;
            if (!m_batteryOverridden) {
                updateBattery();
            }
        }
    });
    m_timer->start();
}

void ReadingFooterWidget::setPageProgress(int curPage, int totalPages) {
    m_curPage = curPage;
    m_totalPages = totalPages;
    if (totalPages > 0) {
        m_pageProgressText = QStringLiteral("Page %1 of %2").arg(curPage).arg(totalPages);
    } else {
        m_pageProgressText = QStringLiteral("Page %1").arg(curPage);
    }
    update();
}

void ReadingFooterWidget::setChapters(int curChapter, int totalChapters) {
    m_curChapter = curChapter;
    m_totalChapters = totalChapters;
    update();
}

void ReadingFooterWidget::setTotalProgress(double pct) {
    m_pct = std::clamp(pct, 0.0, 100.0);
    m_totalProgressText = QStringLiteral("%1%").arg(QString::number(m_pct, 'f', 1));
    update();
}

void ReadingFooterWidget::updateBattery() {
    if (!m_batteryOverridden) {
        m_battery = BatteryStatusInfo::querySystem();
        update();
    }
}

void ReadingFooterWidget::updateClock() {
    if (m_clockOverride.has_value()) {
        m_clockText = m_clockOverride.value();
    } else {
        m_clockText = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    }
    update();
}

void ReadingFooterWidget::setClockOverride(const QString& timeStr) {
    m_clockOverride = timeStr;
    m_clockText = timeStr;
    update();
}

void ReadingFooterWidget::clearClockOverride() {
    m_clockOverride.reset();
    updateClock();
}

void ReadingFooterWidget::setBatteryOverride(int percentage, bool isCharging, bool isAc, bool hasBattery) {
    m_batteryOverridden = true;
    m_battery.percentage = std::clamp(percentage, 0, 100);
    m_battery.isCharging = isCharging;
    m_battery.isAcConnected = isAc;
    m_battery.hasBattery = hasBattery;
    update();
}

void ReadingFooterWidget::clearBatteryOverride() {
    m_batteryOverridden = false;
    updateBattery();
}

void ReadingFooterWidget::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    update();
}

void ReadingFooterWidget::setDrawBackground(bool draw) {
    if (m_drawBackground != draw) {
        m_drawBackground = draw;
        update();
    }
}

QSize ReadingFooterWidget::sizeHint() const {
    return QSize(400, 28);
}

void ReadingFooterWidget::mousePressEvent(QMouseEvent* event) {
    // If clicked on bottom progress bar area (bottom 8 pixels), request seek
    if (event->position().y() >= height() - 8 && width() > 0) {
        const double pct = (event->position().x() / static_cast<double>(width())) * 100.0;
        emit progressSeekRequested(std::clamp(pct, 0.0, 100.0));
    }
    emit clicked();
    QWidget::mousePressEvent(event);
}

void ReadingFooterWidget::drawBatteryIcon(QPainter* painter, const QRectF& iconRect) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal bodyWidth = iconRect.width() - 3.0;
    const qreal bodyHeight = iconRect.height();
    const QRectF bodyRect(iconRect.left(), iconRect.top(), bodyWidth, bodyHeight);

    // Terminal nipple on the right edge
    const qreal nippleWidth = 2.0;
    const qreal nippleHeight = bodyHeight * 0.45;
    const QRectF nippleRect(iconRect.left() + bodyWidth, iconRect.top() + (bodyHeight - nippleHeight) / 2.0, nippleWidth, nippleHeight);

    // Battery outline
    QPen pen(m_tokens.headerFooterColor, 1.2);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(bodyRect, 2.0, 2.0);

    // Terminal nipple
    painter->setBrush(m_tokens.headerFooterColor);
    painter->drawRoundedRect(nippleRect, 0.8, 0.8);

    // Inner charge fill
    const qreal fillInset = 2.0;
    const qreal maxFillW = bodyWidth - fillInset * 2.0;
    const qreal fillH = bodyHeight - fillInset * 2.0;
    const qreal fillW = maxFillW * std::clamp(static_cast<qreal>(m_battery.percentage) / 100.0, 0.0, 1.0);

    const QRectF fillRect(bodyRect.left() + fillInset, bodyRect.top() + fillInset, fillW, fillH);
    QColor fillColor = m_tokens.headerFooterColor;
    if (m_battery.isCharging) {
        fillColor = m_tokens.accentColor;
    } else if (m_battery.percentage <= 15) {
        fillColor = QColor(220, 53, 69); // Low battery warning red
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(fillColor);
    painter->drawRoundedRect(fillRect, 1.0, 1.0);

    // Lightning bolt for charging / AC
    if (m_battery.isCharging || (m_battery.isAcConnected && !m_battery.hasBattery)) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_tokens.accentColor);
        const qreal cx = bodyRect.center().x();
        const qreal cy = bodyRect.center().y();
        QPolygonF bolt;
        bolt << QPointF(cx + 0.8, cy - 3.8)
             << QPointF(cx - 2.0, cy + 0.2)
             << QPointF(cx - 0.2, cy + 0.2)
             << QPointF(cx - 0.8, cy + 3.8)
             << QPointF(cx + 2.0, cy - 0.2)
             << QPointF(cx + 0.2, cy - 0.2);
        painter->drawPolygon(bolt);
    }

    painter->restore();
}

void ReadingFooterWidget::drawProgressBar(QPainter* painter, const QRectF& barRect) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    // Track background
    painter->fillRect(barRect, m_tokens.dividerColor);

    // Filled progress
    const qreal filledWidth = barRect.width() * std::clamp(m_pct / 100.0, 0.0, 1.0);
    const QRectF filledRect(barRect.left(), barRect.top(), filledWidth, barRect.height());
    painter->fillRect(filledRect, m_tokens.accentColor);

    // Chapter tick marks if multi-chapter
    if (m_totalChapters > 1 && m_totalChapters <= 150) {
        QColor tickColor = m_tokens.backgroundColor;
        tickColor.setAlpha(180);
        painter->setPen(tickColor);
        for (int i = 1; i < m_totalChapters; ++i) {
            const int x = static_cast<int>(barRect.left() + (static_cast<double>(i) / m_totalChapters) * barRect.width());
            painter->drawLine(x, static_cast<int>(barRect.top()), x, static_cast<int>(barRect.bottom()));
        }
    }

    painter->restore();
}

void ReadingFooterWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_drawBackground) {
        painter.fillRect(rect(), m_tokens.backgroundColor);
    }

    const int w = width();
    const int h = height();
    const qreal barHeight = 2.5;
    const QRectF barRect(0, h - barHeight, w, barHeight);

    // Draw thin sleek progress bar along bottom edge
    drawProgressBar(&painter, barRect);

    // Text & status area
    QFont statusFont = font();
    statusFont.setPointSize(9);
    painter.setFont(statusFont);
    painter.setPen(m_tokens.headerFooterColor);

    const QFontMetrics fm(statusFont);
    const int margin = 16;
    const qreal contentH = h - barHeight;

    // 1. Left: Clock and Battery
    const int clockW = fm.horizontalAdvance(m_clockText);
    const QRectF clockRect(margin, 0, clockW, contentH);
    painter.drawText(clockRect, Qt::AlignLeft | Qt::AlignVCenter, m_clockText);

    // Battery icon + text
    const qreal batIconX = margin + clockW + 10.0;
    const qreal batIconW = 18.0;
    const qreal batIconH = 10.0;
    const QRectF batIconRect(batIconX, (contentH - batIconH) / 2.0, batIconW, batIconH);
    drawBatteryIcon(&painter, batIconRect);

    QString batLabel;
    if (!m_battery.hasBattery && m_battery.isAcConnected) {
        batLabel = QStringLiteral("100% AC");
    } else {
        batLabel = QStringLiteral("%1%").arg(m_battery.percentage);
    }
    const qreal batLabelX = batIconX + batIconW + 4.0;
    const int batLabelW = fm.horizontalAdvance(batLabel);
    const QRectF batLabelRect(batLabelX, 0, batLabelW, contentH);
    painter.drawText(batLabelRect, Qt::AlignLeft | Qt::AlignVCenter, batLabel);

    // 2. Right: Total progress ("34.5%")
    const int rightTextW = fm.horizontalAdvance(m_totalProgressText);
    const QRectF rightRect(w - margin - rightTextW, 0, rightTextW, contentH);
    painter.drawText(rightRect, Qt::AlignRight | Qt::AlignVCenter, m_totalProgressText);

    // 3. Center: Page progress ("Page 42 of 310")
    const int pageTextW = fm.horizontalAdvance(m_pageProgressText);
    const qreal cx = (w - pageTextW) / 2.0;
    const QRectF pageRect(cx, 0, pageTextW, contentH);
    painter.drawText(pageRect, Qt::AlignCenter | Qt::AlignVCenter, m_pageProgressText);
}

// ============================================================================
// ReadingHeaderFooterWidget Implementation
// ============================================================================

ReadingHeaderFooterWidget::ReadingHeaderFooterWidget(QWidget* parent)
    : QWidget(parent)
    , m_tokens(ReaderThemeTokens::parchment()) {
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_header = new ReadingHeaderWidget(this);
    m_footer = new ReadingFooterWidget(this);

    connect(m_header, &ReadingHeaderWidget::clicked, this, &ReadingHeaderFooterWidget::headerClicked);
    connect(m_footer, &ReadingFooterWidget::clicked, this, &ReadingHeaderFooterWidget::footerClicked);
    connect(m_footer, &ReadingFooterWidget::progressSeekRequested, this, &ReadingHeaderFooterWidget::progressSeekRequested);

    applyTheme(m_tokens);
    updateLayout();
}

void ReadingHeaderFooterWidget::setBookTitle(const QString& title) {
    if (m_header) {
        m_header->setBookTitle(title);
    }
}

QString ReadingHeaderFooterWidget::bookTitle() const {
    return m_header ? m_header->bookTitle() : QString();
}

void ReadingHeaderFooterWidget::setChapterTitle(const QString& title) {
    if (m_header) {
        m_header->setChapterTitle(title);
    }
}

QString ReadingHeaderFooterWidget::chapterTitle() const {
    return m_header ? m_header->chapterTitle() : QString();
}

void ReadingHeaderFooterWidget::updateStats(int curPage, int totalPages, int curChapter, int totalChapters, double pct, int wpm, int wordsLeftInChapter) {
    m_curPage = curPage;
    m_totalPages = totalPages;
    m_curChapter = curChapter;
    m_totalChapters = totalChapters;
    m_pct = pct;
    m_wpm = wpm;
    m_wordsLeft = wordsLeftInChapter;

    if (m_header) {
        m_header->setTimeLeft(wpm, wordsLeftInChapter);
    }
    if (m_footer) {
        m_footer->setPageProgress(curPage, totalPages);
        m_footer->setChapters(curChapter, totalChapters);
        m_footer->setTotalProgress(pct);
    }
}

void ReadingHeaderFooterWidget::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    if (m_header) {
        m_header->applyTheme(tokens);
    }
    if (m_footer) {
        m_footer->applyTheme(tokens);
    }
}

void ReadingHeaderFooterWidget::setThemeMode(ReaderThemeMode mode) {
    applyTheme(ReaderThemeTokens::fromMode(mode));
}

void ReadingHeaderFooterWidget::setHeaderVisible(bool visible) {
    if (m_header) {
        m_header->setVisible(visible);
    }
}

bool ReadingHeaderFooterWidget::isHeaderVisible() const {
    return m_header && !m_header->isHidden();
}

void ReadingHeaderFooterWidget::setFooterVisible(bool visible) {
    if (m_footer) {
        m_footer->setVisible(visible);
    }
}

bool ReadingHeaderFooterWidget::isFooterVisible() const {
    return m_footer && !m_footer->isHidden();
}

void ReadingHeaderFooterWidget::setDrawBackground(bool draw) {
    if (m_header) m_header->setDrawBackground(draw);
    if (m_footer) m_footer->setDrawBackground(draw);
}

bool ReadingHeaderFooterWidget::drawBackground() const {
    return m_header ? m_header->drawBackground() : false;
}

void ReadingHeaderFooterWidget::updateLayout() {
    if (m_header) {
        const int hHeight = m_header->sizeHint().height();
        m_header->setGeometry(0, 0, width(), hHeight);
    }
    if (m_footer) {
        const int fHeight = m_footer->sizeHint().height();
        m_footer->setGeometry(0, height() - fHeight, width(), fHeight);
    }
}

void ReadingHeaderFooterWidget::setGeometry(const QRect& rect) {
    QWidget::setGeometry(rect);
    updateLayout();
}

ReadingHeaderWidget* ReadingHeaderFooterWidget::headerWidget() {
    updateLayout();
    return m_header;
}

ReadingFooterWidget* ReadingHeaderFooterWidget::footerWidget() {
    updateLayout();
    return m_footer;
}

void ReadingHeaderFooterWidget::reposition(const QRect& bounds) {
    setGeometry(bounds);
    updateLayout();
}

void ReadingHeaderFooterWidget::reposition(const QSize& size) {
    setGeometry(QRect(0, 0, size.width(), size.height()));
    updateLayout();
}

QString ReadingHeaderFooterWidget::timeLeftText() const {
    return m_header ? m_header->timeLeftText() : QString();
}

QString ReadingHeaderFooterWidget::pageProgressText() const {
    return m_footer ? m_footer->pageProgressText() : QString();
}

QString ReadingHeaderFooterWidget::totalProgressText() const {
    return m_footer ? m_footer->totalProgressText() : QString();
}

QString ReadingHeaderFooterWidget::clockText() const {
    return m_footer ? m_footer->clockText() : QString();
}

BatteryStatusInfo ReadingHeaderFooterWidget::batteryInfo() const {
    return m_footer ? m_footer->batteryInfo() : BatteryStatusInfo{};
}

void ReadingHeaderFooterWidget::setClockOverride(const QString& timeStr) {
    if (m_footer) {
        m_footer->setClockOverride(timeStr);
    }
}

void ReadingHeaderFooterWidget::clearClockOverride() {
    if (m_footer) {
        m_footer->clearClockOverride();
    }
}

void ReadingHeaderFooterWidget::setBatteryOverride(int percentage, bool isCharging, bool isAc, bool hasBattery) {
    if (m_footer) {
        m_footer->setBatteryOverride(percentage, isCharging, isAc, hasBattery);
    }
}

void ReadingHeaderFooterWidget::clearBatteryOverride() {
    if (m_footer) {
        m_footer->clearBatteryOverride();
    }
}

void ReadingHeaderFooterWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateLayout();
}

void ReadingHeaderFooterWidget::mousePressEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    const bool inHeader = m_header && m_header->isVisible() && m_header->geometry().contains(pos.toPoint());
    const bool inFooter = m_footer && m_footer->isVisible() && m_footer->geometry().contains(pos.toPoint());

    if (!inHeader && !inFooter) {
        // Let gestures / viewport process clicks in middle reading area
        event->ignore();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ReadingHeaderFooterWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    const bool inHeader = m_header && m_header->isVisible() && m_header->geometry().contains(pos.toPoint());
    const bool inFooter = m_footer && m_footer->isVisible() && m_footer->geometry().contains(pos.toPoint());

    if (!inHeader && !inFooter) {
        event->ignore();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void ReadingHeaderFooterWidget::mouseReleaseEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    const bool inHeader = m_header && m_header->isVisible() && m_header->geometry().contains(pos.toPoint());
    const bool inFooter = m_footer && m_footer->isVisible() && m_footer->geometry().contains(pos.toPoint());

    if (!inHeader && !inFooter) {
        event->ignore();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ReadingHeaderFooterWidget::wheelEvent(QWheelEvent* event) {
    // Wheel events scroll or turn pages underneath
    event->ignore();
}

} // namespace LunaRead
