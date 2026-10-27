#include "ContinuousScrollRenderer.h"
#include "../features/ImageCropService.h"
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
    m_doc = doc;
    m_tokens = tokens;
    updateTheme(tokens);
    rebuildLayout();
}

void ContinuousScrollRenderer::setWebtoonMode(bool webtoon) {
    if (m_isWebtoonMode != webtoon) {
        m_isWebtoonMode = webtoon;
        rebuildLayout();
    }
}

void ContinuousScrollRenderer::rebuildLayout() {
    if (!m_container || !m_layout) return;

    // Clear previous children
    QLayoutItem* item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (m_isWebtoonMode) {
        // Seamless vertical strip layout for Webtoons / Manhwa: zero padding, zero spacing
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);

        int viewW = viewport() ? qMax(viewport()->width(), 400) : 800;

        for (const auto& ch : m_doc.chapters) {
            if (ch.isImagePage && !ch.imageBytes.isEmpty()) {
                QLabel* imgLabel = new QLabel(m_container);
                QPixmap pm;
                pm.loadFromData(ch.imageBytes);
                if (!pm.isNull()) {
                    pm = ImageCropService::instance().cropMargins(pm);
                    // Scale full width preserving aspect ratio
                    QPixmap scaled = pm.scaledToWidth(viewW, Qt::SmoothTransformation);
                    imgLabel->setPixmap(scaled);
                    imgLabel->setAlignment(Qt::AlignCenter);
                    imgLabel->setContentsMargins(0, 0, 0, 0);
                    m_layout->addWidget(imgLabel);
                    continue;
                }
            }

            // Fallback for text chapters in webtoon mode
            QLabel* textLabel = new QLabel(m_container);
            textLabel->setTextFormat(Qt::RichText);
            textLabel->setWordWrap(true);
            textLabel->setContentsMargins(30, 20, 30, 20);
            textLabel->setStyleSheet(QString("color: %1; font-size: 14pt; line-height: 1.6;").arg(m_tokens.textColor.name()));
            textLabel->setText(ch.htmlContent.isEmpty() ? QString("<p>%1</p>").arg(ch.textContent.toHtmlEscaped().replace("\n", "<br/>")) : ch.htmlContent);
            m_layout->addWidget(textLabel);
        }
    } else {
        // Standard continuous e-book scroll layout
        m_layout->setContentsMargins(40, 40, 40, 40);
        m_layout->setSpacing(24);

        for (const auto& ch : m_doc.chapters) {
            QLabel* titleLabel = new QLabel(QString("<h2>%1</h2>").arg(ch.title), m_container);
            titleLabel->setTextFormat(Qt::RichText);
            titleLabel->setStyleSheet(QString("color: %1;").arg(m_tokens.accentColor.name()));
            m_layout->addWidget(titleLabel);

            if (ch.isImagePage && !ch.imageBytes.isEmpty()) {
                QLabel* imgLabel = new QLabel(m_container);
                QPixmap pm;
                pm.loadFromData(ch.imageBytes);
                if (!pm.isNull()) {
                    pm = ImageCropService::instance().cropMargins(pm);
                    imgLabel->setPixmap(pm.scaledToWidth(750, Qt::SmoothTransformation));
                    imgLabel->setAlignment(Qt::AlignCenter);
                    m_layout->addWidget(imgLabel);
                    continue;
                }
            }

            QLabel* textLabel = new QLabel(m_container);
            textLabel->setTextFormat(Qt::RichText);
            textLabel->setWordWrap(true);
            textLabel->setStyleSheet(QString("color: %1; font-size: 14pt; line-height: 1.6;").arg(m_tokens.textColor.name()));
            textLabel->setText(ch.htmlContent.isEmpty() ? QString("<p>%1</p>").arg(ch.textContent.toHtmlEscaped().replace("\n", "<br/>")) : ch.htmlContent);
            m_layout->addWidget(textLabel);
        }
    }
}

void ContinuousScrollRenderer::resizeEvent(QResizeEvent* event) {
    QScrollArea::resizeEvent(event);
    if (m_isWebtoonMode && !m_doc.chapters.isEmpty()) {
        static int lastWidth = 0;
        int curWidth = viewport() ? viewport()->width() : width();
        if (qAbs(curWidth - lastWidth) > 60) {
            lastWidth = curWidth;
            rebuildLayout();
        }
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
