#pragma once

#include <QString>
#include <QByteArray>

namespace LunaRead {

class HashUtils {
public:
    static QString calculateBytesMd5(const QByteArray& data);
    static QString calculateBytesSha256(const QByteArray& data);
    static QString calculateFileMd5(const QString& filePath);
    static QString getMoonReaderSyncFileName(const QString& bookMd5);
};

} // namespace LunaRead
