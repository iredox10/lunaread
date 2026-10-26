#pragma once

#include "BookParser.h"
#include <QString>
#include <QByteArray>
#include <QVector>
#include <optional>

namespace LunaRead {

/**
 * @brief Production-grade DjVu Parser for the LunaRead-Qt desktop reader.
 *
 * Implements AT&T / LizardTech DjVu IFF document format parsing:
 * - Validates AT&T / FORM magic bytes with DJVU (single-page) and DJVM (multi-page bundled) support.
 * - Parses 8-byte IFF chunk headers with even-byte alignment padding.
 * - Extracts INFO chunks: page width, height, DPI, version, and gamma.
 * - Parses DIRM (directory) chunks for multi-page bundled documents.
 * - Extracts TXTa (text / OCR layer) chunks and converts S-expressions into readable page text.
 * - Parses NAVI (navigation / bookmarks) and ANNa (annotations) chunks via an S-expression parser
 *   into a hierarchical TocItem table of contents.
 * - Extracts document metadata (title, author, creation date) from ANNa/NAVI metadata structures.
 * - Assembles a complete, structured BookDocument with metadata, chapters, and table of contents.
 */
class DjVuParser : public BookParser {
public:
    DjVuParser() = default;
    ~DjVuParser() override = default;

    /// BookParser interface implementation
    QString name() const override { return QStringLiteral("DjVuParser"); }
    QVector<QString> supportedExtensions() const override { return {QStringLiteral("djvu")}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;

    /// DjVu IFF Chunk representation
    struct Chunk {
        QByteArray id;         /// 4-byte chunk FourCC (e.g. "INFO", "TXTa", "NAVI", "FORM")
        uint32_t length = 0;   /// Chunk payload length
        QByteArray formType;   /// Secondary type for "FORM" chunks (e.g. "DJVU", "DJVM")
        QByteArray data;       /// Raw chunk payload
        QVector<Chunk> children; /// Nested chunks if this is a "FORM" container
    };

    /// DjVu INFO chunk fields
    struct PageInfo {
        uint16_t width = 0;
        uint16_t height = 0;
        uint8_t minorVersion = 0;
        uint8_t majorVersion = 0;
        uint16_t dpi = 300;
        uint8_t gamma = 0;
        uint8_t flags = 0;
    };

    /// S-Expression AST node for DjVu metadata, annotations, and bookmarks
    struct SExpNode {
        enum class Type { Symbol, String, List };
        Type type = Type::Symbol;
        QString value;
        QVector<SExpNode> children;
    };

    // Public static utilities for inspection and testing
    static bool validateMagic(const QByteArray& data, bool* isMultiPage = nullptr);
    static PageInfo parseInfoChunk(const QByteArray& chunkData);
    static QString extractTextFromTXTa(const QByteArray& chunkData);
    static QVector<TocItem> parseBookmarksSExp(const QByteArray& sexpData);

private:
    /// Parses IFF chunks from a byte stream
    static QVector<Chunk> parseChunks(const QByteArray& data, qsizetype offset, qsizetype maxLen);

    /// S-Expression parser helpers
    static SExpNode parseSExpression(const QString& input, qsizetype& pos);
    static void skipWhitespaceAndComments(const QString& input, qsizetype& pos);
    static QString parseQuotedString(const QString& input, qsizetype& pos);
    static QString parseSymbol(const QString& input, qsizetype& pos);

    /// Converts bookmark S-expression nodes to hierarchical TocItem entries
    static QVector<TocItem> buildTocFromSExp(const QVector<SExpNode>& bookmarkNodes, int level);

    /// Extracts title and author from ANNa / NAVI metadata S-expressions
    static void extractMetadataFromSExp(const SExpNode& root, QString& title, QString& author);

    /// Recursively searches for all chunks with a specific ID in the chunk hierarchy
    static void findChunksById(const QVector<Chunk>& chunks, const QByteArray& id, QVector<Chunk>& result);
};

} // namespace LunaRead
