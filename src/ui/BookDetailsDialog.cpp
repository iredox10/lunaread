#include "BookDetailsDialog.h"
#include "../db/DatabaseManager.h"
#include "../utils/HashUtils.h"
#include "../features/MetadataFetcherService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QFontDatabase>

#include <cmath>
#include <algorithm>

namespace LunaRead {

// =========================================================================
// StarRatingWidget Implementation
// =========================================================================

StarRatingWidget::StarRatingWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void StarRatingWidget::setRating(double rating) {
    const double clamped = std::clamp(rating, 0.0, static_cast<double>(m_maxStars));
    if (std::abs(m_rating - clamped) > 0.001) {
        m_rating = clamped;
        emit ratingChanged(m_rating);
        update();
    }
}

void StarRatingWidget::setMaxStars(int maxStars) {
    if (maxStars > 0 && maxStars != m_maxStars) {
        m_maxStars = maxStars;
        updateGeometry();
        update();
    }
}

void StarRatingWidget::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    update();
}

QSize StarRatingWidget::sizeHint() const {
    const int totalWidth = static_cast<int>(m_maxStars * (m_starSize + m_starSpacing) + 90);
    const int totalHeight = static_cast<int>(m_starSize + 8);
    return QSize(totalWidth, totalHeight);
}

QSize StarRatingWidget::minimumSizeHint() const {
    const int totalWidth = static_cast<int>(m_maxStars * (m_starSize + m_starSpacing) + 70);
    const int totalHeight = static_cast<int>(m_starSize + 6);
    return QSize(totalWidth, totalHeight);
}

QPolygonF StarRatingWidget::starPolygon(const QPointF& center, qreal rOuter, qreal rInner) {
    QPolygonF poly;
    for (int i = 0; i < 10; ++i) {
        const qreal angle = -M_PI / 2.0 + (i * M_PI / 5.0);
        const qreal r = (i % 2 == 0) ? rOuter : rInner;
        poly << QPointF(center.x() + r * std::cos(angle), center.y() + r * std::sin(angle));
    }
    return poly;
}

int StarRatingWidget::starAtPosition(const QPointF& pos) const {
    const qreal margin = 4.0;
    if (pos.x() < margin) return 0;

    const qreal starStep = m_starSize + m_starSpacing;
    const int idx = static_cast<int>((pos.x() - margin) / starStep) + 1;
    if (idx < 1 || idx > m_maxStars) return 0;
    return idx;
}

void StarRatingWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_readOnly) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int star = starAtPosition(event->position());
    if (star != m_hoverRating) {
        m_hoverRating = star;
        if (m_hoverRating > 0) {
            setToolTip(QString(tr("%1 star%2")).arg(m_hoverRating).arg(m_hoverRating > 1 ? tr("s") : ""));
        } else {
            setToolTip(QString());
        }
        update();
    }
}

void StarRatingWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    if (m_hoverRating != -1) {
        m_hoverRating = -1;
        setToolTip(QString());
        update();
    }
}

void StarRatingWidget::mousePressEvent(QMouseEvent* event) {
    if (m_readOnly) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const int star = starAtPosition(event->position());
        if (star >= 1 && star <= m_maxStars) {
            // Clicking current rating resets to 0, otherwise sets to clicked star
            if (std::abs(m_rating - static_cast<double>(star)) < 0.1) {
                setRating(0.0);
            } else {
                setRating(static_cast<double>(star));
            }
        }
    }
}

void StarRatingWidget::keyPressEvent(QKeyEvent* event) {
    if (m_readOnly) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Down) {
        setRating(std::max(0.0, m_rating - 1.0));
    } else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Up) {
        setRating(std::min(static_cast<double>(m_maxStars), m_rating + 1.0));
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        setRating(0.0);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void StarRatingWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal margin = 4.0;
    const qreal cy = height() / 2.0;
    const qreal rOuter = m_starSize / 2.0;
    const qreal rInner = rOuter * 0.42;

    const int activeThreshold = (m_hoverRating > 0)
                                    ? m_hoverRating
                                    : static_cast<int>(std::round(m_rating));

    for (int i = 1; i <= m_maxStars; ++i) {
        const qreal cx = margin + (i - 1) * (m_starSize + m_starSpacing) + rOuter;
        const QPointF center(cx, cy);
        const QPolygonF star = starPolygon(center, rOuter, rInner);

        if (i <= activeThreshold) {
            p.setBrush(QColor(245, 158, 11)); // Amber 500
            p.setPen(QPen(QColor(217, 119, 6), 1.2)); // Amber 600
        } else {
            p.setBrush(QColor(241, 245, 249)); // Slate 100
            p.setPen(QPen(QColor(203, 213, 225), 1.0)); // Slate 300
        }
        p.drawPolygon(star);
    }

    // Text label next to stars
    const qreal textX = margin + m_maxStars * (m_starSize + m_starSpacing) + 8.0;
    QFont f = font();
    f.setPointSize(9);
    p.setFont(f);

    const QRectF labelRect(textX, 0, width() - textX, height());
    if (m_rating <= 0.0) {
        p.setPen(QColor(100, 116, 139)); // Slate 500
        p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, tr("(Unrated)"));
    } else {
        p.setPen(QColor(30, 41, 59)); // Slate 800
        const QString ratingStr = QString("%1 / %2")
                                      .arg(QString::number(m_rating, 'f', 1))
                                      .arg(m_maxStars);
        p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, ratingStr);
    }
}

// =========================================================================
// BookDetailsDialog Implementation
// =========================================================================

BookDetailsDialog::BookDetailsDialog(const BookItem& book, QWidget* parent)
    : QDialog(parent), m_book(book) {
    // Populate missing technical fields from disk file if available
    if (m_book.fileSize <= 0 && !m_book.filePath.isEmpty() && QFile::exists(m_book.filePath)) {
        m_book.fileSize = QFileInfo(m_book.filePath).size();
    }
    if (m_book.fileHash.isEmpty() && !m_book.filePath.isEmpty() && QFile::exists(m_book.filePath)) {
        m_book.fileHash = HashUtils::calculateFileMd5(m_book.filePath);
    }
    if (m_book.format.isEmpty() && !m_book.filePath.isEmpty()) {
        m_book.format = QFileInfo(m_book.filePath).suffix().toLower();
    }

    setWindowTitle(tr("Book Details & Metadata - %1").arg(m_book.title.isEmpty() ? tr("Untitled") : m_book.title));
    resize(720, 580);
    setMinimumSize(660, 520);

    setupUi();
    applyStyles();
    updateCoverPreview();
}

void BookDetailsDialog::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(16);

    // Main 2-column layout
    auto* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(20);

    // ---------------------------------------------------------------------
    // LEFT COLUMN: Cover preview & Reading progress
    // ---------------------------------------------------------------------
    auto* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(12);

    // Cover image preview container
    m_coverLabel = new QLabel(this);
    m_coverLabel->setFixedSize(140, 200);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setStyleSheet("QLabel { background-color: #F1F5F9; border: 1px solid #CBD5E1; border-radius: 8px; }");
    leftLayout->addWidget(m_coverLabel, 0, Qt::AlignHCenter);

    // "Change Cover..." button
    m_changeCoverBtn = new QPushButton(tr("Change Cover..."), this);
    m_changeCoverBtn->setFixedHeight(28);
    m_changeCoverBtn->setCursor(Qt::PointingHandCursor);
    m_changeCoverBtn->setToolTip(tr("Select a custom cover image for this book"));
    connect(m_changeCoverBtn, &QPushButton::clicked, this, &BookDetailsDialog::onChangeCover);
    leftLayout->addWidget(m_changeCoverBtn);

    // "Fetch Online Metadata..." button
    m_fetchOnlineBtn = new QPushButton(tr("🌐 Fetch Online Metadata"), this);
    m_fetchOnlineBtn->setFixedHeight(28);
    m_fetchOnlineBtn->setCursor(Qt::PointingHandCursor);
    m_fetchOnlineBtn->setToolTip(tr("Lookup title, author, and cover on OpenLibrary"));
    connect(m_fetchOnlineBtn, &QPushButton::clicked, this, &BookDetailsDialog::onFetchOnlineMetadata);
    leftLayout->addWidget(m_fetchOnlineBtn);

    // Reading progress box
    auto* progressGroup = new QGroupBox(tr("Reading Progress"), this);
    auto* progressLayout = new QVBoxLayout(progressGroup);
    progressLayout->setContentsMargins(10, 12, 10, 10);
    progressLayout->setSpacing(6);

    m_progressBar = new QProgressBar(progressGroup);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    const int progVal = static_cast<int>(std::clamp(m_book.progressPercentage, 0.0, 100.0));
    m_progressBar->setValue(progVal);
    progressLayout->addWidget(m_progressBar);

    m_progressTextLabel = new QLabel(QString("%1% Read").arg(QString::number(m_book.progressPercentage, 'f', 1)), progressGroup);
    QFont progFont = font();
    progFont.setBold(true);
    progFont.setPointSize(10);
    m_progressTextLabel->setFont(progFont);
    m_progressTextLabel->setStyleSheet("color: #1E293B;");
    progressLayout->addWidget(m_progressTextLabel);

    // Chapter and page readout
    QString chapterPageText;
    if (m_book.totalChapters > 0 && m_book.totalPages > 0) {
        chapterPageText = tr("Ch. %1 of %2  •  Pg. %3 of %4")
                              .arg(m_book.currentChapterIndex + 1)
                              .arg(m_book.totalChapters)
                              .arg(m_book.currentPageIndex + 1)
                              .arg(m_book.totalPages);
    } else if (m_book.totalChapters > 0) {
        chapterPageText = tr("Chapter %1 of %2").arg(m_book.currentChapterIndex + 1).arg(m_book.totalChapters);
    } else if (m_book.totalPages > 0) {
        chapterPageText = tr("Page %1 of %2").arg(m_book.currentPageIndex + 1).arg(m_book.totalPages);
    } else {
        chapterPageText = tr("Position: Beginning");
    }
    m_chapterPageLabel = new QLabel(chapterPageText, progressGroup);
    m_chapterPageLabel->setStyleSheet("color: #64748B; font-size: 8.5pt;");
    progressLayout->addWidget(m_chapterPageLabel);

    // Last read timestamp
    auto* lastReadHeader = new QLabel(tr("Last read:"), progressGroup);
    lastReadHeader->setStyleSheet("color: #94A3B8; font-size: 8pt; margin-top: 4px;");
    progressLayout->addWidget(lastReadHeader);

    m_lastReadLabel = new QLabel(formatTimestamp(m_book.lastReadTime), progressGroup);
    m_lastReadLabel->setStyleSheet("color: #334155; font-size: 8.5pt; font-weight: 500;");
    progressLayout->addWidget(m_lastReadLabel);

    // Date added
    auto* addedHeader = new QLabel(tr("Added to library:"), progressGroup);
    addedHeader->setStyleSheet("color: #94A3B8; font-size: 8pt; margin-top: 2px;");
    progressLayout->addWidget(addedHeader);

    m_dateAddedLabel = new QLabel(formatTimestamp(m_book.dateAdded), progressGroup);
    m_dateAddedLabel->setStyleSheet("color: #334155; font-size: 8.5pt;");
    progressLayout->addWidget(m_dateAddedLabel);

    leftLayout->addWidget(progressGroup);
    leftLayout->addStretch(1);

    contentLayout->addLayout(leftLayout, 0);

    // ---------------------------------------------------------------------
    // RIGHT COLUMN: Editable Metadata & Read-only Technical Info
    // ---------------------------------------------------------------------
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(14);

    // 1. Group: Book Metadata (Editable)
    auto* metaGroup = new QGroupBox(tr("Book Metadata"), this);
    auto* metaForm = new QFormLayout(metaGroup);
    metaForm->setContentsMargins(14, 16, 14, 14);
    metaForm->setSpacing(10);
    metaForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Title
    m_titleEdit = new QLineEdit(m_book.title, metaGroup);
    m_titleEdit->setPlaceholderText(tr("Book title..."));
    m_titleEdit->setFixedHeight(32);
    metaForm->addRow(tr("Title:"), m_titleEdit);

    // Author
    m_authorEdit = new QLineEdit(m_book.author, metaGroup);
    m_authorEdit->setPlaceholderText(tr("Author or creator..."));
    m_authorEdit->setFixedHeight(32);
    metaForm->addRow(tr("Author:"), m_authorEdit);

    // Series and Series Index
    auto* seriesRowLayout = new QHBoxLayout();
    seriesRowLayout->setSpacing(8);

    m_seriesEdit = new QLineEdit(m_book.series, metaGroup);
    m_seriesEdit->setPlaceholderText(tr("Series name (optional)..."));
    m_seriesEdit->setFixedHeight(32);
    seriesRowLayout->addWidget(m_seriesEdit, 3);

    auto* indexLabel = new QLabel(tr("Index:"), metaGroup);
    seriesRowLayout->addWidget(indexLabel);

    m_seriesIndexSpin = new QDoubleSpinBox(metaGroup);
    m_seriesIndexSpin->setRange(0.0, 9999.0);
    m_seriesIndexSpin->setDecimals(1);
    m_seriesIndexSpin->setSingleStep(1.0);
    m_seriesIndexSpin->setValue(m_book.seriesIndex);
    m_seriesIndexSpin->setFixedHeight(32);
    m_seriesIndexSpin->setFixedWidth(80);
    seriesRowLayout->addWidget(m_seriesIndexSpin, 1);

    metaForm->addRow(tr("Series:"), seriesRowLayout);

    // Rating (Interactive Star Widget)
    auto* ratingRowLayout = new QHBoxLayout();
    ratingRowLayout->setSpacing(8);

    m_starRating = new StarRatingWidget(metaGroup);
    m_starRating->setRating(m_book.rating);
    ratingRowLayout->addWidget(m_starRating);

    m_clearRatingBtn = new QToolButton(metaGroup);
    m_clearRatingBtn->setText(QStringLiteral("✕"));
    m_clearRatingBtn->setToolTip(tr("Clear rating"));
    m_clearRatingBtn->setFixedSize(24, 24);
    m_clearRatingBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearRatingBtn, &QToolButton::clicked, this, &BookDetailsDialog::onClearRating);
    ratingRowLayout->addWidget(m_clearRatingBtn);
    ratingRowLayout->addStretch(1);

    metaForm->addRow(tr("Rating:"), ratingRowLayout);

    // Favorite status (QCheckBox)
    m_favoriteCheck = new QCheckBox(tr("Mark as Favorite (starred in library)"), metaGroup);
    m_favoriteCheck->setChecked(m_book.isFavorite);
    m_favoriteCheck->setCursor(Qt::PointingHandCursor);
    metaForm->addRow(QString(), m_favoriteCheck);

    rightLayout->addWidget(metaGroup);

    // 2. Group: Technical & Storage Information (Read-only)
    auto* techGroup = new QGroupBox(tr("Technical & Storage Information"), this);
    auto* techForm = new QFormLayout(techGroup);
    techForm->setContentsMargins(14, 16, 14, 14);
    techForm->setSpacing(10);
    techForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Format & File Size row
    auto* formatSizeLayout = new QHBoxLayout();
    formatSizeLayout->setSpacing(12);

    m_formatBadgeLabel = new QLabel(techGroup);
    const QString fmtUpper = m_book.format.isEmpty() ? tr("UNKNOWN") : m_book.format.toUpper();
    m_formatBadgeLabel->setText(fmtUpper);
    const QColor badgeCol = formatBadgeColor(m_book.format);
    m_formatBadgeLabel->setStyleSheet(QString(
        "QLabel { background-color: %1; color: white; font-weight: bold; font-size: 8.5pt; "
        "border-radius: 4px; padding: 3px 8px; }").arg(badgeCol.name()));
    formatSizeLayout->addWidget(m_formatBadgeLabel);

    auto* sizePrefixLabel = new QLabel(tr("Size:"), techGroup);
    sizePrefixLabel->setStyleSheet("color: #64748B; font-size: 9pt;");
    formatSizeLayout->addWidget(sizePrefixLabel);

    m_fileSizeLabel = new QLabel(formatFileSize(m_book.fileSize), techGroup);
    m_fileSizeLabel->setStyleSheet("color: #1E293B; font-weight: 500; font-size: 9pt;");
    formatSizeLayout->addWidget(m_fileSizeLabel);
    formatSizeLayout->addStretch(1);

    techForm->addRow(tr("Format:"), formatSizeLayout);

    // MD5 Hash row
    auto* hashRowLayout = new QHBoxLayout();
    hashRowLayout->setSpacing(8);

    m_hashEdit = new QLineEdit(m_book.fileHash.isEmpty() ? tr("(Not calculated)") : m_book.fileHash, techGroup);
    m_hashEdit->setReadOnly(true);
    m_hashEdit->setFixedHeight(30);
    m_hashEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_hashEdit->setStyleSheet("QLineEdit { background-color: #F8FAFC; color: #334155; }");
    hashRowLayout->addWidget(m_hashEdit, 1);

    m_copyHashBtn = new QPushButton(tr("Copy"), techGroup);
    m_copyHashBtn->setFixedHeight(30);
    m_copyHashBtn->setFixedWidth(65);
    m_copyHashBtn->setCursor(Qt::PointingHandCursor);
    m_copyHashBtn->setToolTip(tr("Copy MD5 hash to clipboard"));
    connect(m_copyHashBtn, &QPushButton::clicked, this, &BookDetailsDialog::onCopyMd5Hash);
    hashRowLayout->addWidget(m_copyHashBtn);

    techForm->addRow(tr("MD5 Hash:"), hashRowLayout);

    // File Path row
    auto* pathRowLayout = new QHBoxLayout();
    pathRowLayout->setSpacing(8);

    m_filePathEdit = new QLineEdit(m_book.filePath, techGroup);
    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setFixedHeight(30);
    m_filePathEdit->setToolTip(m_book.filePath);
    m_filePathEdit->setStyleSheet("QLineEdit { background-color: #F8FAFC; color: #334155; }");
    pathRowLayout->addWidget(m_filePathEdit, 1);

    m_showInFileManagerBtn = new QPushButton(tr("Show in File Manager"), techGroup);
    m_showInFileManagerBtn->setFixedHeight(30);
    m_showInFileManagerBtn->setCursor(Qt::PointingHandCursor);
    m_showInFileManagerBtn->setToolTip(tr("Open containing folder in your desktop file manager"));
    connect(m_showInFileManagerBtn, &QPushButton::clicked, this, &BookDetailsDialog::onShowInFileManager);
    pathRowLayout->addWidget(m_showInFileManagerBtn);

    techForm->addRow(tr("File Path:"), pathRowLayout);

    rightLayout->addWidget(techGroup);
    rightLayout->addStretch(1);

    contentLayout->addLayout(rightLayout, 1);
    rootLayout->addLayout(contentLayout, 1);

    // ---------------------------------------------------------------------
    // BOTTOM ACTION BAR
    // ---------------------------------------------------------------------
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 8, 0, 0);
    bottomLayout->setSpacing(10);

    // "Remove from Library" button (styled with danger theme)
    m_removeBtn = new QPushButton(tr("Remove from Library..."), this);
    m_removeBtn->setFixedHeight(36);
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    m_removeBtn->setObjectName("removeButton");
    connect(m_removeBtn, &QPushButton::clicked, this, &BookDetailsDialog::onRemoveFromLibrary);
    bottomLayout->addWidget(m_removeBtn);

    bottomLayout->addStretch(1);

    // "Cancel" button
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setFixedHeight(36);
    m_cancelBtn->setFixedWidth(90);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(m_cancelBtn);

    // "Save Changes" button (primary blue)
    m_saveBtn = new QPushButton(tr("Save Changes"), this);
    m_saveBtn->setFixedHeight(36);
    m_saveBtn->setFixedWidth(130);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setObjectName("saveButton");
    m_saveBtn->setDefault(true);
    connect(m_saveBtn, &QPushButton::clicked, this, &BookDetailsDialog::onSaveChanges);
    bottomLayout->addWidget(m_saveBtn);

    rootLayout->addLayout(bottomLayout);
}

void BookDetailsDialog::applyStyles() {
    setStyleSheet(R"(
        QDialog {
            background-color: #FAFAFA;
            color: #0F172A;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            font-size: 10pt;
        }

        QGroupBox {
            font-weight: 600;
            font-size: 9.5pt;
            color: #334155;
            border: 1px solid #E2E8F0;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 12px;
            background-color: #FFFFFF;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 4px;
            background-color: #FFFFFF;
        }

        QLineEdit, QDoubleSpinBox {
            background-color: #FFFFFF;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            padding: 0 8px;
            color: #1E293B;
            font-size: 9.5pt;
        }

        QLineEdit:focus, QDoubleSpinBox:focus {
            border: 1px solid #3B82F6;
        }

        QCheckBox {
            font-size: 9.5pt;
            color: #1E293B;
            spacing: 8px;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #CBD5E1;
            background-color: #FFFFFF;
        }

        QCheckBox::indicator:checked {
            background-color: #3B82F6;
            border-color: #2563EB;
        }

        QProgressBar {
            background-color: #E2E8F0;
            border: none;
            border-radius: 4px;
        }

        QProgressBar::chunk {
            background-color: #3B82F6;
            border-radius: 4px;
        }

        QPushButton {
            background-color: #F8FAFC;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            padding: 0 12px;
            color: #334155;
            font-weight: 500;
            font-size: 9pt;
        }

        QPushButton:hover {
            background-color: #F1F5F9;
            border-color: #94A3B8;
        }

        QPushButton#saveButton {
            background-color: #2563EB;
            border: 1px solid #1D4ED8;
            color: #FFFFFF;
            font-weight: 600;
        }

        QPushButton#saveButton:hover {
            background-color: #1D4ED8;
        }

        QPushButton#removeButton {
            background-color: #FEF2F2;
            border: 1px solid #FECACA;
            color: #DC2626;
            font-weight: 500;
        }

        QPushButton#removeButton:hover {
            background-color: #FEE2E2;
            border-color: #FCA5A5;
        }

        QToolButton {
            border: 1px solid #E2E8F0;
            border-radius: 4px;
            background-color: #F8FAFC;
            color: #64748B;
            font-size: 8pt;
        }

        QToolButton:hover {
            background-color: #F1F5F9;
            color: #0F172A;
        }
    )");
}

void BookDetailsDialog::updateCoverPreview() {
    if (!m_book.coverImagePath.isEmpty() && QFile::exists(m_book.coverImagePath)) {
        QPixmap original(m_book.coverImagePath);
        if (!original.isNull()) {
            QPixmap scaled = original.scaled(140, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            // Render into rounded-corner pixmap
            QPixmap rounded(scaled.size());
            rounded.fill(Qt::transparent);

            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing, true);
            QPainterPath clip;
            clip.addRoundedRect(rounded.rect(), 6.0, 6.0);
            p.setClipPath(clip);
            p.drawPixmap(0, 0, scaled);
            p.end();

            m_coverLabel->setPixmap(rounded);
            m_coverLabel->setToolTip(m_book.coverImagePath);
            return;
        }
    }

    // Fallback procedural cover
    QPixmap fallback = createFallbackCover(140, 200);
    m_coverLabel->setPixmap(fallback);
    m_coverLabel->setToolTip(tr("Procedural fallback cover preview"));
}

QPixmap BookDetailsDialog::createFallbackCover(int width, int height) {
    QPixmap pix(width, height);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF rect(0, 0, width, height);
    QPainterPath path;
    path.addRoundedRect(rect, 6.0, 6.0);
    p.setClipPath(path);

    // Procedural gradient matching BookCardWidget
    const uint hash = qHash(m_book.title);
    const int hue = static_cast<int>(hash % 360);
    const QColor c1 = QColor::fromHsv(hue, 140, 70);
    const QColor c2 = QColor::fromHsv((hue + 35) % 360, 180, 40);

    QLinearGradient grad(rect.topLeft(), rect.bottomRight());
    grad.setColorAt(0.0, c1);
    grad.setColorAt(1.0, c2);
    p.fillPath(path, grad);

    // Spine crease
    p.setPen(QColor(0, 0, 0, 40));
    p.drawLine(QPointF(rect.left() + 7, rect.top()), QPointF(rect.left() + 7, rect.bottom()));
    p.setPen(QColor(255, 255, 255, 30));
    p.drawLine(QPointF(rect.left() + 8, rect.top()), QPointF(rect.left() + 8, rect.bottom()));

    // Title text
    p.setPen(Qt::white);
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    p.setFont(titleFont);

    const QRectF textRect = rect.adjusted(14, 14, -10, -30);
    const QFontMetrics fm(titleFont);
    const QString elidedTitle = fm.elidedText(m_book.title.isEmpty() ? tr("Untitled Book") : m_book.title,
                                             Qt::ElideRight,
                                             static_cast<int>(textRect.width() * 2));
    p.drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, elidedTitle);

    // Author text
    if (!m_book.author.isEmpty()) {
        QFont authorFont = font();
        authorFont.setPointSize(8);
        p.setFont(authorFont);
        p.setPen(QColor(255, 255, 255, 200));
        const QRectF authorRect = rect.adjusted(14, height - 36, -10, -10);
        p.drawText(authorRect, Qt::AlignBottom | Qt::AlignLeft,
                   fm.elidedText(m_book.author, Qt::ElideRight, static_cast<int>(authorRect.width())));
    }

    p.end();
    return pix;
}

QString BookDetailsDialog::formatFileSize(qint64 bytes) {
    if (bytes <= 0) return QStringLiteral("0 B");
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = 1024 * KB;
    constexpr qint64 GB = 1024 * MB;

    if (bytes >= GB) {
        return QString("%1 GB (%2 bytes)")
            .arg(QString::number(static_cast<double>(bytes) / GB, 'f', 2),
                 QLocale().toString(bytes));
    } else if (bytes >= MB) {
        return QString("%1 MB (%2 bytes)")
            .arg(QString::number(static_cast<double>(bytes) / MB, 'f', 2),
                 QLocale().toString(bytes));
    } else if (bytes >= KB) {
        return QString("%1 KB (%2 bytes)")
            .arg(QString::number(static_cast<double>(bytes) / KB, 'f', 1),
                 QLocale().toString(bytes));
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QString BookDetailsDialog::formatTimestamp(qint64 msecs) {
    if (msecs <= 0) return tr("Never");
    // Handle both seconds and milliseconds since epoch
    if (msecs < 10000000000LL) {
        msecs *= 1000;
    }
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecs);
    return dt.toString("yyyy-MM-dd hh:mm AP");
}

QColor BookDetailsDialog::formatBadgeColor(const QString& format) {
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

void BookDetailsDialog::onChangeCover() {
    const QString filter = tr("Image Files (*.png *.jpg *.jpeg *.webp *.bmp);;All Files (*.*)");
    const QString path = QFileDialog::getOpenFileName(this, tr("Select Book Cover Image"), QString(), filter);
    if (!path.isEmpty()) {
        m_book.coverImagePath = path;
        updateCoverPreview();
    }
}

void BookDetailsDialog::onClearRating() {
    if (m_starRating) {
        m_starRating->setRating(0.0);
    }
}

void BookDetailsDialog::onCopyMd5Hash() {
    if (m_hashEdit && !m_hashEdit->text().isEmpty()) {
        QGuiApplication::clipboard()->setText(m_hashEdit->text());
        m_copyHashBtn->setText(tr("Copied!"));
        QTimer::singleShot(1500, this, [this]() {
            if (m_copyHashBtn) {
                m_copyHashBtn->setText(tr("Copy"));
            }
        });
    }
}

void BookDetailsDialog::onShowInFileManager() {
    const QString path = m_book.filePath;
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Notice"), tr("No file path is associated with this book."));
        return;
    }

    const QFileInfo fi(path);
    if (!fi.exists()) {
        if (fi.dir().exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.dir().absolutePath()));
        } else {
            QMessageBox::warning(this, tr("File Not Found"),
                                 tr("The book file does not exist on disk:\n%1").arg(path));
        }
        return;
    }

    // Open containing directory in native desktop file manager
    const QString folder = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void BookDetailsDialog::onSaveChanges() {
    const QString title = m_titleEdit->text().trimmed();
    if (title.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Book title cannot be empty."));
        m_titleEdit->setFocus();
        return;
    }

    m_book.title = title;
    m_book.author = m_authorEdit->text().trimmed();
    m_book.series = m_seriesEdit->text().trimmed();
    m_book.seriesIndex = m_seriesIndexSpin->value();
    m_book.rating = m_starRating->rating();
    m_book.isFavorite = m_favoriteCheck->isChecked();

    const bool ok = DatabaseManager::instance().updateBook(m_book);
    if (!ok) {
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to save changes to the library database."));
        return;
    }

    emit bookUpdated(m_book);
    accept();
}

void BookDetailsDialog::onRemoveFromLibrary() {
    QMessageBox confirmBox(this);
    confirmBox.setWindowTitle(tr("Remove Book"));
    confirmBox.setIcon(QMessageBox::Warning);
    confirmBox.setText(tr("Are you sure you want to remove \"%1\" from your library?").arg(m_book.title));
    confirmBox.setInformativeText(tr("This will remove the book, its reading progress, annotations, and statistics from the library."));

    auto* deleteFileCheck = new QCheckBox(tr("Also delete book file from disk permanently"), &confirmBox);
    deleteFileCheck->setStyleSheet("color: #DC2626; font-weight: bold; margin-top: 10px;");
    confirmBox.setCheckBox(deleteFileCheck);

    QPushButton* removeBtn = confirmBox.addButton(tr("Remove from Library"), QMessageBox::AcceptRole);
    removeBtn->setStyleSheet("background-color: #DC2626; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    QPushButton* cancelBtn = confirmBox.addButton(QMessageBox::Cancel);
    confirmBox.setDefaultButton(cancelBtn);

    confirmBox.exec();

    if (confirmBox.clickedButton() == removeBtn) {
        const bool deletePermanently = deleteFileCheck->isChecked();
        if (deletePermanently && !m_book.filePath.isEmpty() && QFile::exists(m_book.filePath)) {
            if (!QFile::remove(m_book.filePath)) {
                QMessageBox::warning(this, tr("Warning"),
                                     tr("Could not delete file from disk:\n%1\n\nRemoving from library anyway.").arg(m_book.filePath));
            }
        }

        const bool deleted = DatabaseManager::instance().deleteBook(m_book.id);
        if (deleted) {
            m_bookDeleted = true;
            emit bookDeleted(m_book.id);
            accept();
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Failed to delete book from database."));
        }
    }
}

void BookDetailsDialog::onFetchOnlineMetadata() {
    QString searchTitle = m_titleEdit ? m_titleEdit->text().trimmed() : m_book.title;
    if (searchTitle.isEmpty()) {
        QMessageBox::warning(this, tr("Search Error"), tr("Please enter a book title first."));
        return;
    }

    m_fetchOnlineBtn->setEnabled(false);
    m_fetchOnlineBtn->setText(tr("Searching OpenLibrary..."));

    auto* fetcher = new MetadataFetcherService(this);
    connect(fetcher, &MetadataFetcherService::metadataFound, this, [this, fetcher](const QVariantMap& meta, const QString& coverUrl) {
        if (meta.contains("title") && m_titleEdit) {
            m_titleEdit->setText(meta["title"].toString());
        }
        if (meta.contains("author") && m_authorEdit) {
            m_authorEdit->setText(meta["author"].toString());
        }
        if (!coverUrl.isEmpty()) {
            m_fetchOnlineBtn->setText(tr("Downloading cover..."));
            fetcher->downloadCover(coverUrl);
        } else {
            m_fetchOnlineBtn->setEnabled(true);
            m_fetchOnlineBtn->setText(tr("🌐 Fetch Online Metadata"));
            QMessageBox::information(this, tr("Metadata Found"), tr("Updated title and author from OpenLibrary."));
            fetcher->deleteLater();
        }
    });

    connect(fetcher, &MetadataFetcherService::coverDownloaded, this, [this, fetcher](const QString& localPath) {
        m_book.coverImagePath = localPath;
        updateCoverPreview();
        m_fetchOnlineBtn->setEnabled(true);
        m_fetchOnlineBtn->setText(tr("🌐 Fetch Online Metadata"));
        QMessageBox::information(this, tr("Metadata & Cover Updated"), tr("Successfully fetched book details and cover image from OpenLibrary!"));
        fetcher->deleteLater();
    });

    connect(fetcher, &MetadataFetcherService::fetchError, this, [this, fetcher](const QString& errMsg) {
        m_fetchOnlineBtn->setEnabled(true);
        m_fetchOnlineBtn->setText(tr("🌐 Fetch Online Metadata"));
        QMessageBox::warning(this, tr("Online Search"), errMsg);
        fetcher->deleteLater();
    });

    fetcher->searchMetadata(searchTitle, m_authorEdit ? m_authorEdit->text().trimmed() : QString());
}

} // namespace LunaRead
