#pragma once

#include "BookParser.h"
#include <QString>
#include <QByteArray>
#include <QVector>

namespace LunaRead {

/**
 * @brief Parser for HTML, HTM, and XHTML documents.
 * Extracts title, author metadata, chapters based on headings (H1-H6) or page-break dividers,
 * strips script/style tags, and generates hierarchical TOC.
 */
class HtmlParser : public BookParser {
public:
    HtmlParser() = default;
    ~HtmlParser() override = default;

    QString name() const override { return QStringLiteral("HtmlParser"); }
    QVector<QString> supportedExtensions() const override {
        return {QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("xhtml")};
    }

    bool canParse(const QString& filePathOrExtension) const override;
    bool canParse(const QString& filePath, const QByteArray& headerBytes) const;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data,
                                           const QString& format = "",
                                           const QString& filePath = "") override;

private:
    static QString stripTags(const QString& html);
    static QString extractMetadata(const QString& html, const QString& tag);
};

} // namespace LunaRead
