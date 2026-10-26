#pragma once

#include "BookParser.h"
#include <memory>
#include <vector>

namespace LunaRead {

class ParserFactory {
public:
    static std::optional<BookDocument> parse(const QString& filePath);
    static bool isSupported(const QString& filePathOrExtension);
    static QVector<QString> supportedExtensions();

private:
    static std::vector<std::shared_ptr<BookParser>> getParsers();
};

} // namespace LunaRead
