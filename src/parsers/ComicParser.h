#pragma once

#include "BookParser.h"

namespace LunaRead {

class ComicParser : public BookParser {
public:
    ComicParser() = default;
    ~ComicParser() override = default;

    QString name() const override { return "ComicParser"; }
    QVector<QString> supportedExtensions() const override { return {"cbz", "cbr"}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;
};

} // namespace LunaRead
