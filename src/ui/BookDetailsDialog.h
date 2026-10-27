#pragma once

#include <QDialog>
#include <QString>
#include <QPixmap>
#include <QWidget>
#include "../core/Types.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QToolButton;
class QProgressBar;
class QGroupBox;
QT_END_NAMESPACE

namespace LunaRead {

/**
 * @brief Interactive 1-5 stars rating widget with gold amber stars,
 *        hover glow preview, keyboard navigation, and click-to-rate.
 */
class StarRatingWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double rating READ rating WRITE setRating NOTIFY ratingChanged)

public:
    explicit StarRatingWidget(QWidget* parent = nullptr);
    ~StarRatingWidget() override = default;

    [[nodiscard]] double rating() const { return m_rating; }
    void setRating(double rating);

    [[nodiscard]] int maxStars() const { return m_maxStars; }
    void setMaxStars(int maxStars);

    [[nodiscard]] bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void ratingChanged(double rating);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int starAtPosition(const QPointF& pos) const;
    static QPolygonF starPolygon(const QPointF& center, qreal rOuter, qreal rInner);

    double m_rating = 0.0;
    int m_hoverRating = -1;
    int m_maxStars = 5;
    bool m_readOnly = false;
    qreal m_starSize = 22.0;
    qreal m_starSpacing = 6.0;
};

/**
 * @brief Comprehensive e-book properties and metadata editor dialog for LunaRead.
 *
 * Opened when right-clicking a book card or clicking an info action.
 * Displays and allows editing:
 * - Cover thumbnail preview with fallback procedural rendering and custom cover picker
 * - Title (editable QLineEdit)
 * - Author (editable QLineEdit)
 * - Series and Series Index (editable QLineEdit & QDoubleSpinBox)
 * - Rating (interactive 1-5 stars rating widget)
 * - Favorite status (QCheckBox)
 * - Read-only technical metadata: File format badge, formatted file size,
 *   File MD5 hash (with copy action), and File path (with "Show in File Manager" button)
 * - Reading progress percentage, chapter/page details, and last read timestamp
 * - "Save Changes" button persisting to DatabaseManager
 * - "Remove from Library" button with confirmation dialog and option to delete file
 */
class BookDetailsDialog : public QDialog {
    Q_OBJECT

public:
    explicit BookDetailsDialog(const BookItem& book, QWidget* parent = nullptr);
    ~BookDetailsDialog() override = default;

    [[nodiscard]] const BookItem& book() const { return m_book; }
    [[nodiscard]] bool isBookDeleted() const { return m_bookDeleted; }

signals:
    void bookUpdated(const BookItem& book);
    void bookDeleted(const QString& bookId);

private slots:
    void onSaveChanges();
    void onRemoveFromLibrary();
    void onChangeCover();
    void onClearRating();
    void onCopyMd5Hash();
    void onShowInFileManager();
    void onFetchOnlineMetadata();

private:
    void setupUi();
    void applyStyles();
    void updateCoverPreview();
    QPixmap createFallbackCover(int width, int height);

    static QString formatFileSize(qint64 bytes);
    static QString formatTimestamp(qint64 msecs);
    static QColor formatBadgeColor(const QString& format);

    BookItem m_book;
    bool m_bookDeleted = false;

    // UI Widgets - Left Column (Cover & Progress)
    QLabel* m_coverLabel = nullptr;
    QPushButton* m_changeCoverBtn = nullptr;
    QPushButton* m_fetchOnlineBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_progressTextLabel = nullptr;
    QLabel* m_chapterPageLabel = nullptr;
    QLabel* m_lastReadLabel = nullptr;
    QLabel* m_dateAddedLabel = nullptr;

    // UI Widgets - Right Column (Editable Metadata)
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QLineEdit* m_seriesEdit = nullptr;
    QDoubleSpinBox* m_seriesIndexSpin = nullptr;
    StarRatingWidget* m_starRating = nullptr;
    QToolButton* m_clearRatingBtn = nullptr;
    QCheckBox* m_favoriteCheck = nullptr;

    // UI Widgets - Right Column (Technical Metadata)
    QLabel* m_formatBadgeLabel = nullptr;
    QLabel* m_fileSizeLabel = nullptr;
    QLineEdit* m_hashEdit = nullptr;
    QPushButton* m_copyHashBtn = nullptr;
    QLineEdit* m_filePathEdit = nullptr;
    QPushButton* m_showInFileManagerBtn = nullptr;

    // Dialog Action Buttons
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
};

} // namespace LunaRead
