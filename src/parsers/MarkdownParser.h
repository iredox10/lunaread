#pragma once

#include "BookParser.h"

namespace LunaRead {

class MarkdownParser : public BookParser {
public:
    MarkdownParser() = default;
    ~MarkdownParser() override = default;

    QString name() const override { return "MarkdownParser"; }
    QVector<QString> supportedExtensions() const override { return {"md", "markdown"}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;
};

} // namespace LunaRead
