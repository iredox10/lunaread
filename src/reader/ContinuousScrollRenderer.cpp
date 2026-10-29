#include "ContinuousScrollRenderer.h"
#include "Typesetter.h"
#include "../features/ImageCropService.h"
#include "../features/PdfPageService.h"
#include <QPainter>
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

QPixmap ContinuousScrollRenderer::themifyPage(const QPixmap& page) const {
    if (m_tokens.id == QLatin1String("modernLight") || page.isNull()) return page;
    QPixmap tinted = page;
    QPainter painter(&tinted);
    Typesetter::applyPageTint(&painter, QRectF(tinted.rect()), m_tokens);
    painter.end();
    return tinted;
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

        int loadedImagePages = 0;
        for (const auto& ch : m_doc.chapters) {
            if (ch.isImagePage) {
                QByteArray bytes = ch.imageBytes;
                if (bytes.isEmpty() && m_doc.format.compare("pdf", Qt::CaseInsensitive) == 0 && loadedImagePages < 5) {
                    bytes = PdfPageService::instance().getPageImage(m_doc.filePath, m_doc.id, ch.index + 1);
                    loadedImagePages++;
                }
                if (!bytes.isEmpty()) {
                    QLabel* imgLabel = new QLabel(m_container);
                    QPixmap pm;
                    pm.loadFromData(bytes);
                    if (!pm.isNull()) {
                        pm = ImageCropService::instance().cropMargins(pm);
                        pm = themifyPage(pm);
                        // Scale full width preserving aspect ratio
                        QPixmap scaled = pm.scaledToWidth(viewW, Qt::SmoothTransformation);
                        imgLabel->setPixmap(scaled);
                        imgLabel->setAlignment(Qt::AlignCenter);
                        imgLabel->setContentsMargins(0, 0, 0, 0);
                        m_layout->addWidget(imgLabel);
                        continue;
                    }
                }
            }

            // Fallback for text chapters in webtoon mode
            QLabel* textLabel = new QLabel(m_container);
            textLabel->setTextFormat(Qt::RichText);
            textLabel->setWordWrap(true);
            textLabel->setContentsMargins(30, 20, 30, 20);
            textLabel->setStyleSheet(QString(
                "QLabel { color: %1; font-family: 'Noto Serif', 'Liberation Serif', 'Georgia', serif; font-size: 14pt; line-height: 1.6; }"
            ).arg(m_tokens.textColor.name()));

            QString contentHtml = ch.htmlContent;
            if (contentHtml.isEmpty() || (!contentHtml.contains("<p", Qt::CaseInsensitive) && !contentHtml.contains("<div", Qt::CaseInsensitive))) {
                contentHtml = Typesetter::formatChapterToBookHtml(ch.textContent, ch.title, true);
            }
            textLabel->setText(contentHtml);
            m_layout->addWidget(textLabel);
        }
    } else {
        // Standard continuous e-book scroll layout with classical typography
        m_layout->setContentsMargins(50, 40, 50, 40);
        m_layout->setSpacing(32);

        int loadedImagePages = 0;
        for (const auto& ch : m_doc.chapters) {
            if (ch.isImagePage) {
                QByteArray bytes = ch.imageBytes;
                if (bytes.isEmpty() && m_doc.format.compare("pdf", Qt::CaseInsensitive) == 0 && loadedImagePages < 5) {
                    bytes = PdfPageService::instance().getPageImage(m_doc.filePath, m_doc.id, ch.index + 1);
                    loadedImagePages++;
                }
                if (!bytes.isEmpty()) {
                    QLabel* imgLabel = new QLabel(m_container);
                    QPixmap pm;
                    pm.loadFromData(bytes);
                    if (!pm.isNull()) {
                        pm = ImageCropService::instance().cropMargins(pm);
                        pm = themifyPage(pm);
                        const int targetW = viewport() ? qMax(viewport()->width() - 100, 400) : 750;
                        imgLabel->setPixmap(pm.scaledToWidth(targetW, Qt::SmoothTransformation));
                        imgLabel->setAlignment(Qt::AlignCenter);
                        m_layout->addWidget(imgLabel);
                        continue;
                    }
                }
            }

            QLabel* textLabel = new QLabel(m_container);
            textLabel->setTextFormat(Qt::RichText);
            textLabel->setWordWrap(true);
            textLabel->setStyleSheet(QString(
                "QLabel {"
                "  color: %1;"
                "  font-family: 'Noto Serif', 'Liberation Serif', 'Georgia', serif;"
                "  font-size: 14pt;"
                "  line-height: 1.6;"
                "  padding: 16px 24px;"
                "}"
            ).arg(m_tokens.textColor.name()));

            QString contentHtml = ch.htmlContent;
            if (contentHtml.isEmpty() || (!contentHtml.contains("<p", Qt::CaseInsensitive) && !contentHtml.contains("<div", Qt::CaseInsensitive))) {
                contentHtml = Typesetter::formatChapterToBookHtml(ch.textContent, ch.title, true);
            }
            textLabel->setText(contentHtml);
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
    const bool themeChanged = (m_tokens.id != tokens.id);
    m_tokens = tokens;
    m_container->setStyleSheet(QString("background-color: %1;").arg(tokens.backgroundColor.name()));
    setStyleSheet(QString("QScrollArea { background-color: %1; border: none; }").arg(tokens.backgroundColor.name()));
    // Scanned pages are baked with the theme tint, so they must be rebuilt.
    if (themeChanged && !m_doc.chapters.isEmpty()) {
        rebuildLayout();
    }
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
