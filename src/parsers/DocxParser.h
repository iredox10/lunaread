#pragma once

#include "BookParser.h"

namespace LunaRead {

class DocxParser : public BookParser {
public:
    DocxParser() = default;
    ~DocxParser() override = default;

    QString name() const override { return "DocxParser"; }
    QVector<QString> supportedExtensions() const override { return {"docx"}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;
};

} // namespace LunaRead
