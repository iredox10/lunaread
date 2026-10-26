#include "ReaderViewport.h"
#include "../ui/TocDrawerWidget.h"
#include "../ui/TtsControlBar.h"
#include "../ui/SearchInBookDialog.h"
#include "../ui/FontSettingsDialog.h"
#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>

namespace LunaRead {

ReaderViewport::ReaderViewport(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    m_tokens = ReaderThemeTokens::parchment();

    m_gestures = new GestureController(this);
    m_shortcuts = new DesktopShortcuts(this);
    installEventFilter(m_shortcuts);

    m_scrollRenderer = new ContinuousScrollRenderer(this);
    m_scrollRenderer->hide();

    m_dualPageRenderer = new DualPageRenderer(this);
    m_dualPageRenderer->hide();

    m_curlAnimation = new QVariantAnimation(this);
    m_curlAnimation->setDuration(320);
    connect(m_curlAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_curlProgress = val.toReal();
        update();
    });

    // 1. Build Header Toolbar
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(48);
    auto* hLayout = new QHBoxLayout(m_headerBar);
    hLayout->setContentsMargins(16, 0, 16, 0);
    hLayout->setSpacing(8);

    auto* backBtn = new QPushButton(tr("← Shelf"), m_headerBar);
    connect(backBtn, &QPushButton::clicked, this, &ReaderViewport::closeRequested);
    hLayout->addWidget(backBtn);

    auto* tocBtn = new QPushButton(tr("☰ Contents"), m_headerBar);
    connect(tocBtn, &QPushButton::clicked, this, &ReaderViewport::openToc);
    hLayout->addWidget(tocBtn);

    auto* searchBtn = new QPushButton(tr("🔍 Search"), m_headerBar);
    connect(searchBtn, &QPushButton::clicked, this, &ReaderViewport::openSearch);
    hLayout->addWidget(searchBtn);

    m_titleLabel = new QLabel(m_headerBar);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    hLayout->addWidget(m_titleLabel, 1);

    auto* fontBtn = new QPushButton(tr("Aa Typography"), m_headerBar);
    connect(fontBtn, &QPushButton::clicked, this, &ReaderViewport::openFontSettings);
    hLayout->addWidget(fontBtn);

    auto* ttsBtn = new QPushButton(tr("🔊 TTS"), m_headerBar);
    connect(ttsBtn, &QPushButton::clicked, this, &ReaderViewport::toggleTts);
    hLayout->addWidget(ttsBtn);

    auto* themeBtn = new QPushButton(tr("🎨 Theme"), m_headerBar);
    connect(themeBtn, &QPushButton::clicked, this, &ReaderViewport::toggleTheme);
    hLayout->addWidget(themeBtn);

    m_headerBar->hide();

    // 2. Build Footer Toolbar
    m_footerBar = new QWidget(this);
    m_footerBar->setFixedHeight(50);
    auto* fLayout = new QHBoxLayout(m_footerBar);
    fLayout->setContentsMargins(16, 0, 16, 0);

    m_progressSlider = new QSlider(Qt::Horizontal, m_footerBar);
    m_progressSlider->setRange(0, 100);
    connect(m_progressSlider, &QSlider::sliderMoved, this, [this](int value) {
        if (m_doc.totalChapters > 0) {
            int targetCh = (value * m_doc.totalChapters) / 100;
            jumpToChapter(std::clamp(targetCh, 0, m_doc.totalChapters - 1));
        }
    });
    fLayout->addWidget(m_progressSlider, 1);

    m_pageInfoLabel = new QLabel(m_footerBar);
    fLayout->addWidget(m_pageInfoLabel);
    m_footerBar->hide();

    // 3. Navigation Drawer & TTS HUD
    m_tocDrawer = new TocDrawerWidget(this);
    m_tocDrawer->hide();
    connect(m_tocDrawer, &TocDrawerWidget::chapterSelected, this, &ReaderViewport::jumpToChapter);
    connect(m_tocDrawer, &TocDrawerWidget::pageSelected, this, &ReaderViewport::jumpToPage);

    m_ttsService = new TtsService(this);
    m_ttsBar = new TtsControlBar(m_ttsService, this);
    m_ttsBar->hide();
    connect(m_ttsBar, &TtsControlBar::sentenceHighlighted, this, [this](int /*index*/, const QString& sentence) {
        m_activeTtsSentence = sentence;
        update();
    });
    connect(m_ttsBar, &TtsControlBar::closeRequested, this, [this]() {
        m_ttsBar->hide();
    });

    // 4. Connect shortcuts
    connect(m_shortcuts, &DesktopShortcuts::nextPageRequested, this, &ReaderViewport::nextPage);
    connect(m_shortcuts, &DesktopShortcuts::prevPageRequested, this, &ReaderViewport::prevPage);
    connect(m_shortcuts, &DesktopShortcuts::toggleThemeRequested, this, &ReaderViewport::toggleTheme);
    connect(m_shortcuts, &DesktopShortcuts::searchRequested, this, &ReaderViewport::openSearch);
    connect(m_shortcuts, &DesktopShortcuts::openTocRequested, this, &ReaderViewport::openToc);
    connect(m_shortcuts, &DesktopShortcuts::fontSettingsRequested, this, &ReaderViewport::openFontSettings);
    connect(m_shortcuts, &DesktopShortcuts::toggleTtsRequested, this, &ReaderViewport::toggleTts);
    connect(m_shortcuts, &DesktopShortcuts::escapeRequested, this, [this]() {
        if (m_tocDrawer->isOpen()) m_tocDrawer->closeDrawer();
        else if (m_isToolbarVisible) toggleToolbar();
        else emit closeRequested();
    });

    updateHudStyle();
}

void ReaderViewport::loadDocument(const BookDocument& doc) {
    m_doc = doc;
    m_currentChapter = 0;
    m_currentPageInChapter = 0;
    m_titleLabel->setText(doc.title);
    m_tocDrawer->setToc(doc.toc);

    // Load TTS text
    QString fullText;
    for (const auto& ch : doc.chapters) {
        fullText.append(ch.textContent).append("\n\n");
    }
    m_ttsService->loadText(fullText);

    m_scrollRenderer->setDocument(doc, m_tokens);
    repaginate();
}

void ReaderViewport::setReadingMode(ReadingMode mode) {
    m_readingMode = mode;
    m_scrollRenderer->setVisible(mode == ReadingMode::ContinuousScroll);
    m_dualPageRenderer->setVisible(mode == ReadingMode::DualPage);
    repaginate();
    update();
}

void ReaderViewport::setThemeMode(ReaderThemeMode theme) {
    m_tokens = ReaderThemeTokens::fromMode(theme);
    m_scrollRenderer->updateTheme(m_tokens);
    m_dualPageRenderer->updateTheme(m_tokens);
    m_tocDrawer->applyTheme(m_tokens);
    m_ttsBar->updateTheme(m_tokens);
    updateHudStyle();
    update();
}

void ReaderViewport::toggleTheme() {
    if (m_tokens.id == "parchment") setThemeMode(ReaderThemeMode::OledBlack);
    else if (m_tokens.id == "oledBlack") setThemeMode(ReaderThemeMode::Sepia);
    else if (m_tokens.id == "sepia") setThemeMode(ReaderThemeMode::Night);
    else if (m_tokens.id == "night") setThemeMode(ReaderThemeMode::ModernLight);
    else setThemeMode(ReaderThemeMode::Parchment);
}

void ReaderViewport::openSearch() {
    auto* dlg = new SearchInBookDialog(m_doc, this);
    dlg->applyTheme(m_tokens);
    connect(dlg, &SearchInBookDialog::matchSelected, this, [this](int chIdx, int /*offset*/, int /*len*/) {
        jumpToChapter(chIdx);
    });
    dlg->show();
}

void ReaderViewport::openToc() {
    m_tocDrawer->setCurrentReadingPosition(m_currentChapter, m_currentPageInChapter);
    m_tocDrawer->toggleDrawer();
}

void ReaderViewport::openFontSettings() {
    auto* dlg = new FontSettingsDialog(this);
    connect(dlg, &FontSettingsDialog::fontChanged, this, [this](const QFont& f) {
        m_typesetter.setFont(f);
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::lineHeightChanged, this, [this](qreal lh) {
        m_typesetter.setLineHeight(lh);
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::marginsChanged, this, [this](qreal h, qreal v) {
        m_typesetter.setMargins(QMarginsF(h, v, h, v));
        repaginate();
    });
    connect(dlg, &FontSettingsDialog::themeChanged, this, &ReaderViewport::setThemeMode);
    dlg->show();
}

void ReaderViewport::toggleTts() {
    if (m_ttsBar->isVisible()) {
        m_ttsBar->hide();
    } else {
        m_ttsBar->show();
        m_ttsBar->reposition(size());
        m_ttsBar->play();
    }
}

void ReaderViewport::jumpToChapter(int chapterIndex) {
    if (chapterIndex >= 0 && chapterIndex < m_doc.chapters.size()) {
        m_currentChapter = chapterIndex;
        m_currentPageInChapter = 0;
        repaginate();
    }
}

void ReaderViewport::jumpToPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < m_pages.size()) {
        m_currentPageInChapter = pageIndex;
        update();
    }
}

void ReaderViewport::updateHudStyle() {
    QString barStyle = QString("background-color: %1; border: 1px solid %2; border-radius: 8px; color: %3;")
        .arg(m_tokens.backgroundColor.name())
        .arg(m_tokens.dividerColor.name())
        .arg(m_tokens.textColor.name());

    m_headerBar->setStyleSheet(barStyle);
    m_footerBar->setStyleSheet(barStyle);
}

void ReaderViewport::repaginate() {
    if (m_doc.chapters.isEmpty()) return;

    const auto& ch = m_doc.chapters[m_currentChapter];
    QRectF pageBounds = rect().adjusted(32, 48, -32, -48);
    m_pages = m_typesetter.paginate(ch, pageBounds.size(), m_currentChapter);

    if (m_pages.isEmpty()) {
        ParsedPage emptyPage;
        emptyPage.chapterIndex = m_currentChapter;
        emptyPage.text = ch.textContent;
        m_pages.append(emptyPage);
    }

    m_currentPageInChapter = std::clamp(m_currentPageInChapter, 0, static_cast<int>(m_pages.size()) - 1);

    // Update footer info
    QString info = QString("Ch %1/%2 • Page %3/%4")
        .arg(m_currentChapter + 1)
        .arg(m_doc.totalChapters)
        .arg(m_currentPageInChapter + 1)
        .arg(m_pages.size());
    m_pageInfoLabel->setText(info);

    double progress = 0.0;
    if (m_doc.totalChapters > 0) {
        progress = (static_cast<double>(m_currentChapter) / m_doc.totalChapters) * 100.0;
    }
    m_progressSlider->setValue(static_cast<int>(progress));
    emit progressUpdated(progress);

    if (m_readingMode == ReadingMode::DualPage && !m_pages.isEmpty()) {
        std::optional<ParsedPage> rightPage;
        if (m_currentPageInChapter + 1 < m_pages.size()) {
            rightPage = m_pages[m_currentPageInChapter + 1];
        }
        m_dualPageRenderer->setPages(m_pages[m_currentPageInChapter], rightPage, m_tokens);
    }

    update();
}

void ReaderViewport::nextPage() {
    if (m_currentPageInChapter + 1 < m_pages.size()) {
        m_currentPageInChapter++;
        update();
    } else if (m_currentChapter + 1 < m_doc.chapters.size()) {
        m_currentChapter++;
        m_currentPageInChapter = 0;
        repaginate();
    }
}

void ReaderViewport::prevPage() {
    if (m_currentPageInChapter > 0) {
        m_currentPageInChapter--;
        update();
    } else if (m_currentChapter > 0) {
        m_currentChapter--;
        m_currentPageInChapter = 999999;
        repaginate();
    }
}

void ReaderViewport::toggleToolbar() {
    m_isToolbarVisible = !m_isToolbarVisible;
    m_headerBar->setVisible(m_isToolbarVisible);
    m_footerBar->setVisible(m_isToolbarVisible);
}

void ReaderViewport::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_pages.isEmpty()) {
        painter.fillRect(rect(), m_tokens.backgroundColor);
        painter.setPen(m_tokens.textColor);
        painter.setFont(QFont("Roboto", 14));
        painter.drawText(rect(), Qt::AlignCenter, tr("No readable content found in this section.\nClick '← Shelf' above to return to your library."));
        return;
    }

    const ParsedPage& curPage = m_pages[m_currentPageInChapter];
    std::optional<ParsedPage> nextPage;
    if (m_currentPageInChapter + 1 < m_pages.size()) {
        nextPage = m_pages[m_currentPageInChapter + 1];
    }

    QRectF bounds = rect();
    PageCurlRenderer::paint(
        &painter, bounds, curPage, nextPage, m_curlProgress,
        QPointF(bounds.width(), bounds.height()), m_dragPos,
        m_tokens, m_readingMode == ReadingMode::DualPage,
        m_activeTtsSentence
    );
}

void ReaderViewport::resizeEvent(QResizeEvent* /*event*/) {
    m_scrollRenderer->setGeometry(rect());
    m_dualPageRenderer->setGeometry(rect());
    m_headerBar->setGeometry(16, 16, width() - 32, 48);
    m_footerBar->setGeometry(16, height() - 66, width() - 32, 50);
    m_tocDrawer->setGeometry(0, 0, m_tocDrawer->width(), height());
    m_ttsBar->reposition(size());
    m_headerBar->raise();
    m_footerBar->raise();
    m_tocDrawer->raise();
    m_ttsBar->raise();
    repaginate();
}

void ReaderViewport::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
    m_scrollRenderer->setGeometry(rect());
    m_dualPageRenderer->setGeometry(rect());
    m_headerBar->setGeometry(16, 16, width() - 32, 48);
    m_footerBar->setGeometry(16, height() - 66, width() - 32, 50);
    m_tocDrawer->setGeometry(0, 0, m_tocDrawer->width(), height());
    m_ttsBar->reposition(size());
    m_headerBar->raise();
    m_footerBar->raise();
    m_tocDrawer->raise();
    m_ttsBar->raise();
    repaginate();
}

void ReaderViewport::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->position();
        m_dragPos = event->position();
        m_isDragging = false;
        m_curlProgress = 0.0;
    }
}

void ReaderViewport::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        qreal dist = (event->position() - m_pressPos).manhattanLength();
        if (dist > 8.0) {
            m_isDragging = true;
            m_dragPos = event->position();
            qreal progress = 1.0 - (m_dragPos.x() / static_cast<qreal>(width()));
            m_curlProgress = std::clamp(progress, 0.0, 1.0);
            update();
        }
    }
}

void ReaderViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (!m_isDragging) {
            // TAP / CLICK: Moon+ Reader 9-Zone Touch Matrix
            int zoneX = static_cast<int>(event->position().x() / (width() / 3.0));
            int zoneY = static_cast<int>(event->position().y() / (height() / 3.0));
            zoneX = std::clamp(zoneX, 0, 2);
            zoneY = std::clamp(zoneY, 0, 2);
            int zone = zoneY * 3 + zoneX + 1; // 1 to 9

            if (zone == 5) {
                // Zone 5 (Center): Toggle toolbars
                toggleToolbar();
            } else if (zone == 2 || zone == 8) {
                // Top-Center or Bottom-Center: toggle toolbar
                toggleToolbar();
            } else if (zoneX == 0) {
                // Left 1/3 (Zones 1, 4, 7): Previous Page
                prevPage();
            } else if (zoneX == 2) {
                // Right 1/3 (Zones 3, 6, 9): Next Page
                nextPage();
            }
        } else {
            // Drag release
            m_isDragging = false;
            if (m_curlProgress > 0.25) {
                nextPage();
            }
            m_curlProgress = 0.0;
            update();
        }
    }
}

void ReaderViewport::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
        case Qt::Key_Space:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            nextPage();
            event->accept();
            break;
        case Qt::Key_Left:
        case Qt::Key_Up:
        case Qt::Key_PageUp:
        case Qt::Key_Backspace:
            prevPage();
            event->accept();
            break;
        case Qt::Key_Home:
            jumpToChapter(0);
            jumpToPage(0);
            event->accept();
            break;
        case Qt::Key_End:
            if (!m_doc.chapters.isEmpty()) {
                jumpToChapter(static_cast<int>(m_doc.chapters.size()) - 1);
            }
            event->accept();
            break;
        case Qt::Key_Escape:
            emit closeRequested();
            event->accept();
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void ReaderViewport::wheelEvent(QWheelEvent* event) {
    if (m_readingMode == ReadingMode::ContinuousScroll) {
        QWidget::wheelEvent(event);
        return;
    }
    if (event->angleDelta().y() < 0) {
        nextPage();
    } else if (event->angleDelta().y() > 0) {
        prevPage();
    }
    event->accept();
}

} // namespace LunaRead
