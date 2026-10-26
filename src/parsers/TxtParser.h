#pragma once

#include "BookParser.h"

namespace LunaRead {

class TxtParser : public BookParser {
public:
    TxtParser() = default;
    ~TxtParser() override = default;

    QString name() const override { return "TxtParser"; }
    QVector<QString> supportedExtensions() const override { return {"txt", "text"}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;
};

} // namespace LunaRead
