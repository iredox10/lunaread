#pragma once

#include "BookParser.h"
#include <QString>
#include <QByteArray>
#include <QVector>

namespace LunaRead {

/**
 * @brief Production parser for UMD (Universal Mobile Document) format.
 *
 * Implements binary parsing of 0xDE9A9B89 container:
 * - Validates UMD magic header.
 * - Extracts book title, author, year, and publisher metadata in UCS-2 LE / UTF-16LE.
 * - Parses chapter index offsets ($#0x83) and titles ($#0x84).
 * - Extracts embedded cover images (Block 0x0E / 0x82).
 * - Decompresses zlib-packed text streams (Block 0x81 / 0x24).
 * - Constructs complete BookDocument with structured chapters and Table of Contents.
 */
class UmdParser : public BookParser {
public:
    UmdParser() = default;
    ~UmdParser() override = default;

    QString name() const override { return QStringLiteral("UmdParser"); }
    QVector<QString> supportedExtensions() const override {
        return {QStringLiteral("umd")};
    }

    bool canParse(const QString& filePathOrExtension) const override;
    bool canParse(const QString& filePath, const QByteArray& headerBytes) const;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data,
                                           const QString& format = "",
                                           const QString& filePath = "") override;

private:
    static bool validateHeader(const QByteArray& data);
    static QString readUcs2String(const QByteArray& data, qsizetype offset, qsizetype length);
};

} // namespace LunaRead
