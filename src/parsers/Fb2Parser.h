#pragma once

#include "BookParser.h"

namespace LunaRead {

class Fb2Parser : public BookParser {
public:
    Fb2Parser() = default;
    ~Fb2Parser() override = default;

    QString name() const override { return "Fb2Parser"; }
    QVector<QString> supportedExtensions() const override { return {"fb2"}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;
};

} // namespace LunaRead
