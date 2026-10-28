#include "ParserFactory.h"
#include "TxtParser.h"
#include "EpubParser.h"
#include "PdfParser.h"
#include "DjVuParser.h"
#include "MobiParser.h"
#include "Fb2Parser.h"
#include "DocxParser.h"
#include "OdtParser.h"
#include "RtfParser.h"
#include "ComicParser.h"
#include "MarkdownParser.h"
#include "HtmlParser.h"
#include "ChmParser.h"
#include "UmdParser.h"
#include <QFileInfo>

namespace LunaRead {

std::vector<std::shared_ptr<BookParser>> ParserFactory::getParsers() {
    static std::vector<std::shared_ptr<BookParser>> s_parsers = {
        std::make_shared<PdfParser>(),
        std::make_shared<DjVuParser>(),
        std::make_shared<EpubParser>(),
        std::make_shared<MobiParser>(),
        std::make_shared<Fb2Parser>(),
        std::make_shared<DocxParser>(),
        std::make_shared<OdtParser>(),
        std::make_shared<RtfParser>(),
        std::make_shared<ComicParser>(),
        std::make_shared<MarkdownParser>(),
        std::make_shared<HtmlParser>(),
        std::make_shared<ChmParser>(),
        std::make_shared<UmdParser>(),
        std::make_shared<TxtParser>()
    };
    return s_parsers;
}

std::optional<BookDocument> ParserFactory::parse(const QString& filePath) {
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return std::nullopt;
    }

    for (const auto& parser : getParsers()) {
        if (parser->canParse(filePath)) {
            auto doc = parser->parseFile(filePath);
            if (doc.has_value()) {
                doc->filePath = filePath;
            }
            return doc;
        }
    }

    // Default fallback to text parser
    TxtParser fallback;
    auto doc = fallback.parseFile(filePath);
    if (doc.has_value()) {
        doc->filePath = filePath;
    }
    return doc;
}

bool ParserFactory::isSupported(const QString& filePathOrExtension) {
    for (const auto& parser : getParsers()) {
        if (parser->canParse(filePathOrExtension)) {
            return true;
        }
    }
    return false;
}

QVector<QString> ParserFactory::supportedExtensions() {
    QVector<QString> exts;
    for (const auto& parser : getParsers()) {
        exts.append(parser->supportedExtensions());
    }
    return exts;
}

} // namespace LunaRead
