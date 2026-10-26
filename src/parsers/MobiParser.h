#pragma once

#include "BookParser.h"

namespace LunaRead {

class MobiParser : public BookParser {
public:
    MobiParser() = default;
    ~MobiParser() override = default;

    QString name() const override { return "MobiParser"; }
    QVector<QString> supportedExtensions() const override { return {"mobi", "azw", "azw3", "prc"}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;

private:
    static QByteArray decompressPalmDocLz77(const QByteArray& src);
};

} // namespace LunaRead
