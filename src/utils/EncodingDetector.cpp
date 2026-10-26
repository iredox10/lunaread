#include "EncodingDetector.h"
#include <QStringConverter>

namespace LunaRead {

QString EncodingDetector::decodeBytes(const QByteArray& data, QString* detectedEncoding) {
    if (data.isEmpty()) {
        if (detectedEncoding) *detectedEncoding = "UTF-8";
        return QString();
    }

    // 1. Check BOM signatures
    if (data.size() >= 3 &&
        static_cast<uint8_t>(data[0]) == 0xEF &&
        static_cast<uint8_t>(data[1]) == 0xBB &&
        static_cast<uint8_t>(data[2]) == 0xBF) {
        if (detectedEncoding) *detectedEncoding = "UTF-8-BOM";
        return QString::fromUtf8(data.constData() + 3, data.size() - 3);
    }

    if (data.size() >= 2) {
        uint8_t b0 = static_cast<uint8_t>(data[0]);
        uint8_t b1 = static_cast<uint8_t>(data[1]);
        if (b0 == 0xFF && b1 == 0xFE) {
            if (detectedEncoding) *detectedEncoding = "UTF-16LE";
            auto toUtf16LE = QStringDecoder(QStringDecoder::Utf16LE);
            return toUtf16LE(data.mid(2));
        } else if (b0 == 0xFE && b1 == 0xFF) {
            if (detectedEncoding) *detectedEncoding = "UTF-16BE";
            auto toUtf16BE = QStringDecoder(QStringDecoder::Utf16BE);
            return toUtf16BE(data.mid(2));
        }
    }

    // 2. Strict UTF-8 validation
    auto utf8Decoder = QStringDecoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString utf8Result = utf8Decoder(data);
    if (!utf8Decoder.hasError()) {
        if (detectedEncoding) *detectedEncoding = "UTF-8";
        return utf8Result;
    }

    // 3. Fallback to Windows-1252 / Latin-1
    if (detectedEncoding) *detectedEncoding = "Windows-1252";
    auto latin1Decoder = QStringDecoder(QStringDecoder::Latin1);
    return latin1Decoder(data);
}

} // namespace LunaRead
