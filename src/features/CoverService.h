#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QPixmap>
#include <QSet>
#include <QMutex>
#include "../core/Types.h"

namespace LunaRead {

class CoverService : public QObject {
    Q_OBJECT
public:
    static CoverService& instance();

    /// Returns cover image path if cached or available, otherwise kicks off async extraction
    QString getCoverPath(const BookItem& book);

    /// Asynchronously extracts and caches cover image for a book
    void requestCover(const BookItem& book);

    /// Synchronously extracts cover bytes from file and saves to cache
    QString extractAndCacheCover(const BookItem& book);

    /// Saves raw image bytes as the book's cover image
    QString saveCoverBytes(const QString& bookId, const QByteArray& imageBytes);

    /// Generates a handsome procedural hardcover thumbnail for text-based books (MD, TXT)
    QByteArray generateProceduralCover(const QString& title, const QString& author, const QString& format);

    /// Directory where covers are stored
    QString coverCacheDir();

signals:
    void coverExtracted(const QString& bookId, const QString& coverPath);

private:
    explicit CoverService(QObject* parent = nullptr);
    ~CoverService() override = default;
    CoverService(const CoverService&) = delete;
    CoverService& operator=(const CoverService&) = delete;

    QByteArray extractCoverFromEpub(const QString& filePath);
    QByteArray extractCoverFromPdf(const QString& filePath, const QString& bookId);
    QByteArray extractCoverFromComic(const QString& filePath);
    QByteArray extractCoverFromMobi(const QString& filePath);
    QByteArray extractCoverFromFb2(const QString& filePath);

    QMutex m_mutex;
    QSet<QString> m_pendingBookIds;
};

} // namespace LunaRead
