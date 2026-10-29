#include "CoverService.h"
#include "PdfPageService.h"
#include "../db/DatabaseManager.h"
#include "../parsers/MobiParser.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QBuffer>
#include <QtConcurrent>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QByteArrayView>

namespace LunaRead {

CoverService& CoverService::instance() {
    static CoverService s_instance;
    return s_instance;
}

CoverService::CoverService(QObject* parent) : QObject(parent) {
    QDir().mkpath(coverCacheDir());
}

QString CoverService::coverCacheDir() {
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QDir::homePath() + QStringLiteral("/.cache/LunaRead");
    }
    QString dir = cacheRoot + QStringLiteral("/covers");
    QDir().mkpath(dir);
    return dir;
}

QString CoverService::getCoverPath(const BookItem& book) {
    if (!book.coverImagePath.isEmpty() && QFile::exists(book.coverImagePath)) {
        return book.coverImagePath;
    }

    QString expected = coverCacheDir() + QStringLiteral("/") + book.id + QStringLiteral(".png");
    if (QFile::exists(expected)) {
        DatabaseManager::instance().updateCoverImagePath(book.id, expected);
        return expected;
    }

    requestCover(book);
    return QString();
}

void CoverService::requestCover(const BookItem& book) {
    if (book.id.isEmpty()) return;

    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingBookIds.contains(book.id)) {
            return;
        }
        m_pendingBookIds.insert(book.id);
    }

    QtConcurrent::run([this, book]() {
        QString path = extractAndCacheCover(book);
        {
            QMutexLocker locker(&m_mutex);
            m_pendingBookIds.remove(book.id);
        }
        if (!path.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, id = book.id, path]() {
                emit coverExtracted(id, path);
            }, Qt::QueuedConnection);
        }
    });
}

QString CoverService::saveCoverBytes(const QString& bookId, const QByteArray& imageBytes) {
    if (bookId.isEmpty() || imageBytes.isEmpty()) return QString();

    QString targetPath = coverCacheDir() + QStringLiteral("/") + bookId + QStringLiteral(".png");
    
    // Validate image format; convert to PNG if necessary
    QImage img;
    if (img.loadFromData(imageBytes)) {
        img.save(targetPath, "PNG");
    } else {
        QFile f(targetPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(imageBytes);
            f.close();
        }
    }

    DatabaseManager::instance().updateCoverImagePath(bookId, targetPath);
    emit coverExtracted(bookId, targetPath);
    return targetPath;
}

QString CoverService::extractAndCacheCover(const BookItem& book) {
    QString expected = coverCacheDir() + QStringLiteral("/") + book.id + QStringLiteral(".png");
    if (QFile::exists(expected)) {
        DatabaseManager::instance().updateCoverImagePath(book.id, expected);
        return expected;
    }

    QByteArray bytes;
    const QString fmt = book.format.toLower();

    if (QFile::exists(book.filePath)) {
        if (fmt == QStringLiteral("pdf")) {
            bytes = extractCoverFromPdf(book.filePath, book.id);
        } else if (fmt == QStringLiteral("epub")) {
            bytes = extractCoverFromEpub(book.filePath);
        } else if (fmt == QStringLiteral("cbz") || fmt == QStringLiteral("cbr")) {
            bytes = extractCoverFromComic(book.filePath);
        } else if (fmt == QStringLiteral("mobi") || fmt == QStringLiteral("azw") || fmt == QStringLiteral("azw3")) {
            bytes = extractCoverFromMobi(book.filePath);
        } else if (fmt == QStringLiteral("fb2")) {
            bytes = extractCoverFromFb2(book.filePath);
        }
    }

    if (bytes.isEmpty()) {
        bytes = generateProceduralCover(book.title, book.author, book.format);
    }

    return saveCoverBytes(book.id, bytes);
}

QByteArray CoverService::extractCoverFromPdf(const QString& filePath, const QString& bookId) {
    return PdfPageService::instance().getPageImage(filePath, bookId, 1, 100);
}

QByteArray CoverService::extractCoverFromEpub(const QString& filePath) {
    QString unzip = QStandardPaths::findExecutable(QStringLiteral("unzip"));
    if (unzip.isEmpty()) return QByteArray();

    // 1. Locate container.xml to find OPF path
    QString opfPath;
    {
        QProcess proc;
        proc.start(unzip, QStringList() << QStringLiteral("-p") << filePath << QStringLiteral("META-INF/container.xml"));
        if (proc.waitForFinished(2000) && proc.exitCode() == 0) {
            QByteArray xmlData = proc.readAllStandardOutput();
            QXmlStreamReader xml(xmlData);
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QLatin1String("rootfile")) {
                    opfPath = xml.attributes().value(QStringLiteral("full-path")).toString();
                    break;
                }
            }
        }
    }

    // 2. Read OPF to find cover href
    QString coverRelPath;
    if (!opfPath.isEmpty()) {
        QFileInfo opfInfo(opfPath);
        QString opfDir = opfInfo.path();
        if (opfDir == QStringLiteral(".")) opfDir.clear();

        QProcess proc;
        proc.start(unzip, QStringList() << QStringLiteral("-p") << filePath << opfPath);
        if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
            QByteArray opfData = proc.readAllStandardOutput();
            QXmlStreamReader xml(opfData);
            QString coverId;
            QMap<QString, QString> manifestHref;
            QMap<QString, QString> manifestType;

            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement()) {
                    QString tag = xml.name().toString().toLower();
                    if (tag == QStringLiteral("meta")) {
                        auto attrs = xml.attributes();
                        if (attrs.value(QStringLiteral("name")).toString().compare(QStringLiteral("cover"), Qt::CaseInsensitive) == 0) {
                            coverId = attrs.value(QStringLiteral("content")).toString();
                        }
                    } else if (tag == QStringLiteral("item")) {
                        auto attrs = xml.attributes();
                        QString id = attrs.value(QStringLiteral("id")).toString();
                        QString href = attrs.value(QStringLiteral("href")).toString();
                        QString type = attrs.value(QStringLiteral("media-type")).toString();
                        QString properties = attrs.value(QStringLiteral("properties")).toString();
                        manifestHref[id] = href;
                        manifestType[id] = type;
                        if (properties.contains(QStringLiteral("cover-image"), Qt::CaseInsensitive) ||
                            (coverId.isEmpty() && id.contains(QStringLiteral("cover"), Qt::CaseInsensitive) && type.startsWith(QStringLiteral("image/")))) {
                            coverId = id;
                        }
                    }
                }
            }

            QString foundHref;
            if (!coverId.isEmpty() && manifestHref.contains(coverId)) {
                foundHref = manifestHref[coverId];
            } else {
                for (auto it = manifestHref.begin(); it != manifestHref.end(); ++it) {
                    if (it.value().contains(QStringLiteral("cover"), Qt::CaseInsensitive) && manifestType.value(it.key()).startsWith(QStringLiteral("image/"))) {
                        foundHref = it.value();
                        break;
                    }
                }
            }

            if (!foundHref.isEmpty()) {
                coverRelPath = opfDir.isEmpty() ? foundHref : (opfDir + QStringLiteral("/") + foundHref);
            }
        }
    }

    // 3. Extract cover image by path if found
    if (!coverRelPath.isEmpty()) {
        QProcess proc;
        proc.start(unzip, QStringList() << QStringLiteral("-p") << filePath << coverRelPath);
        if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
            QByteArray data = proc.readAllStandardOutput();
            if (!data.isEmpty()) return data;
        }
    }

    // 4. Fallback: Search zip table for any cover image or first image
    {
        QProcess proc;
        proc.start(unzip, QStringList() << QStringLiteral("-l") << filePath);
        if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
            QString out = QString::fromUtf8(proc.readAllStandardOutput());
            QStringList lines = out.split(QLatin1Char('\n'));
            QString bestImage;
            QString firstImage;

            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                QStringList parts = trimmed.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
                if (parts.size() >= 4) {
                    QString name = parts.mid(3).join(QLatin1Char(' '));
                    QString lower = name.toLower();
                    if (lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg")) ||
                        lower.endsWith(QStringLiteral(".png")) || lower.endsWith(QStringLiteral(".webp"))) {
                        if (firstImage.isEmpty()) firstImage = name;
                        if (lower.contains(QStringLiteral("cover"))) {
                            bestImage = name;
                            break;
                        }
                    }
                }
            }

            QString chosen = !bestImage.isEmpty() ? bestImage : firstImage;
            if (!chosen.isEmpty()) {
                QProcess imgProc;
                imgProc.start(unzip, QStringList() << QStringLiteral("-p") << filePath << chosen);
                if (imgProc.waitForFinished(3000) && imgProc.exitCode() == 0) {
                    return imgProc.readAllStandardOutput();
                }
            }
        }
    }

    return QByteArray();
}

QByteArray CoverService::extractCoverFromComic(const QString& filePath) {
    QString unzip = QStandardPaths::findExecutable(QStringLiteral("unzip"));
    if (unzip.isEmpty()) return QByteArray();

    QProcess proc;
    proc.start(unzip, QStringList() << QStringLiteral("-l") << filePath);
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
        QString out = QString::fromUtf8(proc.readAllStandardOutput());
        for (const QString& line : out.split(QLatin1Char('\n'))) {
            QString trimmed = line.trimmed();
            QStringList parts = trimmed.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
            if (parts.size() >= 4) {
                QString name = parts.mid(3).join(QLatin1Char(' '));
                QString lower = name.toLower();
                if (lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg")) ||
                    lower.endsWith(QStringLiteral(".png")) || lower.endsWith(QStringLiteral(".webp"))) {
                    QProcess imgProc;
                    imgProc.start(unzip, QStringList() << QStringLiteral("-p") << filePath << name);
                    if (imgProc.waitForFinished(3000) && imgProc.exitCode() == 0) {
                        return imgProc.readAllStandardOutput();
                    }
                    break;
                }
            }
        }
    }
    return QByteArray();
}

QByteArray CoverService::extractCoverFromMobi(const QString& filePath) {
    MobiParser parser;
    auto doc = parser.parseFile(filePath);
    if (doc.has_value() && !doc->coverImageBytes.isEmpty()) {
        return doc->coverImageBytes;
    }
    return QByteArray();
}

QByteArray CoverService::extractCoverFromFb2(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();

    // Read first 1MB where coverpage and binary typically live
    QByteArray header = f.read(1024 * 1024);
    static const QRegularExpression binRegex(QStringLiteral(R"(<binary[^>]*id=["'](?i:cover|cover\.jpg|cover\.png)[^"']*["'][^>]*>([\s\S]*?)</binary>)"));
    auto match = binRegex.match(QString::fromUtf8(header));
    if (match.hasMatch()) {
        QByteArray base64Data = match.captured(1).trimmed().toUtf8();
        return QByteArray::fromBase64(base64Data);
    }
    return QByteArray();
}

QByteArray CoverService::generateProceduralCover(const QString& title, const QString& author, const QString& format) {
    const int w = 240;
    const int h = 340;
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    uint hash = qHash(title.isEmpty() ? QStringLiteral("LunaRead") : title);
    int hue = static_cast<int>(hash % 360);
    QColor c1 = QColor::fromHsv(hue, 160, 50);
    QColor c2 = QColor::fromHsv((hue + 35) % 360, 190, 25);

    QPainterPath cardPath;
    cardPath.addRoundedRect(QRectF(0, 0, w, h), 6.0, 6.0);
    p.setClipPath(cardPath);

    // Deep rich bookcloth gradient background
    QLinearGradient grad(0, 0, w, h);
    grad.setColorAt(0.0, c1);
    grad.setColorAt(1.0, c2);
    p.fillPath(cardPath, grad);

    // Spine hinge shadow
    QLinearGradient spine(0, 0, 16, 0);
    spine.setColorAt(0.0, QColor(0, 0, 0, 100));
    spine.setColorAt(0.4, QColor(0, 0, 0, 40));
    spine.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(QRect(0, 0, 16, h), spine);

    // Delicate gold foil border
    p.setPen(QPen(QColor(245, 208, 140, 180), 1.5));
    p.drawRoundedRect(QRectF(12.5, 12.5, w - 25, h - 25), 4.0, 4.0);
    p.setPen(QPen(QColor(245, 208, 140, 100), 0.75));
    p.drawRoundedRect(QRectF(16.5, 16.5, w - 33, h - 33), 3.0, 3.0);

    // Title typesetting
    p.setPen(QColor(254, 243, 199)); // Warm champagne gold
    QFont titleFont(QStringLiteral("Noto Serif"), 13, QFont::Bold);
    p.setFont(titleFont);
    QRectF titleRect(24, 45, w - 48, 120);
    p.drawText(titleRect, Qt::AlignCenter | Qt::TextWordWrap, title.isEmpty() ? QStringLiteral("Untitled") : title);

    // Decorative divider
    p.setPen(QColor(245, 208, 140, 160));
    QFont divFont(QStringLiteral("Noto Serif"), 9);
    p.setFont(divFont);
    p.drawText(QRectF(24, 175, w - 48, 20), Qt::AlignCenter, QStringLiteral("― ✦ ―"));

    // Author typesetting
    p.setPen(QColor(226, 232, 240));
    QFont authorFont(QStringLiteral("Noto Serif"), 10);
    authorFont.setItalic(true);
    p.setFont(authorFont);
    QRectF authorRect(24, 200, w - 48, 40);
    p.drawText(authorRect, Qt::AlignCenter | Qt::TextWordWrap, author.isEmpty() ? QStringLiteral("Unknown Author") : author);

    // Format badge near footer
    QRectF badgeRect((w - 70) / 2.0, h - 42, 70, 20);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 80));
    p.drawRoundedRect(badgeRect, 10.0, 10.0);
    p.setPen(QColor(245, 208, 140));
    QFont badgeFont(QStringLiteral("Roboto"), 8, QFont::Bold);
    badgeFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    p.setFont(badgeFont);
    p.drawText(badgeRect, Qt::AlignCenter, format.isEmpty() ? QStringLiteral("BOOK") : format.toUpper());

    p.end();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

} // namespace LunaRead
