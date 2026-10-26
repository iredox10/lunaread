#include "ContinuousScrollRenderer.h"
#include <QScrollBar>

namespace LunaRead {

ContinuousScrollRenderer::ContinuousScrollRenderer(QWidget* parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_container = new QWidget(this);
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(40, 40, 40, 40);
    m_layout->setSpacing(24);
    setWidget(m_container);
}

void ContinuousScrollRenderer::setDocument(const BookDocument& doc, const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    updateTheme(tokens);

    // Clear previous children
    QLayoutItem* item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (const auto& ch : doc.chapters) {
        QLabel* titleLabel = new QLabel(QString("<h2>%1</h2>").arg(ch.title), m_container);
        titleLabel->setTextFormat(Qt::RichText);
        titleLabel->setStyleSheet(QString("color: %1;").arg(tokens.accentColor.name()));
        m_layout->addWidget(titleLabel);

        if (ch.isImagePage && !ch.imageBytes.isEmpty()) {
            QLabel* imgLabel = new QLabel(m_container);
            QPixmap pm;
            pm.loadFromData(ch.imageBytes);
            if (!pm.isNull()) {
                imgLabel->setPixmap(pm.scaledToWidth(750, Qt::SmoothTransformation));
                imgLabel->setAlignment(Qt::AlignCenter);
                m_layout->addWidget(imgLabel);
                continue;
            }
        }

        QLabel* textLabel = new QLabel(m_container);
        textLabel->setTextFormat(Qt::RichText);
        textLabel->setWordWrap(true);
        textLabel->setStyleSheet(QString("color: %1; font-size: 14pt; line-height: 1.6;").arg(tokens.textColor.name()));
        textLabel->setText(ch.htmlContent.isEmpty() ? QString("<p>%1</p>").arg(ch.textContent.toHtmlEscaped().replace("\n", "<br/>")) : ch.htmlContent);
        m_layout->addWidget(textLabel);
    }
}

void ContinuousScrollRenderer::updateTheme(const ReaderThemeTokens& tokens) {
    m_tokens = tokens;
    m_container->setStyleSheet(QString("background-color: %1;").arg(tokens.backgroundColor.name()));
    setStyleSheet(QString("QScrollArea { background-color: %1; border: none; }").arg(tokens.backgroundColor.name()));
}

void ContinuousScrollRenderer::scrollContentsBy(int dx, int dy) {
    QScrollArea::scrollContentsBy(dx, dy);
    int maxScroll = verticalScrollBar()->maximum();
    int current = verticalScrollBar()->value();
    if (maxScroll > 0) {
        double progress = (static_cast<double>(current) / maxScroll) * 100.0;
        emit scrollProgressChanged(progress);
    }
}

} // namespace LunaRead
