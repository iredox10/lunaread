#pragma once

#include "BookParser.h"
#include <QString>
#include <QByteArray>
#include <QVector>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace LunaRead {

/**
 * @brief Production-grade PDF Parser for the LunaRead-Qt desktop reader.
 *
 * Implements ISO 32000-1 (PDF) specification parsing:
 * - Validates %PDF- magic header and extracts version (1.0 through 2.0).
 * - Scans PDF cross-reference table (XRef) and trailer dictionary.
 * - Extracts Document Information dictionary (/Title, /Author, /Subject, /CreationDate)
 *   with support for PDFDocEncoding, UTF-16BE (with BOM), and UTF-8.
 * - Traverses Document Catalog (/Root) and Page Tree (/Pages).
 * - Accurately counts pages and verifies via /Type /Page object scanning.
 * - Parses hierarchical bookmark / outline tree (/Outlines -> /First -> /Next -> /Title -> /Dest)
 *   into a recursive TocItem list.
 * - Decompresses page content streams (FlateDecode via zlib) and extracts text layout (BT...ET, Tj, TJ).
 * - Assembles a complete, structured BookDocument with metadata, chapters, and table of contents.
 */
class PdfParser : public BookParser {
public:
    PdfParser() = default;
    ~PdfParser() override = default;

    /// BookParser interface implementation
    QString name() const override { return QStringLiteral("PdfParser"); }
    QVector<QString> supportedExtensions() const override { return {QStringLiteral("pdf")}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;

    // Public static utilities for inspection and testing
    static bool validateHeader(const QByteArray& data, QString* outVersion = nullptr);
    static QString decodePdfString(const QByteArray& raw);
    static QString decodePdfName(const QByteArray& raw);
    static QByteArray decompressFlate(const QByteArray& compressed);

private:
    /// Trailer dictionary metadata
    struct TrailerInfo {
        qsizetype startxref = -1;
        int rootObjNum = -1;
        int infoObjNum = -1;
        int size = 0;
        int prev = -1;
    };

    /// PDF indirect object position
    struct ObjectLocation {
        int objNum = 0;
        int generation = 0;
        qsizetype byteOffset = 0;
    };

    /// Scans the PDF byte array to map all indirect object numbers to byte offsets
    std::unordered_map<int, qsizetype> buildObjectIndex(const QByteArray& data) const;

    /// Parses the XRef table and trailer dictionary
    bool parseTrailerAndXRef(const QByteArray& data, const std::unordered_map<int, qsizetype>& objMap, TrailerInfo& trailer) const;

    /// Extracts raw object data given an object number
    QByteArray getObjectData(const QByteArray& data, const std::unordered_map<int, qsizetype>& objMap, int objNum) const;

    /// Finds a dictionary entry value by key
    QByteArray findDictValue(const QByteArray& dictBytes, const QByteArray& key) const;

    /// Resolves an indirect object reference "N G R" into object number N
    static int parseIndirectRef(const QByteArray& value);

    /// Collects page objects from the /Pages tree hierarchy
    void collectPagesFromNode(const QByteArray& data,
                             const std::unordered_map<int, qsizetype>& objMap,
                             int nodeObjNum,
                             QVector<int>& pageObjNums,
                             std::unordered_set<int>& visitedNodes) const;

    /// Scans for /Type /Page objects throughout the file
    QVector<int> scanPagesDirectly(const QByteArray& data, const std::unordered_map<int, qsizetype>& objMap) const;

    /// Recursively traverses outline items starting from firstObjNum
    QVector<TocItem> parseOutlineSiblings(const QByteArray& data,
                                         const std::unordered_map<int, qsizetype>& objMap,
                                         int firstObjNum,
                                         int level,
                                         const std::unordered_map<int, int>& pageObjToIndex,
                                         std::unordered_set<int>& visitedOutlines) const;

    /// Extracts destination page index from an outline item's /Dest or /A entry
    int extractDestPageIndex(const QByteArray& itemDict,
                             const QByteArray& data,
                             const std::unordered_map<int, qsizetype>& objMap,
                             const std::unordered_map<int, int>& pageObjToIndex) const;

    /// Extracts text content from a page's /Contents streams
    QString extractPageText(const QByteArray& data,
                            const std::unordered_map<int, qsizetype>& objMap,
                            const QByteArray& pageDict) const;

    /// Extracts text tokens from a decompressed content stream
    static QString parseContentStreamText(const QByteArray& streamBytes);
};

} // namespace LunaRead
