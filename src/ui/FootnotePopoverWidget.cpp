#include "FootnotePopoverWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>

namespace LunaRead {

FootnotePopoverWidget::FootnotePopoverWidget(QWidget* parent)
    : QWidget(parent)
    , m_tokens(ReaderThemeTokens::modernLight()) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(420, 260);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 100));
    setGraphicsEffect(shadow);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(10);

    // Header Row
    auto* headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(tr("📌 Note Reference"), this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();

    m_closeBtn = new QPushButton(tr("✕"), this);
    m_closeBtn->setFixedSize(26, 26);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit closeRequested();
    });
    headerLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(headerLayout);

    // Note Browser
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setStyleSheet("QTextBrowser { background: transparent; border: none; font-size: 13px; line-height: 1.5; }");
    mainLayout->addWidget(m_browser, 1);

    // Action Row
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    m_jumpBtn = new QPushButton(tr("↗ Jump to Section"), this);
    m_jumpBtn->setCursor(Qt::PointingHandCursor);
    connect(m_jumpBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit jumpRequested(m_targetChapter, m_targetPage);
    });
    bottomLayout->addWidget(m_jumpBtn);
    mainLayout->addLayout(bottomLayout);
}

void FootnotePopoverWidget::setFootnote(const QString& referenceId, const QString& contentHtml, int targetChapter, int targetPage) {
    m_targetChapter = targetChapter;
    m_targetPage = targetPage;
    m_titleLabel->setText(QString("📌 Note Reference [%1]").arg(referenceId));
    m_browser->setHtml(contentHtml);
    m_jumpBtn->setVisible(m_targetChapter >= 0);
}

void FootnotePopoverWidget::applyTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;

    QString cardBg = tokens.backgroundColor.name();
    QString textColor = tokens.textColor.name();
    QString accentColor = tokens.accentColor.name();

    m_titleLabel->setStyleSheet(QString("font-weight: bold; font-size: 13px; color: %1;").arg(accentColor));
    m_browser->setStyleSheet(QString("QTextBrowser { background: transparent; border: none; color: %1; font-size: 13px; }").arg(textColor));

    m_closeBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 13px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(128, 128, 128, 0.2); }"
    ).arg(textColor));

    m_jumpBtn->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border: none; border-radius: 6px; padding: 6px 14px; font-weight: 500; font-size: 12px; }"
        "QPushButton:hover { opacity: 0.9; }"
    ).arg(accentColor));

    update();
}

void FootnotePopoverWidget::reposition(const QSize& viewportSize, const QPointF& clickPos) {
    int targetX = static_cast<int>(clickPos.x()) - (width() / 2);
    int targetY = static_cast<int>(clickPos.y()) - height() - 16;

    // Boundary constraints
    if (targetX < 16) targetX = 16;
    if (targetX + width() > viewportSize.width() - 16) {
        targetX = viewportSize.width() - width() - 16;
    }

    if (targetY < 60) {
        // Place below click position
        targetY = static_cast<int>(clickPos.y()) + 24;
    }
    if (targetY + height() > viewportSize.height() - 60) {
        targetY = viewportSize.height() - height() - 60;
    }

    move(targetX, targetY);
}

void FootnotePopoverWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF bounds = rect().adjusted(2, 2, -2, -2);
    QPainterPath path;
    path.addRoundedRect(bounds, 12.0, 12.0);

    // Frosted card background
    QColor bg = m_tokens.backgroundColor;
    bg.setAlpha(245);
    painter.fillPath(path, bg);

    // Subtle 1px border
    QColor borderCol = m_tokens.dividerColor;
    borderCol.setAlpha(120);
    painter.setPen(QPen(borderCol, 1.2));
    painter.drawPath(path);
}

} // namespace LunaRead
