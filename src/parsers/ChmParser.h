#pragma once

#include "BookParser.h"
#include <QString>
#include <QByteArray>
#include <QVector>

namespace LunaRead {

/**
 * @brief Production parser for CHM (Compiled HTML Help) e-books.
 *
 * Implements ITSF container validation, internal directory chunk traversal (PMGL/PMGI),
 * topic listing extraction, HTML content aggregation, and hierarchical TOC reconstruction.
 */
class ChmParser : public BookParser {
public:
    ChmParser() = default;
    ~ChmParser() override = default;

    QString name() const override { return QStringLiteral("ChmParser"); }
    QVector<QString> supportedExtensions() const override {
        return {QStringLiteral("chm")};
    }

    bool canParse(const QString& filePathOrExtension) const override;
    bool canParse(const QString& filePath, const QByteArray& headerBytes) const;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data,
                                           const QString& format = "",
                                           const QString& filePath = "") override;

private:
    struct ChmFileEntry {
        QString path;
        quint64 section = 0;
        quint64 offset = 0;
        quint64 length = 0;
    };

    static bool validateHeader(const QByteArray& data);
    static QVector<ChmFileEntry> parseDirectory(const QByteArray& data);
    static QString stripHtml(const QString& html);
};

} // namespace LunaRead
