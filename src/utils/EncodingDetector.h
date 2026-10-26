#pragma once

#include <QString>
#include <QByteArray>

namespace LunaRead {

class EncodingDetector {
public:
    /// Detects character encoding and decodes raw byte array without exception throwing.
    static QString decodeBytes(const QByteArray& data, QString* detectedEncoding = nullptr);
};

} // namespace LunaRead
