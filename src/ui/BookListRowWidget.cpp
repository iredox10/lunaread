#include "BookListRowWidget.h"
#include "../features/CoverService.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QDateTime>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QLocale>
#include <algorithm>
#include <cmath>

namespace LunaRead {

BookListRowWidget::BookListRowWidget(const BookItem& book, QWidget* parent)
    : QFrame(parent)
    , m_book(book)
{
    setupUi();
}

void BookListRowWidget::setupUi() {
    setObjectName(QStringLiteral("bookListRowWidget"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(76);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);

    setStyleSheet(
        "#bookListRowWidget {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E2E8F0;"
        "  border-radius: 8px;"
        "}"
        "#bookListRowWidget:hover {"
        "  background-color: #F8FAFC;"
        "  border: 1px solid #93C5FD;"
        "}"
    );

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(14, 6, 14, 6);
    mainLayout->setSpacing(14);
    mainLayout->setAlignment(Qt::AlignVCenter);

    // 1. Cover Thumbnail (48x64)
    m_coverLabel = new QLabel(this);
    m_coverLabel->setFixedSize(48, 64);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_coverLabel->setPixmap(generateThumbnail(m_book, 48, 64));
    mainLayout->addWidget(m_coverLabel, 0);

    connect(&CoverService::instance(), &CoverService::coverExtracted, this, [this](const QString& bookId, const QString& coverPath) {
        if (m_book.id == bookId) {
            m_book.coverImagePath = coverPath;
            m_coverLabel->setPixmap(generateThumbnail(m_book, 48, 64));
        }
    });

    // 2. Title & Author Info Column (Stretches)
    auto* infoLayout = new QVBoxLayout();
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(4);
    infoLayout->setAlignment(Qt::AlignVCenter);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("color: #0F172A; font-weight: bold; font-size: 11pt; background: transparent;");
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_titleLabel->setToolTip(m_book.title);
    infoLayout->addWidget(m_titleLabel);

    m_authorLabel = new QLabel(this);
    m_authorLabel->setStyleSheet("color: #64748B; font-size: 9.5pt; background: transparent;");
    m_authorLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_authorLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QString authorStr = m_book.author.isEmpty() ? tr("Unknown Author") : m_book.author;
    m_authorLabel->setToolTip(authorStr);
    infoLayout->addWidget(m_authorLabel);

    mainLayout->addLayout(infoLayout, 1);

    // 3. Format Pill Badge
    m_formatBadge = new QLabel(this);
    m_formatBadge->setFixedSize(54, 22);
    m_formatBadge->setAlignment(Qt::AlignCenter);
    m_formatBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    if (!m_book.format.isEmpty()) {
        m_formatBadge->setText(m_book.format.toUpper());
        m_formatBadge->setStyleSheet(QString(
            "QLabel {"
            "  background-color: %1;"
            "  color: #FFFFFF;"
            "  font-size: 8pt;"
            "  font-weight: bold;"
            "  border-radius: 4px;"
            "}"
        ).arg(formatBadgeColor(m_book.format).name()));
    } else {
        m_formatBadge->hide();
    }
    mainLayout->addWidget(m_formatBadge, 0);

    // 4. Progress Column (Percentage Text + Progress Bar)
    auto* progContainer = new QWidget(this);
    progContainer->setFixedWidth(110);
    progContainer->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* progLayout = new QVBoxLayout(progContainer);
    progLayout->setContentsMargins(0, 0, 0, 0);
    progLayout->setSpacing(4);
    progLayout->setAlignment(Qt::AlignVCenter);

    m_progressText = new QLabel(progContainer);
    m_progressText->setStyleSheet("color: #334155; font-size: 9pt; font-weight: 600; background: transparent;");
    m_progressText->setAlignment(Qt::AlignLeft);
    m_progressText->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_progressText->setText(QString("%1%").arg(static_cast<int>(m_book.progressPercentage)));
    progLayout->addWidget(m_progressText);

    m_progressBar = new QProgressBar(progContainer);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(std::clamp(static_cast<int>(m_book.progressPercentage), 0, 100));
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { background-color: #E2E8F0; border: none; border-radius: 3px; }"
        "QProgressBar::chunk { background-color: #3B82F6; border-radius: 3px; }"
    );
    m_progressBar->setAttribute(Qt::WA_TransparentForMouseEvents);
    progLayout->addWidget(m_progressBar);

    mainLayout->addWidget(progContainer, 0);

    // 5. File Size Column
    m_sizeLabel = new QLabel(this);
    m_sizeLabel->setFixedWidth(75);
    m_sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sizeLabel->setStyleSheet("color: #64748B; font-size: 9pt; font-weight: 500; background: transparent;");
    m_sizeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_sizeLabel->setText(formatFileSize(m_book.fileSize));
    m_sizeLabel->setToolTip(tr("%1 bytes").arg(QLocale().toString(m_book.fileSize)));
    mainLayout->addWidget(m_sizeLabel, 0);

    // 6. Last Read Column
    m_lastReadLabel = new QLabel(this);
    m_lastReadLabel->setFixedWidth(105);
    m_lastReadLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lastReadLabel->setStyleSheet("color: #64748B; font-size: 9pt; background: transparent;");
    m_lastReadLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_lastReadLabel->setText(formatLastRead(m_book.lastReadTime));
    if (m_book.lastReadTime > 0) {
        qint64 ts = m_book.lastReadTime;
        if (ts < 10000000000LL) ts *= 1000;
        m_lastReadLabel->setToolTip(tr("Last read: %1").arg(QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("yyyy-MM-dd hh:mm AP"))));
    } else {
        m_lastReadLabel->setToolTip(tr("Not read yet"));
    }
    mainLayout->addWidget(m_lastReadLabel, 0);

    // 7. Action Buttons (Open, Details, Delete)
    auto* actionsLayout = new QHBoxLayout();
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(6);
    actionsLayout->setAlignment(Qt::AlignVCenter);

    m_openBtn = new QPushButton(tr("Open"), this);
    m_openBtn->setFixedHeight(30);
    m_openBtn->setCursor(Qt::PointingHandCursor);
    m_openBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563EB; color: #FFFFFF; font-weight: 600; font-size: 9pt;"
        "  border: none; border-radius: 6px; padding: 4px 14px;"
        "}"
        "QPushButton:hover { background-color: #1D4ED8; }"
        "QPushButton:pressed { background-color: #1E40AF; }"
    );
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        emit openRequested(m_book.id);
        emit bookClicked(m_book.id);
    });
    actionsLayout->addWidget(m_openBtn);

    m_detailsBtn = new QPushButton(tr("Details"), this);
    m_detailsBtn->setFixedHeight(30);
    m_detailsBtn->setCursor(Qt::PointingHandCursor);
    m_detailsBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #F1F5F9; color: #334155; font-weight: 500; font-size: 9pt;"
        "  border: 1px solid #CBD5E1; border-radius: 6px; padding: 4px 10px;"
        "}"
        "QPushButton:hover { background-color: #E2E8F0; border-color: #94A3B8; color: #0F172A; }"
        "QPushButton:pressed { background-color: #CBD5E1; }"
    );
    connect(m_detailsBtn, &QPushButton::clicked, this, [this]() {
        emit detailsRequested(m_book.id);
    });
    actionsLayout->addWidget(m_detailsBtn);

    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_deleteBtn->setFixedHeight(30);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent; color: #94A3B8; font-weight: 500; font-size: 9pt;"
        "  border: 1px solid transparent; border-radius: 6px; padding: 4px 8px;"
        "}"
        "QPushButton:hover { background-color: #FEE2E2; color: #DC2626; border-color: #FECACA; }"
        "QPushButton:pressed { background-color: #FCA5A5; color: #B91C1C; }"
    );
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(m_book.id);
    });
    actionsLayout->addWidget(m_deleteBtn);

    mainLayout->addLayout(actionsLayout, 0);

    updateElidedLabels();
}

void BookListRowWidget::updateBook(const BookItem& book) {
    m_book = book;
    if (m_coverLabel) {
        m_coverLabel->setPixmap(generateThumbnail(m_book, 48, 64));
    }
    if (m_titleLabel) {
        m_titleLabel->setToolTip(m_book.title);
    }
    if (m_authorLabel) {
        QString authorStr = m_book.author.isEmpty() ? tr("Unknown Author") : m_book.author;
        m_authorLabel->setToolTip(authorStr);
    }
    if (m_formatBadge) {
        if (!m_book.format.isEmpty()) {
            m_formatBadge->setText(m_book.format.toUpper());
            m_formatBadge->setStyleSheet(QString(
                "QLabel {"
                "  background-color: %1;"
                "  color: #FFFFFF;"
                "  font-size: 8pt;"
                "  font-weight: bold;"
                "  border-radius: 4px;"
                "}"
            ).arg(formatBadgeColor(m_book.format).name()));
            m_formatBadge->show();
        } else {
            m_formatBadge->hide();
        }
    }
    if (m_progressText) {
        m_progressText->setText(QString("%1%").arg(static_cast<int>(m_book.progressPercentage)));
    }
    if (m_progressBar) {
        m_progressBar->setValue(std::clamp(static_cast<int>(m_book.progressPercentage), 0, 100));
    }
    if (m_sizeLabel) {
        m_sizeLabel->setText(formatFileSize(m_book.fileSize));
        m_sizeLabel->setToolTip(tr("%1 bytes").arg(QLocale().toString(m_book.fileSize)));
    }
    if (m_lastReadLabel) {
        m_lastReadLabel->setText(formatLastRead(m_book.lastReadTime));
        if (m_book.lastReadTime > 0) {
            qint64 ts = m_book.lastReadTime;
            if (ts < 10000000000LL) ts *= 1000;
            m_lastReadLabel->setToolTip(tr("Last read: %1").arg(QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("yyyy-MM-dd hh:mm AP"))));
        } else {
            m_lastReadLabel->setToolTip(tr("Not read yet"));
        }
    }
    updateElidedLabels();
    update();
}

void BookListRowWidget::updateElidedLabels() {
    if (!m_titleLabel || !m_authorLabel) return;

    int titleWidth = m_titleLabel->width();
    if (titleWidth > 30) {
        QFontMetrics tfm(m_titleLabel->font());
        m_titleLabel->setText(tfm.elidedText(m_book.title, Qt::ElideRight, titleWidth));
    } else {
        m_titleLabel->setText(m_book.title);
    }

    int authorWidth = m_authorLabel->width();
    QString authorStr = m_book.author.isEmpty() ? tr("Unknown Author") : m_book.author;
    if (authorWidth > 30) {
        QFontMetrics afm(m_authorLabel->font());
        m_authorLabel->setText(afm.elidedText(authorStr, Qt::ElideRight, authorWidth));
    } else {
        m_authorLabel->setText(authorStr);
    }
}

void BookListRowWidget::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    updateElidedLabels();
}

void BookListRowWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit bookClicked(m_book.id);
    }
    QFrame::mousePressEvent(event);
}

void BookListRowWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit openRequested(m_book.id);
    }
    QFrame::mouseDoubleClickEvent(event);
}

void BookListRowWidget::contextMenuEvent(QContextMenuEvent* event) {
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
        emit openRequested(m_book.id);
        emit bookClicked(m_book.id);
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

QString BookListRowWidget::formatFileSize(qint64 bytes) {
    if (bytes <= 0) return QStringLiteral("0 B");
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = 1024 * KB;
    constexpr qint64 GB = 1024 * MB;

    if (bytes >= GB) {
        return QStringLiteral("%1 GB").arg(QString::number(static_cast<double>(bytes) / GB, 'f', 1));
    } else if (bytes >= MB) {
        return QStringLiteral("%1 MB").arg(QString::number(static_cast<double>(bytes) / MB, 'f', 1));
    } else if (bytes >= KB) {
        return QStringLiteral("%1 KB").arg(QString::number(static_cast<double>(bytes) / KB, 'f', 0));
    } else {
        return QStringLiteral("%1 B").arg(bytes);
    }
}

QString BookListRowWidget::formatLastRead(qint64 timestamp) {
    if (timestamp <= 0) {
        return tr("Never");
    }
    if (timestamp < 10000000000LL) {
        timestamp *= 1000;
    }
    const QDateTime readDt = QDateTime::fromMSecsSinceEpoch(timestamp);
    const QDateTime now = QDateTime::currentDateTime();
    const QDate readDate = readDt.date();
    const QDate today = now.date();

    if (readDate == today) {
        return tr("Today");
    } else if (readDate == today.addDays(-1)) {
        return tr("Yesterday");
    } else {
        return readDate.toString(QStringLiteral("MMM d, yyyy"));
    }
}

QColor BookListRowWidget::formatBadgeColor(const QString& format) {
    const QString f = format.toLower();
    if (f == "epub") return QColor(16, 185, 129);       // Emerald green
    if (f == "pdf") return QColor(239, 68, 68);         // Crimson red
    if (f == "mobi" || f == "azw" || f == "azw3") return QColor(245, 158, 11); // Amber
    if (f == "fb2") return QColor(139, 92, 246);        // Purple
    if (f == "djvu") return QColor(14, 165, 233);       // Cyan
    if (f == "cbz" || f == "cbr") return QColor(236, 72, 153); // Pink
    if (f == "docx" || f == "odt") return QColor(59, 130, 246); // Blue
    if (f == "txt" || f == "md") return QColor(107, 114, 128); // Slate
    return QColor(99, 102, 241);                        // Indigo
}

QPixmap BookListRowWidget::generateThumbnail(const BookItem& book, int width, int height) {
    QPixmap pix(width, height);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath clipPath;
    clipPath.addRoundedRect(QRectF(0, 0, width, height), 4.0, 4.0);
    p.setClipPath(clipPath);

    bool coverLoaded = false;
    QString coverPath = book.coverImagePath;
    if (coverPath.isEmpty() || !QFile::exists(coverPath)) {
        coverPath = CoverService::instance().getCoverPath(book);
    }
    if (!coverPath.isEmpty() && QFile::exists(coverPath)) {
        QPixmap loadedCover;
        if (loadedCover.load(coverPath) && !loadedCover.isNull()) {
            QPixmap scaled = loadedCover.scaled(width, height, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int x = (width - scaled.width()) / 2;
            int y = (height - scaled.height()) / 2;
            p.drawPixmap(x, y, scaled);
            coverLoaded = true;
        }
    }

    if (!coverLoaded) {
        // Procedural gradient cover fallback based on title hash
        uint hash = qHash(book.title.isEmpty() ? book.id : book.title);
        int hue = static_cast<int>(hash % 360);
        QColor c1 = QColor::fromHsv(hue, 150, 75);
        QColor c2 = QColor::fromHsv((hue + 40) % 360, 180, 45);

        QLinearGradient grad(0, 0, width, height);
        grad.setColorAt(0.0, c1);
        grad.setColorAt(1.0, c2);
        p.fillPath(clipPath, grad);

        // Spine crease line
        p.setPen(QColor(0, 0, 0, 50));
        p.drawLine(4, 0, 4, height);
        p.setPen(QColor(255, 255, 255, 40));
        p.drawLine(5, 0, 5, height);

        // Centered format badge text or book icon
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(10);
        p.setFont(f);
        QRectF textRect(6, 6, width - 10, height - 12);
        if (!book.format.isEmpty()) {
            p.drawText(textRect, Qt::AlignCenter, book.format.toUpper());
        } else {
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("📖"));
        }
    }

    // Subtle outer border
    p.setClipping(false);
    p.setPen(QPen(QColor(0, 0, 0, 25), 1.0));
    p.drawRoundedRect(QRectF(0.5, 0.5, width - 1.0, height - 1.0), 4.0, 4.0);

    return pix;
}

} // namespace LunaRead
