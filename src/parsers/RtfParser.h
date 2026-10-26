#pragma once

#include "BookParser.h"
#include <QString>
#include <QByteArray>
#include <QVector>
#include <optional>

namespace LunaRead {

/**
 * @class RtfParser
 * @brief Production-grade parser for Rich Text Format (.rtf) documents.
 *
 * Implements a robust RTF state machine tracking:
 * - Group depth nesting with curly braces '{' and '}'.
 * - Standard control words including \par (paragraph break), \page and \sect (chapter breaks),
 *   \b and \i (typography styling), \'xx (hex character escapes), and \uN? (Unicode code points).
 * - RTF destination filtering including \fonttbl, \colortbl, \stylesheet, \info, \pict,
 *   and arbitrary skipped destinations starting with \*.
 * - Metadata extraction from the \info block (\title and \author).
 * - Complete BookDocument assembly containing chapters and hierarchical Table of Contents (TOC).
 */
class RtfParser : public BookParser {
public:
    RtfParser() = default;
    ~RtfParser() override = default;

    [[nodiscard]] QString name() const override { return QStringLiteral("RtfParser"); }
    [[nodiscard]] QVector<QString> supportedExtensions() const override { return {QStringLiteral("rtf")}; }
    [[nodiscard]] bool canParse(const QString& filePathOrExtension) const override;

    [[nodiscard]] std::optional<BookDocument> parseFile(const QString& filePath) override;
    [[nodiscard]] std::optional<BookDocument> parseBytes(const QByteArray& data,
                                                         const QString& format = QStringLiteral("rtf"),
                                                         const QString& filePath = QString()) override;

    /**
     * @brief Validates if the provided byte sequence contains a valid RTF signature header.
     * @param data Raw byte array to validate.
     * @return True if data begins with "{\\rtf", false otherwise.
     */
    static bool isValidRtfHeader(const QByteArray& data);
};

} // namespace LunaRead
