#pragma once

#include "BookParser.h"
#include <QString>
#include <QByteArray>
#include <QVector>
#include <optional>

namespace LunaRead {

/**
 * @class OdtParser
 * @brief Production-grade parser for OpenDocument Text (.odt) documents.
 *
 * Implements OpenDocument Format (ODF) parsing:
 * - Validates ZIP archive headers (PK\x03\x04) and OpenDocument signatures.
 * - Extracts package contents (content.xml, meta.xml, and embedded media) via libarchive.
 * - Parses Dublin Core metadata (<dc:title>, <dc:creator>) from meta.xml.
 * - Parses headings (<text:h>) with outline levels for chapter segmentation and hierarchical TOC.
 * - Extracts paragraphs (<text:p>) with rich text spans, formatting, lists, line breaks, and images.
 * - Assembles a complete BookDocument structure for LunaRead reader viewport.
 */
class OdtParser : public BookParser {
public:
    OdtParser() = default;
    ~OdtParser() override = default;

    [[nodiscard]] QString name() const override { return QStringLiteral("OdtParser"); }
    [[nodiscard]] QVector<QString> supportedExtensions() const override { return {QStringLiteral("odt")}; }
    [[nodiscard]] bool canParse(const QString& filePathOrExtension) const override;

    [[nodiscard]] std::optional<BookDocument> parseFile(const QString& filePath) override;
    [[nodiscard]] std::optional<BookDocument> parseBytes(const QByteArray& data,
                                                         const QString& format = QStringLiteral("odt"),
                                                         const QString& filePath = QString()) override;

    /**
     * @brief Checks if the given byte data has a valid ZIP signature and contains content.xml.
     * @param data Byte array to inspect.
     * @return True if valid ODT package.
     */
    static bool isValidOdtPackage(const QByteArray& data);
};

} // namespace LunaRead
