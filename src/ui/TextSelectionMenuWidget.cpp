#include "TextSelectionMenuWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QClipboard>
#include <QScreen>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>

namespace LunaRead {

// ============================================================================
// HighlighterCircleButton Implementation
// ============================================================================

HighlighterCircleButton::HighlighterCircleButton(const QColor& color, const QString& colorName, QWidget* parent)
    : QPushButton(parent)
    , m_color(color)
    , m_colorName(colorName)
{
    setFixedSize(22, 22);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Highlight %1 (%2)").arg(m_colorName, m_color.name().toUpper()));
}

void HighlighterCircleButton::setColor(const QColor& color) {
    m_color = color;
    update();
}

void HighlighterCircleButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal w = static_cast<qreal>(width());
    const qreal h = static_cast<qreal>(height());
    const QRectF circleRect(1.5, 1.5, w - 3.0, h - 3.0);

    if (isDown()) {
        p.setPen(QPen(QColor(0, 0, 0, 80), 1.5));
        p.setBrush(m_color.darker(115));
        p.drawEllipse(circleRect.adjusted(1.0, 1.0, -1.0, -1.0));
    } else if (underMouse()) {
        // Outer halo ring
        p.setPen(QPen(m_color, 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(circleRect);

        // Inner solid circle with crisp border
        p.setPen(QPen(QColor(0, 0, 0, 50), 1.0));
        p.setBrush(m_color);
        p.drawEllipse(circleRect.adjusted(2.5, 2.5, -2.5, -2.5));

        // Subtle white center highlight dot
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 130));
        p.drawEllipse(QRectF(w / 2.0 - 2.0, h / 2.0 - 2.0, 4.0, 4.0));
    } else {
        // Normal state: crisp circle with subtle 1px border
        p.setPen(QPen(QColor(0, 0, 0, 45), 1.0));
        p.setBrush(m_color);
        p.drawEllipse(circleRect);
    }
}

// ============================================================================
// TextSelectionMenuWidget Implementation
// ============================================================================

const QVector<TextSelectionMenuWidget::HighlightColorOption>& TextSelectionMenuWidget::defaultHighlightColors() {
    static const QVector<HighlightColorOption> colors = {
        {QStringLiteral("Yellow"), QColor(0xFF, 0xE0, 0x82)},
        {QStringLiteral("Green"),  QColor(0xA5, 0xD6, 0xA7)},
        {QStringLiteral("Cyan"),   QColor(0x80, 0xDE, 0xEA)},
        {QStringLiteral("Pink"),   QColor(0xF4, 0x8F, 0xB1)},
        {QStringLiteral("Purple"), QColor(0xCE, 0x93, 0xD8)}
    };
    return colors;
}

TextSelectionMenuWidget::TextSelectionMenuWidget(QWidget* parent)
    : QWidget(parent)
    , m_tokens(ReaderThemeTokens::night())
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    if (parent) {
        setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
        parent->installEventFilter(this);
    } else {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    }

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 95));
    setGraphicsEffect(shadow);

    setupUi();
    setupStyle();
}

TextSelectionMenuWidget::~TextSelectionMenuWidget() {
    if (parentWidget()) {
        parentWidget()->removeEventFilter(this);
    }
}

void TextSelectionMenuWidget::setSelectedText(const QString& text) {
    m_selectedText = text;
}

QString TextSelectionMenuWidget::selectedText() const {
    return m_selectedText;
}

void TextSelectionMenuWidget::showAboveSelection(const QRect& selectionRect, const QPoint& globalOffset) {
    if (parentWidget() && parentWidget()->width() < 420) {
        setCompactMode(true);
    }

    adjustSize();
    const QSize s = sizeHint();
    resize(s);

    QRect targetSelection = selectionRect;
    if (targetSelection.isNull() || !targetSelection.isValid()) {
        if (parentWidget()) {
            targetSelection = QRect(parentWidget()->rect().center(), QSize(1, 1));
        } else {
            targetSelection = QRect(QPoint(100, 100), QSize(1, 1));
        }
    }

    // Determine viewport bounds
    int boundsLeft = 0;
    int boundsRight = 1920;
    int boundsTop = 0;
    int boundsBottom = 1080;

    if (parentWidget()) {
        const QRect pr = parentWidget()->rect();
        boundsLeft = pr.left();
        boundsRight = pr.right();
        boundsTop = pr.top();
        boundsBottom = pr.bottom();
    } else if (auto* screen = QGuiApplication::primaryScreen()) {
        const QRect sr = screen->availableGeometry();
        boundsLeft = sr.left();
        boundsRight = sr.right();
        boundsTop = sr.top();
        boundsBottom = sr.bottom();
    }

    // Horizontal centering
    int x = targetSelection.center().x() - s.width() / 2 + globalOffset.x();

    // Position above selection (offset by ShadowMargin)
    constexpr int verticalGap = 6;
    int y = targetSelection.top() - s.height() + ShadowMargin - verticalGap + globalOffset.y();

    // If overflowing top bound, flip to appear below selection
    if (y < boundsTop + 4) {
        y = targetSelection.bottom() - ShadowMargin + verticalGap + globalOffset.y();
    }

    // Clamp horizontally and vertically
    x = std::clamp(x, boundsLeft + 4, std::max(boundsLeft + 4, boundsRight - s.width() - 4));
    y = std::clamp(y, boundsTop + 4, std::max(boundsTop + 4, boundsBottom - s.height() - 4));

    move(x, y);
    show();
    raise();
}

void TextSelectionMenuWidget::showAt(const QPoint& targetPos, const QRect& bounds) {
    adjustSize();
    const QSize s = sizeHint();
    resize(s);

    QRect effectiveBounds = bounds;
    if (effectiveBounds.isEmpty()) {
        if (parentWidget()) {
            effectiveBounds = parentWidget()->rect();
        } else if (auto* screen = QGuiApplication::primaryScreen()) {
            effectiveBounds = screen->availableGeometry();
        }
    }

    int x = targetPos.x() - s.width() / 2;
    constexpr int verticalGap = 6;
    int y = targetPos.y() - s.height() + ShadowMargin - verticalGap;

    if (!effectiveBounds.isEmpty()) {
        if (y < effectiveBounds.top() + 4) {
            y = targetPos.y() - ShadowMargin + verticalGap;
        }
        x = std::clamp(x, effectiveBounds.left() + 4, std::max(effectiveBounds.left() + 4, effectiveBounds.right() - s.width() - 4));
        y = std::clamp(y, effectiveBounds.top() + 4, std::max(effectiveBounds.top() + 4, effectiveBounds.bottom() - s.height() - 4));
    }

    move(x, y);
    show();
    raise();
}

void TextSelectionMenuWidget::dismiss() {
    hide();
    emit dismissed();
}

void TextSelectionMenuWidget::updateTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    setupStyle();
    update();
}

void TextSelectionMenuWidget::setCompactMode(bool compact) {
    m_compactMode = compact;

    if (m_compactMode) {
        m_addNoteBtn->setText(QString());
        m_copyBtn->setText(QString());
        m_defineBtn->setText(QString());
        m_translateBtn->setText(QString());
        m_speakBtn->setText(QString());
        m_searchBtn->setText(QString());

        m_addNoteBtn->setFixedSize(28, 26);
        m_copyBtn->setFixedSize(28, 26);
        m_defineBtn->setFixedSize(28, 26);
        m_translateBtn->setFixedSize(28, 26);
        m_speakBtn->setFixedSize(28, 26);
        m_searchBtn->setFixedSize(28, 26);
    } else {
        m_addNoteBtn->setText(tr("Note"));
        m_copyBtn->setText(tr("Copy"));
        m_defineBtn->setText(tr("Define"));
        m_translateBtn->setText(tr("Translate"));
        m_speakBtn->setText(tr("Speak"));
        m_searchBtn->setText(tr("Search"));

        m_addNoteBtn->setMinimumSize(0, 0);
        m_addNoteBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_addNoteBtn->setFixedHeight(26);

        m_copyBtn->setMinimumSize(0, 0);
        m_copyBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_copyBtn->setFixedHeight(26);

        m_defineBtn->setMinimumSize(0, 0);
        m_defineBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_defineBtn->setFixedHeight(26);

        m_translateBtn->setMinimumSize(0, 0);
        m_translateBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_translateBtn->setFixedHeight(26);

        m_speakBtn->setMinimumSize(0, 0);
        m_speakBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_speakBtn->setFixedHeight(26);

        m_searchBtn->setMinimumSize(0, 0);
        m_searchBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_searchBtn->setFixedHeight(26);
    }

    adjustSize();
}

void TextSelectionMenuWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(ShadowMargin + 8, ShadowMargin + 6, ShadowMargin + 8, ShadowMargin + 6);
    mainLayout->setSpacing(5);

    // ========================================================================
    // Top Row: 5 Highlighter colors | Underline, Strikethrough | Close
    // ========================================================================
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    const auto& colors = defaultHighlightColors();
    for (const auto& opt : colors) {
        auto* btn = new HighlighterCircleButton(opt.color, opt.name, this);
        m_colorButtons.append(btn);
        topRow->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, c = opt.color]() {
            emit highlightRequested(m_selectedText, c);
            emit highlightColorRequested(c);
            if (m_dismissOnAction) {
                dismiss();
            }
        });
    }

    // Vertical Divider
    m_vDivider = new QFrame(this);
    m_vDivider->setObjectName("menuVDivider");
    m_vDivider->setFrameShape(QFrame::VLine);
    m_vDivider->setFrameShadow(QFrame::Plain);
    m_vDivider->setFixedHeight(16);
    topRow->addWidget(m_vDivider);

    // Underline Button
    m_underlineBtn = new QPushButton(this);
    m_underlineBtn->setObjectName("formatBtn");
    m_underlineBtn->setFixedSize(28, 24);
    m_underlineBtn->setCursor(Qt::PointingHandCursor);
    m_underlineBtn->setToolTip(tr("Underline"));
    connect(m_underlineBtn, &QPushButton::clicked, this, [this]() {
        emit underlineRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    topRow->addWidget(m_underlineBtn);

    // Strikethrough Button
    m_strikethroughBtn = new QPushButton(this);
    m_strikethroughBtn->setObjectName("formatBtn");
    m_strikethroughBtn->setFixedSize(28, 24);
    m_strikethroughBtn->setCursor(Qt::PointingHandCursor);
    m_strikethroughBtn->setToolTip(tr("Strikethrough"));
    connect(m_strikethroughBtn, &QPushButton::clicked, this, [this]() {
        emit strikethroughRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    topRow->addWidget(m_strikethroughBtn);

    topRow->addStretch(1);

    // Dismiss / Close Button
    m_closeBtn = new QPushButton(this);
    m_closeBtn->setObjectName("closeBtn");
    m_closeBtn->setFixedSize(22, 22);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("Dismiss"));
    connect(m_closeBtn, &QPushButton::clicked, this, &TextSelectionMenuWidget::dismiss);
    topRow->addWidget(m_closeBtn);

    mainLayout->addLayout(topRow);

    // ========================================================================
    // Subtle Horizontal Divider
    // ========================================================================
    m_hDivider = new QFrame(this);
    m_hDivider->setObjectName("menuHDivider");
    m_hDivider->setFrameShape(QFrame::HLine);
    m_hDivider->setFrameShadow(QFrame::Plain);
    m_hDivider->setFixedHeight(1);
    mainLayout->addWidget(m_hDivider);

    // ========================================================================
    // Bottom Row: Action Buttons
    // ========================================================================
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);

    // Add Note
    m_addNoteBtn = new QPushButton(tr("Note"), this);
    m_addNoteBtn->setObjectName("actionBtn");
    m_addNoteBtn->setFixedHeight(26);
    m_addNoteBtn->setCursor(Qt::PointingHandCursor);
    m_addNoteBtn->setToolTip(tr("Add Note"));
    connect(m_addNoteBtn, &QPushButton::clicked, this, [this]() {
        emit addNoteRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    bottomRow->addWidget(m_addNoteBtn);

    // Copy
    m_copyBtn = new QPushButton(tr("Copy"), this);
    m_copyBtn->setObjectName("actionBtn");
    m_copyBtn->setFixedHeight(26);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setToolTip(tr("Copy to Clipboard"));
    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        if (auto* clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(m_selectedText);
        }
        emit copyRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    bottomRow->addWidget(m_copyBtn);

    // Dictionary / Define
    m_defineBtn = new QPushButton(tr("Define"), this);
    m_defineBtn->setObjectName("actionBtn");
    m_defineBtn->setFixedHeight(26);
    m_defineBtn->setCursor(Qt::PointingHandCursor);
    m_defineBtn->setToolTip(tr("Dictionary / Define"));
    connect(m_defineBtn, &QPushButton::clicked, this, [this]() {
        emit defineRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    bottomRow->addWidget(m_defineBtn);

    // Translate
    m_translateBtn = new QPushButton(tr("Translate"), this);
    m_translateBtn->setObjectName("actionBtn");
    m_translateBtn->setFixedHeight(26);
    m_translateBtn->setCursor(Qt::PointingHandCursor);
    m_translateBtn->setToolTip(tr("Translate"));
    connect(m_translateBtn, &QPushButton::clicked, this, [this]() {
        emit translateRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    bottomRow->addWidget(m_translateBtn);

    // Speak / TTS
    m_speakBtn = new QPushButton(tr("Speak"), this);
    m_speakBtn->setObjectName("actionBtn");
    m_speakBtn->setFixedHeight(26);
    m_speakBtn->setCursor(Qt::PointingHandCursor);
    m_speakBtn->setToolTip(tr("Speak / TTS"));
    connect(m_speakBtn, &QPushButton::clicked, this, [this]() {
        emit speakRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    bottomRow->addWidget(m_speakBtn);

    // Search in Book
    m_searchBtn = new QPushButton(tr("Search"), this);
    m_searchBtn->setObjectName("actionBtn");
    m_searchBtn->setFixedHeight(26);
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    m_searchBtn->setToolTip(tr("Search in Book"));
    connect(m_searchBtn, &QPushButton::clicked, this, [this]() {
        emit searchRequested(m_selectedText);
        if (m_dismissOnAction) {
            dismiss();
        }
    });
    bottomRow->addWidget(m_searchBtn);

    mainLayout->addLayout(bottomRow);
}

void TextSelectionMenuWidget::setupStyle() {
    const QString accentHex = m_tokens.accentColor.isValid()
        ? m_tokens.accentColor.name()
        : QStringLiteral("#3B82F6");

    const QString textHex = m_tokens.isDark ? QStringLiteral("#E5E7EB") : QStringLiteral("#1F2937");
    const QString hoverBg = m_tokens.isDark ? QStringLiteral("rgba(255, 255, 255, 0.12)") : QStringLiteral("rgba(0, 0, 0, 0.08)");
    const QString pressedBg = m_tokens.isDark ? QStringLiteral("rgba(255, 255, 255, 0.22)") : QStringLiteral("rgba(0, 0, 0, 0.16)");
    const QString pillBg = m_tokens.isDark ? QStringLiteral("rgba(255, 255, 255, 0.06)") : QStringLiteral("rgba(0, 0, 0, 0.04)");
    const QString pillBorder = m_tokens.isDark ? QStringLiteral("rgba(255, 255, 255, 0.12)") : QStringLiteral("rgba(0, 0, 0, 0.10)");
    const QString dividerHex = m_tokens.isDark ? QStringLiteral("rgba(255, 255, 255, 0.14)") : QStringLiteral("rgba(0, 0, 0, 0.10)");

    setStyleSheet(QString(R"(
        QPushButton#formatBtn {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 2px 4px;
        }
        QPushButton#formatBtn:hover {
            background-color: %3;
            border-color: %4;
        }
        QPushButton#formatBtn:pressed {
            background-color: %5;
        }

        QPushButton#actionBtn {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 3px 8px;
            color: %6;
            font-size: 11px;
            font-weight: 500;
        }
        QPushButton#actionBtn:hover {
            background-color: %3;
            border-color: %4;
        }
        QPushButton#actionBtn:pressed {
            background-color: %5;
        }

        QPushButton#closeBtn {
            background-color: transparent;
            border: none;
            border-radius: 11px;
        }
        QPushButton#closeBtn:hover {
            background-color: rgba(239, 68, 68, 0.25);
        }
        QPushButton#closeBtn:pressed {
            background-color: rgba(239, 68, 68, 0.45);
        }

        QFrame#menuHDivider, QFrame#menuVDivider {
            background-color: %7;
            border: none;
        }
    )")
    .arg(pillBg, pillBorder, hoverBg, accentHex, pressedBg, textHex, dividerHex));

    // Update icons with matching theme color
    const QColor iconColor = m_tokens.isDark ? QColor(229, 231, 235) : QColor(55, 65, 81);
    const QColor closeColor = m_tokens.isDark ? QColor(156, 163, 175) : QColor(107, 114, 128);

    m_underlineBtn->setIcon(drawUnderlineIcon(iconColor));
    m_strikethroughBtn->setIcon(drawStrikethroughIcon(iconColor));
    m_addNoteBtn->setIcon(drawNoteIcon(iconColor));
    m_copyBtn->setIcon(drawCopyIcon(iconColor));
    m_defineBtn->setIcon(drawDefineIcon(iconColor));
    m_translateBtn->setIcon(drawTranslateIcon(iconColor));
    m_speakBtn->setIcon(drawSpeakIcon(iconColor));
    m_searchBtn->setIcon(drawSearchIcon(iconColor));
    m_closeBtn->setIcon(drawCloseIcon(closeColor));
}

void TextSelectionMenuWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal m = static_cast<qreal>(ShadowMargin);
    const QRectF cardRect(m, m, static_cast<qreal>(width()) - 2.0 * m, static_cast<qreal>(height()) - 2.0 * m);
    constexpr qreal cornerRadius = 14.0;

    QPainterPath path;
    path.addRoundedRect(cardRect, cornerRadius, cornerRadius);

    // Frosted glass background fill
    QColor bgColor;
    QColor outlineColor;
    if (m_tokens.isDark) {
        bgColor = QColor(24, 28, 36, 235);
        outlineColor = QColor(255, 255, 255, 38);
    } else {
        bgColor = QColor(252, 253, 254, 245);
        outlineColor = QColor(0, 0, 0, 36);
    }

    painter.fillPath(path, bgColor);

    // Subtle glossy top sheen
    const QRectF glossRect(cardRect.x(), cardRect.y(), cardRect.width(), cardRect.height() * 0.45);
    QPainterPath glossPath;
    glossPath.addRoundedRect(glossRect, cornerRadius, cornerRadius);

    QLinearGradient gloss(0.0, glossRect.top(), 0.0, glossRect.bottom());
    if (m_tokens.isDark) {
        gloss.setColorAt(0.0, QColor(255, 255, 255, 18));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    } else {
        gloss.setColorAt(0.0, QColor(255, 255, 255, 65));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    }
    painter.fillPath(glossPath, gloss);

    // Outline stroke
    QPen borderPen(outlineColor, 1.2);
    painter.strokePath(path, borderPen);
}

bool TextSelectionMenuWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (isVisible()) {
                const QPoint localPos = mapFromParent(me->pos());
                if (!rect().contains(localPos)) {
                    dismiss();
                }
            }
        } else if (event->type() == QEvent::Resize) {
            if (isVisible()) {
                dismiss();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TextSelectionMenuWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void TextSelectionMenuWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    raise();
}

// ============================================================================
// Vector Icon Generators
// ============================================================================

QIcon TextSelectionMenuWidget::drawUnderlineIcon(const QColor& color) const {
    constexpr int s = 18;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    QPainterPath uPath;
    uPath.moveTo(4.5, 3.0);
    uPath.lineTo(4.5, 8.5);
    uPath.quadTo(4.5, 12.0, 9.0, 12.0);
    uPath.quadTo(13.5, 12.0, 13.5, 8.5);
    uPath.lineTo(13.5, 3.0);
    p.drawPath(uPath);

    p.drawLine(QPointF(3.0, 15.0), QPointF(15.0, 15.0));
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawStrikethroughIcon(const QColor& color) const {
    constexpr int s = 18;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    QPainterPath sPath;
    sPath.moveTo(13.0, 4.5);
    sPath.cubicTo(12.0, 3.0, 6.0, 2.5, 6.0, 6.5);
    sPath.cubicTo(6.0, 9.0, 12.0, 9.0, 12.0, 12.0);
    sPath.cubicTo(12.0, 15.5, 5.5, 15.0, 5.0, 13.5);
    p.drawPath(sPath);

    p.drawLine(QPointF(2.5, 9.0), QPointF(15.5, 9.0));
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawNoteIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    p.drawRoundedRect(QRectF(2.5, 2.0, 9.0, 12.0), 1.5, 1.5);
    p.drawLine(QPointF(4.5, 5.0), QPointF(8.5, 5.0));
    p.drawLine(QPointF(4.5, 8.0), QPointF(8.5, 8.0));

    // Pencil accent
    QPen pencilPen(color, 1.8, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pencilPen);
    p.drawLine(QPointF(9.5, 13.5), QPointF(14.0, 9.0));
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawCopyIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    // Back card
    p.drawRoundedRect(QRectF(2.0, 2.0, 8.0, 9.5), 1.2, 1.2);
    // Front card
    p.drawRoundedRect(QRectF(5.5, 4.5, 8.5, 10.0), 1.2, 1.2);
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawDefineIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    // Open book pages
    QPainterPath book;
    book.moveTo(8.0, 3.5);
    book.quadTo(5.0, 2.5, 2.0, 3.0);
    book.lineTo(2.0, 12.5);
    book.quadTo(5.0, 12.0, 8.0, 13.0);
    book.quadTo(11.0, 12.0, 14.0, 12.5);
    book.lineTo(14.0, 3.0);
    book.quadTo(11.0, 2.5, 8.0, 3.5);
    book.closeSubpath();
    p.drawPath(book);
    p.drawLine(QPointF(8.0, 3.5), QPointF(8.0, 13.0));
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawTranslateIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    // Globe
    p.drawEllipse(QRectF(2.0, 2.0, 12.0, 12.0));
    p.drawLine(QPointF(2.0, 8.0), QPointF(14.0, 8.0));
    p.drawEllipse(QRectF(5.0, 2.0, 6.0, 12.0));
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawSpeakIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(color);

    QPainterPath cone;
    cone.moveTo(2.5, 5.5);
    cone.lineTo(5.0, 5.5);
    cone.lineTo(8.5, 2.5);
    cone.lineTo(8.5, 13.5);
    cone.lineTo(5.0, 10.5);
    cone.lineTo(2.5, 10.5);
    cone.closeSubpath();
    p.drawPath(cone);

    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(8.5, 5.0, 4.0, 6.0), -50 * 16, 100 * 16);
    p.drawArc(QRectF(8.5, 3.0, 7.0, 10.0), -55 * 16, 110 * 16);
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawSearchIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);

    p.drawEllipse(QRectF(2.5, 2.5, 8.0, 8.0));
    p.drawLine(QPointF(8.5, 8.5), QPointF(13.5, 13.5));
    return QIcon(pix);
}

QIcon TextSelectionMenuWidget::drawCloseIcon(const QColor& color) const {
    constexpr int s = 16;
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));

    p.drawLine(QPointF(4.0, 4.0), QPointF(12.0, 12.0));
    p.drawLine(QPointF(12.0, 4.0), QPointF(4.0, 12.0));
    return QIcon(pix);
}

} // namespace LunaRead
