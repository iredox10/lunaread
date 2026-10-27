#include "QuickNavPopoverWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyleOptionSlider>
#include <QGraphicsDropShadowEffect>
#include <algorithm>
#include <cmath>

namespace LunaRead {

// ============================================================================
// QuickNavSeekSlider
// ============================================================================

QuickNavSeekSlider::QuickNavSeekSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent) {
    setRange(1, 1);
    setValue(1);
    setTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(36);
    setMouseTracking(true);
}

void QuickNavSeekSlider::setTotalPages(int totalPages) {
    m_totalPages = std::max(1, totalPages);
    setRange(1, m_totalPages);
    updateBubbleTextFromValue(value());
    update();
}

void QuickNavSeekSlider::setCurrentPage(int pageNumber) {
    pageNumber = std::clamp(pageNumber, 1, m_totalPages);
    setValue(pageNumber);
    updateBubbleTextFromValue(pageNumber);
    update();
}

void QuickNavSeekSlider::setBubbleText(const QString& text) {
    m_bubbleText = text;
    update();
}

void QuickNavSeekSlider::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    update();
}

void QuickNavSeekSlider::updateBubbleTextFromValue(int val) {
    double pct = (static_cast<double>(val) / static_cast<double>(m_totalPages)) * 100.0;
    m_bubbleText = QStringLiteral("Page %1 of %2 (%3%)")
                       .arg(val)
                       .arg(m_totalPages)
                       .arg(static_cast<int>(std::round(pct)));
}

void QuickNavSeekSlider::updateBubbleFromPosition(int posX) {
    const int w = width() - 24;
    if (w <= 0) return;
    int clampedX = std::clamp(posX - 12, 0, w);
    double ratio = static_cast<double>(clampedX) / static_cast<double>(w);
    int page = 1 + static_cast<int>(std::round(ratio * (m_totalPages - 1)));
    page = std::clamp(page, 1, m_totalPages);
    setValue(page);
    updateBubbleTextFromValue(page);
    emit seekPageChanged(page - 1);
}

void QuickNavSeekSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        updateBubbleFromPosition(event->pos().x());
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void QuickNavSeekSlider::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        updateBubbleFromPosition(event->pos().x());
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void QuickNavSeekSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        updateBubbleFromPosition(event->pos().x());
        emit seekCommitted(value() - 1);
        event->accept();
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

void QuickNavSeekSlider::enterEvent(QEnterEvent* event) {
    m_isHovered = true;
    update();
    QSlider::enterEvent(event);
}

void QuickNavSeekSlider::leaveEvent(QEvent* event) {
    m_isHovered = false;
    update();
    QSlider::leaveEvent(event);
}

void QuickNavSeekSlider::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int trackMargin = 12;
    const int trackY = height() / 2;
    const int trackW = width() - (trackMargin * 2);

    if (trackW <= 0) return;

    // Track Background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 40));
    p.drawRoundedRect(trackMargin, trackY - 3, trackW, 6, 3, 3);

    // Track Progress
    double progressRatio = (m_totalPages > 1)
                               ? static_cast<double>(value() - 1) / static_cast<double>(m_totalPages - 1)
                               : 1.0;
    progressRatio = std::clamp(progressRatio, 0.0, 1.0);
    int activeW = static_cast<int>(progressRatio * trackW);

    p.setBrush(m_tokens.accentColor.isValid() ? m_tokens.accentColor : QColor(37, 99, 235));
    p.drawRoundedRect(trackMargin, trackY - 3, activeW, 6, 3, 3);

    // Thumb Handle
    int thumbX = trackMargin + activeW;
    p.setBrush(Qt::white);
    p.setPen(QPen(m_tokens.accentColor.isValid() ? m_tokens.accentColor : QColor(37, 99, 235), 2));
    p.drawEllipse(QPoint(thumbX, trackY), 7, 7);

    // Floating Bubble when dragging or hovered
    if (m_isDragging || m_isHovered) {
        QFont bubbleFont = font();
        bubbleFont.setPointSize(9);
        bubbleFont.setBold(true);
        p.setFont(bubbleFont);

        QFontMetrics fm(bubbleFont);
        int bubbleW = fm.horizontalAdvance(m_bubbleText) + 16;
        int bubbleH = 22;
        int bubbleX = std::clamp(thumbX - (bubbleW / 2), 4, width() - bubbleW - 4);
        int bubbleY = trackY - 26;

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(15, 23, 42, 230)); // Deep dark bubble
        p.drawRoundedRect(bubbleX, bubbleY, bubbleW, bubbleH, 6, 6);

        p.setPen(Qt::white);
        p.drawText(QRect(bubbleX, bubbleY, bubbleW, bubbleH), Qt::AlignCenter, m_bubbleText);
    }
}

// ============================================================================
// QuickNavPopoverWidget
// ============================================================================

QuickNavPopoverWidget::QuickNavPopoverWidget(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::SubWindow);
    setAttribute(Qt::WA_StyledBackground, true);
    setupUi();
    updateStyles();
}

void QuickNavPopoverWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 14, 18, 14);
    mainLayout->setSpacing(10);

    // Top Row: Chapter navigation & Close
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_prevChapterBtn = new QPushButton(QString::fromUtf8("⏮ Prev"), this);
    m_prevChapterBtn->setCursor(Qt::PointingHandCursor);
    m_prevChapterBtn->setFixedHeight(32);
    connect(m_prevChapterBtn, &QPushButton::clicked, this, &QuickNavPopoverWidget::goToPreviousChapter);
    topRow->addWidget(m_prevChapterBtn);

    m_chapterCombo = new QComboBox(this);
    m_chapterCombo->setCursor(Qt::PointingHandCursor);
    m_chapterCombo->setFixedHeight(32);
    m_chapterCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_chapterCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx >= 0 && idx != m_currentChapter) {
            emit chapterJumpRequested(idx);
        }
    });
    topRow->addWidget(m_chapterCombo, 1);

    m_nextChapterBtn = new QPushButton(QString::fromUtf8("Next ⏭"), this);
    m_nextChapterBtn->setCursor(Qt::PointingHandCursor);
    m_nextChapterBtn->setFixedHeight(32);
    connect(m_nextChapterBtn, &QPushButton::clicked, this, &QuickNavPopoverWidget::goToNextChapter);
    topRow->addWidget(m_nextChapterBtn);

    m_closeBtn = new QPushButton(QString::fromUtf8("✕"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(32, 32);
    connect(m_closeBtn, &QPushButton::clicked, this, &QuickNavPopoverWidget::closeRequested);
    topRow->addWidget(m_closeBtn);

    mainLayout->addLayout(topRow);

    // Slider Row
    auto* sliderRow = new QHBoxLayout();
    m_slider = new QuickNavSeekSlider(Qt::Horizontal, this);
    connect(m_slider, &QuickNavSeekSlider::seekPageChanged, this, [this](int pageIdx) {
        emit pageJumpRequested(pageIdx);
    });
    connect(m_slider, &QuickNavSeekSlider::seekCommitted, this, [this](int pageIdx) {
        emit pageJumpRequested(pageIdx);
    });
    sliderRow->addWidget(m_slider, 1);
    mainLayout->addLayout(sliderRow);

    // Bottom Navigation Bar: Direct Page Jump & WPM Reading Time Remaining
    auto* botRow = new QHBoxLayout();
    botRow->setSpacing(10);

    // Direct Jump
    m_goToPageBtn = new QPushButton(QString::fromUtf8("📄 Go to Page..."), this);
    m_goToPageBtn->setCursor(Qt::PointingHandCursor);
    m_goToPageBtn->setFixedHeight(28);
    connect(m_goToPageBtn, &QPushButton::clicked, this, &QuickNavPopoverWidget::togglePageInput);
    botRow->addWidget(m_goToPageBtn);

    m_directJumpContainer = new QWidget(this);
    auto* jumpLayout = new QHBoxLayout(m_directJumpContainer);
    jumpLayout->setContentsMargins(0, 0, 0, 0);
    jumpLayout->setSpacing(4);

    m_pageSpinBox = new QSpinBox(m_directJumpContainer);
    m_pageSpinBox->setRange(1, 1);
    m_pageSpinBox->setFixedHeight(28);
    jumpLayout->addWidget(m_pageSpinBox);

    m_pageConfirmBtn = new QPushButton(QString::fromUtf8("Go"), m_directJumpContainer);
    m_pageConfirmBtn->setCursor(Qt::PointingHandCursor);
    m_pageConfirmBtn->setFixedHeight(28);
    connect(m_pageConfirmBtn, &QPushButton::clicked, this, &QuickNavPopoverWidget::applyDirectPageJump);
    jumpLayout->addWidget(m_pageConfirmBtn);

    m_directJumpContainer->setVisible(false);
    botRow->addWidget(m_directJumpContainer);

    botRow->addStretch();

    // Time estimate label
    m_timeEstimateLabel = new QLabel(this);
    m_timeEstimateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    botRow->addWidget(m_timeEstimateLabel);

    // WPM adjustment controls
    m_wpmMinusBtn = new QPushButton(QString::fromUtf8("−"), this);
    m_wpmMinusBtn->setFixedSize(24, 24);
    m_wpmMinusBtn->setCursor(Qt::PointingHandCursor);
    connect(m_wpmMinusBtn, &QPushButton::clicked, this, &QuickNavPopoverWidget::decreaseWpm);
    botRow->addWidget(m_wpmMinusBtn);

    m_wpmLabel = new QLabel(this);
    botRow->addWidget(m_wpmLabel);

    m_wpmPlusBtn = new QPushButton(QString::fromUtf8("+"), this);
    m_wpmPlusBtn->setFixedSize(24, 24);
    m_wpmPlusBtn->setCursor(Qt::PointingHandCursor);
    connect(m_wpmPlusBtn, &QPushButton::clicked, this, &QuickNavPopoverWidget::increaseWpm);
    botRow->addWidget(m_wpmPlusBtn);

    mainLayout->addLayout(botRow);

    updateWpmLabel();
    updateTimeEstimateLabel();
}

void QuickNavPopoverWidget::updateStyles() {
    setStyleSheet(
        "LunaRead--QuickNavPopoverWidget {"
        "  background-color: rgba(15, 23, 42, 0.94);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 14px;"
        "}"
        "QPushButton {"
        "  background: rgba(255, 255, 255, 0.10);"
        "  color: #F8FAFC;"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 6px;"
        "  font-size: 9.5pt;"
        "  font-weight: 500;"
        "  padding: 3px 8px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 255, 255, 0.18);"
        "}"
        "QComboBox {"
        "  background: rgba(255, 255, 255, 0.10);"
        "  color: #F8FAFC;"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 6px;"
        "  padding: 2px 8px;"
        "  font-size: 9.5pt;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #0F172A;"
        "  color: #F8FAFC;"
        "  selection-background-color: #2563EB;"
        "}"
        "QSpinBox {"
        "  background: rgba(255, 255, 255, 0.12);"
        "  color: #FFFFFF;"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "  border-radius: 4px;"
        "  padding: 2px 4px;"
        "}"
        "QLabel {"
        "  color: #CBD5E1;"
        "  font-size: 9.5pt;"
        "}"
    );
}

void QuickNavPopoverWidget::setBookTitle(const QString& title) {
    m_bookTitle = title;
}

void QuickNavPopoverWidget::setChapters(const QVector<QString>& chapterTitles, int currentChapter) {
    m_chapterTitles = chapterTitles;
    m_totalChapters = std::max(1, static_cast<int>(chapterTitles.size()));
    m_currentChapter = std::clamp(currentChapter, 0, m_totalChapters - 1);

    m_chapterCombo->blockSignals(true);
    m_chapterCombo->clear();
    for (int i = 0; i < chapterTitles.size(); ++i) {
        m_chapterCombo->addItem(QStringLiteral("%1. %2").arg(i + 1).arg(chapterTitles[i]));
    }
    m_chapterCombo->setCurrentIndex(m_currentChapter);
    m_chapterCombo->blockSignals(false);

    updateChapterUi();
}

void QuickNavPopoverWidget::setChapterInfo(int currentChapter, int totalChapters, const QString& title) {
    m_currentChapter = currentChapter;
    m_totalChapters = std::max(1, totalChapters);
    m_chapterTitle = title;
    updateChapterUi();
}

void QuickNavPopoverWidget::updateChapterUi() {
    m_prevChapterBtn->setEnabled(m_currentChapter > 0);
    m_nextChapterBtn->setEnabled(m_currentChapter + 1 < m_totalChapters);
}

void QuickNavPopoverWidget::setPageProgress(int currentPageIndex, int totalPages) {
    m_currentPageIndex = currentPageIndex;
    m_totalPages = std::max(1, totalPages);
    m_slider->setTotalPages(m_totalPages);
    m_slider->setCurrentPage(currentPageIndex + 1);
    m_pageSpinBox->setRange(1, m_totalPages);
    m_pageSpinBox->setValue(currentPageIndex + 1);
    updateTimeEstimateLabel();
}

void QuickNavPopoverWidget::setWordsRemaining(int chapterWordsLeft, int bookWordsLeft) {
    m_chapterWordsLeft = std::max(0, chapterWordsLeft);
    m_bookWordsLeft = std::max(0, bookWordsLeft);
    updateTimeEstimateLabel();
}

void QuickNavPopoverWidget::setWordsPerMinute(int wpm) {
    m_wpm = std::clamp(wpm, MinWpm, MaxWpm);
    updateWpmLabel();
    updateTimeEstimateLabel();
}

void QuickNavPopoverWidget::recordReadingSession(int wordsRead, qint64 elapsedMs) {
    if (wordsRead <= 0 || elapsedMs < 2000) return;
    double minutes = static_cast<double>(elapsedMs) / 60000.0;
    int measuredWpm = static_cast<int>(std::round(wordsRead / minutes));
    if (measuredWpm >= MinWpm && measuredWpm <= MaxWpm) {
        m_wpmSamples.append(measuredWpm);
        if (m_wpmSamples.size() > 10) m_wpmSamples.removeFirst();
        int sum = 0;
        for (int sample : m_wpmSamples) sum += sample;
        setWordsPerMinute(sum / m_wpmSamples.size());
    }
}

void QuickNavPopoverWidget::calibrateWpm(int targetWpm) {
    setWordsPerMinute(targetWpm);
}

void QuickNavPopoverWidget::resetWpmToDefault() {
    setWordsPerMinute(DefaultWpm);
}

QString QuickNavPopoverWidget::formatDuration(double minutes) {
    if (minutes < 1.0) {
        return QStringLiteral("< 1 min");
    } else if (minutes < 60.0) {
        return QStringLiteral("~%1 mins").arg(static_cast<int>(std::round(minutes)));
    } else {
        double hours = minutes / 60.0;
        return QStringLiteral("~%1 hrs").arg(hours, 0, 'f', 1);
    }
}

QString QuickNavPopoverWidget::readingTimeEstimateText() const {
    if (m_customEstimateText.has_value()) {
        return *m_customEstimateText;
    }
    double chapterMins = static_cast<double>(m_chapterWordsLeft) / static_cast<double>(m_wpm);
    double bookMins = static_cast<double>(m_bookWordsLeft) / static_cast<double>(m_wpm);

    return QStringLiteral("⏱️ %1 in chapter | %2 in book")
        .arg(formatDuration(chapterMins))
        .arg(formatDuration(bookMins));
}

void QuickNavPopoverWidget::setReadingTimeEstimateText(const QString& text) {
    m_customEstimateText = text;
    updateTimeEstimateLabel();
}

void QuickNavPopoverWidget::updateTimeEstimateLabel() {
    m_timeEstimateLabel->setText(readingTimeEstimateText());
}

void QuickNavPopoverWidget::updateWpmLabel() {
    m_wpmLabel->setText(QStringLiteral("%1 WPM").arg(m_wpm));
}

void QuickNavPopoverWidget::goToPreviousChapter() {
    if (m_currentChapter > 0) {
        emit chapterJumpRequested(m_currentChapter - 1);
    }
}

void QuickNavPopoverWidget::goToNextChapter() {
    if (m_currentChapter + 1 < m_totalChapters) {
        emit chapterJumpRequested(m_currentChapter + 1);
    }
}

void QuickNavPopoverWidget::jumpToPageDirect(int pageNumber) {
    emit pageJumpRequested(pageNumber - 1);
}

void QuickNavPopoverWidget::togglePageInput() {
    bool visible = !m_directJumpContainer->isVisible();
    m_directJumpContainer->setVisible(visible);
    if (visible) {
        m_pageSpinBox->setFocus();
        m_pageSpinBox->selectAll();
    }
}

void QuickNavPopoverWidget::applyDirectPageJump() {
    int target = m_pageSpinBox->value();
    jumpToPageDirect(target);
    m_directJumpContainer->setVisible(false);
}

void QuickNavPopoverWidget::increaseWpm() {
    setWordsPerMinute(m_wpm + 20);
    emit wpmChanged(m_wpm);
}

void QuickNavPopoverWidget::decreaseWpm() {
    setWordsPerMinute(m_wpm - 20);
    emit wpmChanged(m_wpm);
}

void QuickNavPopoverWidget::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    m_slider->applyTheme(tokens);
}

void QuickNavPopoverWidget::showAtBottom(QWidget* targetParent) {
    reposition(targetParent);
    show();
    raise();
}

void QuickNavPopoverWidget::reposition(QWidget* targetParent) {
    QWidget* p = targetParent ? targetParent : parentWidget();
    if (!p) return;

    int popoverW = std::min(p->width() - 40, 720);
    int popoverH = sizeHint().height();
    int x = (p->width() - popoverW) / 2;
    int y = p->height() - popoverH - 42; // Above reading footer bar
    setGeometry(x, y, popoverW, popoverH);
}

void QuickNavPopoverWidget::togglePopover(QWidget* targetParent) {
    if (isVisible()) {
        hide();
    } else {
        showAtBottom(targetParent);
    }
}

QSize QuickNavPopoverWidget::sizeHint() const {
    return QSize(680, 115);
}

void QuickNavPopoverWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(QColor(255, 255, 255, 38), 1));
    p.setBrush(QColor(15, 23, 42, 240));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);
}

void QuickNavPopoverWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QuickNavPopoverWidget::mousePressEvent(QMouseEvent* event) {
    event->accept(); // Prevent clicking through to viewport
}

} // namespace LunaRead
