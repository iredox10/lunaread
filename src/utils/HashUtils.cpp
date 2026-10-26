#include "HashUtils.h"
#include <QCryptographicHash>
#include <QFile>

namespace LunaRead {

QString HashUtils::calculateBytesMd5(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex();
}

QString HashUtils::calculateBytesSha256(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

QString HashUtils::calculateFileMd5(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    while (!file.atEnd()) {
        hash.addData(file.read(65536)); // 64 KB streaming buffer
    }
    return hash.result().toHex();
}

QString HashUtils::getMoonReaderSyncFileName(const QString& bookMd5) {
    return QString(".moonreader/%1.po").arg(bookMd5);
}

} // namespace LunaRead
