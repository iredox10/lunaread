#pragma once

#include "BookParser.h"

namespace LunaRead {

class EpubParser : public BookParser {
public:
    EpubParser() = default;
    ~EpubParser() override = default;

    QString name() const override { return "EpubParser"; }
    QVector<QString> supportedExtensions() const override { return {"epub"}; }
    bool canParse(const QString& filePathOrExtension) const override;
    std::optional<BookDocument> parseFile(const QString& filePath) override;
    std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") override;
};

} // namespace LunaRead
