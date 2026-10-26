#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <optional>
#include "../core/Types.h"

namespace LunaRead {

class BookParser {
public:
    virtual ~BookParser() = default;

    virtual QString name() const = 0;
    virtual QVector<QString> supportedExtensions() const = 0;
    virtual bool canParse(const QString& filePathOrExtension) const = 0;
    virtual std::optional<BookDocument> parseFile(const QString& filePath) = 0;
    virtual std::optional<BookDocument> parseBytes(const QByteArray& data, const QString& format = "", const QString& filePath = "") = 0;
};

} // namespace LunaRead
