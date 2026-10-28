#pragma once

#include <QFrame>
#include <QString>
#include <QPixmap>
#include <QColor>
#include "../core/Types.h"

class QLabel;
class QProgressBar;
class QPushButton;

namespace LunaRead {

/**
 * @brief Modern list row widget for bookshelf Detailed List View mode.
 *
 * Displays:
 *  - 48x64 book cover thumbnail (or procedurally generated cover badge if no cover)
 *  - Title (bold, elided if long)
 *  - Author name (muted, elided if long)
 *  - Format pill badge (EPUB, PDF, MOBI, etc.)
 *  - Progress bar with percentage text (e.g. 45%)
 *  - Formatted file size (e.g. "2.4 MB")
 *  - Formatted last read timestamp (e.g. "Yesterday", or "Oct 26, 2026")
 *  - Action buttons: "Open", "Details", "Delete"
 */
class BookListRowWidget : public QFrame {
    Q_OBJECT

public:
    explicit BookListRowWidget(const BookItem& book, QWidget* parent = nullptr);
    ~BookListRowWidget() override = default;

    [[nodiscard]] const BookItem& book() const { return m_book; }
    void updateBook(const BookItem& book);

    static QString formatFileSize(qint64 bytes);
    static QString formatLastRead(qint64 timestamp);
    static QColor formatBadgeColor(const QString& format);
    static QPixmap generateThumbnail(const BookItem& book, int width = 48, int height = 64);

signals:
    void bookClicked(const QString& bookId);
    void openRequested(const QString& bookId);
    void detailsRequested(const QString& bookId);
    void deleteRequested(const QString& bookId);
    void exportRequested(const QString& bookId);
    void favoriteToggled(const QString& bookId, bool isFavorite);
    void markFinishedRequested(const QString& bookId);
    void resetProgressRequested(const QString& bookId);
    void showInFileManagerRequested(const QString& bookId);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void updateElidedLabels();

    BookItem m_book;

    QLabel* m_coverLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_authorLabel = nullptr;
    QLabel* m_formatBadge = nullptr;
    QLabel* m_progressText = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QLabel* m_lastReadLabel = nullptr;
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_detailsBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
};

} // namespace LunaRead
