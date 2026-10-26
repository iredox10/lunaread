#include "DjVuParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <QRegularExpression>
#include <algorithm>

namespace LunaRead {

bool DjVuParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == QStringLiteral("djvu");
}

std::optional<BookDocument> DjVuParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), QStringLiteral("djvu"), filePath);
}

bool DjVuParser::validateMagic(const QByteArray& data, bool* isMultiPage) {
    if (data.size() < 12) {
        return false;
    }

    qsizetype formTypeOffset = -1;

    // Standard DjVu begins with "AT&T" (0x41 0x54 0x26 0x54) followed by "FORM"
    if (data.startsWith("AT&TFORM")) {
        if (data.size() < 16) {
            return false;
        }
        formTypeOffset = 12;
    } else if (data.startsWith("FORM")) {
        // Some standalone DjVu chunks omit the "AT&T" prefix and begin directly with "FORM"
        if (data.size() < 12) {
            return false;
        }
        formTypeOffset = 8;
    } else {
        return false;
    }

    QByteArray formType = data.mid(formTypeOffset, 4);
    if (formType == "DJVU") {
        if (isMultiPage) *isMultiPage = false;
        return true;
    } else if (formType == "DJVM") {
        if (isMultiPage) *isMultiPage = true;
        return true;
    } else if (formType == "PM44" || formType == "BM44") {
        // Photo / B&W DjVu page forms
        if (isMultiPage) *isMultiPage = false;
        return true;
    }

    return false;
}

QVector<DjVuParser::Chunk> DjVuParser::parseChunks(const QByteArray& data, qsizetype offset, qsizetype maxLen) {
    QVector<Chunk> chunks;
    qsizetype current = offset;
    const qsizetype endPos = std::min<qsizetype>(data.size(), offset + maxLen);

    while (current + 8 <= endPos) {
        Chunk chunk;
        chunk.id = data.mid(current, 4);
        chunk.length = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(data.constData() + current + 4));

        qsizetype dataStart = current + 8;
        if (dataStart + chunk.length > endPos) {
            // Truncated chunk or file boundary reached
            chunk.length = static_cast<uint32_t>(std::max<qsizetype>(0, endPos - dataStart));
        }

        if (chunk.id == "FORM") {
            // Nested IFF FORM container
            if (chunk.length >= 4) {
                chunk.formType = data.mid(dataStart, 4);
                // Parse nested child chunks
                chunk.children = parseChunks(data, dataStart + 4, chunk.length - 4);
            }
            chunk.data = data.mid(dataStart, chunk.length);
        } else {
            chunk.data = data.mid(dataStart, chunk.length);
        }

        chunks.append(chunk);

        // IFF chunks are padded to even 2-byte boundaries
        qsizetype paddedLen = chunk.length + (chunk.length & 1);
        current = dataStart + paddedLen;
    }

    return chunks;
}

void DjVuParser::findChunksById(const QVector<Chunk>& chunks, const QByteArray& id, QVector<Chunk>& result) {
    for (const auto& ch : chunks) {
        if (ch.id == id) {
            result.append(ch);
        }
        if (!ch.children.isEmpty()) {
            findChunksById(ch.children, id, result);
        }
    }
}

DjVuParser::PageInfo DjVuParser::parseInfoChunk(const QByteArray& chunkData) {
    PageInfo info;
    if (chunkData.size() < 4) {
        return info;
    }

    const auto* p = reinterpret_cast<const uchar*>(chunkData.constData());
    info.width = qFromBigEndian<uint16_t>(p);
    info.height = qFromBigEndian<uint16_t>(p + 2);

    if (chunkData.size() >= 5) info.minorVersion = p[4];
    if (chunkData.size() >= 6) info.majorVersion = p[5];

    if (chunkData.size() >= 8) {
        uint16_t rawDpi = qFromBigEndian<uint16_t>(p + 6);
        // In DjVu INFO chunk, lower 12 bits store DPI, upper bits are flags
        info.dpi = rawDpi & 0x0FFF;
        if (info.dpi == 0) {
            info.dpi = 300; // Default print resolution
        }
    }

    if (chunkData.size() >= 9) info.gamma = p[8];
    if (chunkData.size() >= 10) info.flags = p[9];

    return info;
}

void DjVuParser::skipWhitespaceAndComments(const QString& input, qsizetype& pos) {
    while (pos < input.size()) {
        QChar c = input[pos];
        if (c.isSpace()) {
            pos++;
        } else if (c == ';') {
            // Lisp comment to end of line
            while (pos < input.size() && input[pos] != '\n' && input[pos] != '\r') {
                pos++;
            }
        } else {
            break;
        }
    }
}

QString DjVuParser::parseQuotedString(const QString& input, qsizetype& pos) {
    QString result;
    if (pos >= input.size() || input[pos] != '"') {
        return result;
    }
    pos++; // Skip opening quote

    while (pos < input.size()) {
        QChar c = input[pos++];
        if (c == '"') {
            break; // Closing quote
        } else if (c == '\\' && pos < input.size()) {
            QChar next = input[pos++];
            if (next == 'n') result.append('\n');
            else if (next == 'r') result.append('\r');
            else if (next == 't') result.append('\t');
            else if (next == '"') result.append('"');
            else if (next == '\\') result.append('\\');
            else if (next >= '0' && next <= '7') {
                // Octal escape \ooo
                int oct = next.toLatin1() - '0';
                for (int k = 0; k < 2 && pos < input.size() && input[pos] >= '0' && input[pos] <= '7'; ++k) {
                    oct = (oct << 3) + (input[pos++].toLatin1() - '0');
                }
                result.append(QChar(oct & 0xFF));
            } else {
                result.append(next);
            }
        } else {
            result.append(c);
        }
    }
    return result;
}

QString DjVuParser::parseSymbol(const QString& input, qsizetype& pos) {
    qsizetype start = pos;
    while (pos < input.size()) {
        QChar c = input[pos];
        if (c.isSpace() || c == '(' || c == ')' || c == '"' || c == ';') {
            break;
        }
        pos++;
    }
    return input.mid(start, pos - start);
}

DjVuParser::SExpNode DjVuParser::parseSExpression(const QString& input, qsizetype& pos) {
    skipWhitespaceAndComments(input, pos);
    SExpNode node;
    if (pos >= input.size()) {
        return node;
    }

    if (input[pos] == '(') {
        node.type = SExpNode::Type::List;
        pos++; // Skip '('
        while (pos < input.size()) {
            skipWhitespaceAndComments(input, pos);
            if (pos < input.size() && input[pos] == ')') {
                pos++; // Skip ')'
                break;
            }
            node.children.append(parseSExpression(input, pos));
        }
    } else if (input[pos] == '"') {
        node.type = SExpNode::Type::String;
        node.value = parseQuotedString(input, pos);
    } else {
        node.type = SExpNode::Type::Symbol;
        node.value = parseSymbol(input, pos);
    }

    return node;
}

QString DjVuParser::extractTextFromTXTa(const QByteArray& chunkData) {
    if (chunkData.isEmpty()) {
        return {};
    }

    // DjVu TXTa contains an S-expression structure:
    // (page x0 y0 x1 y1 (para ... (line ... (word ... "text") ) ) )
    QString input = QString::fromUtf8(chunkData);
    if (input.contains(QChar::ReplacementCharacter)) {
        input = QString::fromLatin1(chunkData);
    }

    qsizetype pos = 0;
    SExpNode root = parseSExpression(input, pos);

    QString extracted;
    auto traverseTextNode = [&](auto& self, const SExpNode& n) -> void {
        if (n.type == SExpNode::Type::String) {
            if (!n.value.isEmpty()) {
                extracted.append(n.value).append(' ');
            }
            return;
        }

        if (n.type == SExpNode::Type::List) {
            // Check symbol name if present
            QString listType;
            if (!n.children.isEmpty() && n.children[0].type == SExpNode::Type::Symbol) {
                listType = n.children[0].value.toLower();
            }

            for (const auto& child : n.children) {
                self(self, child);
            }

            if (listType == "line") {
                extracted.append('\n');
            } else if (listType == "para" || listType == "region") {
                extracted.append("\n\n");
            }
        }
    };

    traverseTextNode(traverseTextNode, root);

    // If S-expression traversal produced no strings, fallback to regex search for quotes or raw text
    if (extracted.trimmed().isEmpty()) {
        static const QRegularExpression quoteRegex(QString::fromUtf8("\"((?:[^\"\\\\]|\\\\.)*)\""));
        auto it = quoteRegex.globalMatch(input);
        while (it.hasNext()) {
            qsizetype p = 0;
            QString quoted = it.next().captured(0);
            extracted.append(parseQuotedString(quoted, p)).append(' ');
        }
    }

    if (extracted.trimmed().isEmpty()) {
        extracted = input.trimmed();
    }

    return extracted.trimmed();
}

QVector<TocItem> DjVuParser::buildTocFromSExp(const QVector<SExpNode>& bookmarkNodes, int level) {
    QVector<TocItem> items;

    for (const auto& node : bookmarkNodes) {
        if (node.type != SExpNode::Type::List || node.children.isEmpty()) {
            continue;
        }

        TocItem item;
        item.level = level;

        // 1st element: Bookmark title
        if (node.children.size() >= 1 && (node.children[0].type == SExpNode::Type::String ||
                                          node.children[0].type == SExpNode::Type::Symbol)) {
            item.title = node.children[0].value;
        }

        // 2nd element: URL or Page target (e.g. "#1", "#p0001", "page_1.djvu", "1")
        if (node.children.size() >= 2 && (node.children[1].type == SExpNode::Type::String ||
                                          node.children[1].type == SExpNode::Type::Symbol)) {
            item.href = node.children[1].value;

            // Parse destination page number
            QString target = item.href;
            if (target.startsWith('#')) target = target.mid(1);
            if (target.startsWith('p') || target.startsWith('P')) target = target.mid(1);

            static const QRegularExpression numRegex(QStringLiteral(R"(\d+)"));
            auto m = numRegex.match(target);
            if (m.hasMatch()) {
                int pageNum = m.captured(0).toInt();
                // Convert 1-based page number to 0-based index
                item.pageIndex = std::max(0, pageNum - 1);
                item.chapterIndex = item.pageIndex;
            }
        }

        item.id = QStringLiteral("toc_djvu_%1_%2").arg(level).arg(items.size());

        // 3rd and subsequent elements: child bookmark items
        QVector<SExpNode> childNodes;
        for (int i = 2; i < node.children.size(); ++i) {
            if (node.children[i].type == SExpNode::Type::List) {
                childNodes.append(node.children[i]);
            }
        }

        if (!childNodes.isEmpty()) {
            item.children = buildTocFromSExp(childNodes, level + 1);
        }

        if (!item.title.isEmpty()) {
            items.append(item);
        }
    }

    return items;
}

QVector<TocItem> DjVuParser::parseBookmarksSExp(const QByteArray& sexpData) {
    if (sexpData.isEmpty()) {
        return {};
    }

    QString input = QString::fromUtf8(sexpData);
    if (input.contains(QChar::ReplacementCharacter)) {
        input = QString::fromLatin1(sexpData);
    }

    qsizetype pos = 0;
    SExpNode root = parseSExpression(input, pos);

    // Bookmarks S-expression is typically: (bookmarks ("Title 1" "#1" ...) ("Title 2" "#5" ...))
    if (root.type == SExpNode::Type::List) {
        if (!root.children.isEmpty() && root.children[0].type == SExpNode::Type::Symbol &&
            root.children[0].value.toLower() == "bookmarks") {
            // Children from index 1 onward are bookmark list entries
            QVector<SExpNode> entries;
            for (int i = 1; i < root.children.size(); ++i) {
                entries.append(root.children[i]);
            }
            return buildTocFromSExp(entries, 0);
        } else {
            // List of bookmarks directly
            return buildTocFromSExp(root.children, 0);
        }
    }

    return {};
}

void DjVuParser::extractMetadataFromSExp(const SExpNode& root, QString& title, QString& author) {
    if (root.type != SExpNode::Type::List) {
        return;
    }

    // Check if this list is a key-value pair: e.g. (title "...") or (author "...")
    if (root.children.size() >= 2 && root.children[0].type == SExpNode::Type::Symbol) {
        QString key = root.children[0].value.toLower();
        QString val;
        if (root.children[1].type == SExpNode::Type::String || root.children[1].type == SExpNode::Type::Symbol) {
            val = root.children[1].value;
        }

        if (key == "title" && title.isEmpty()) {
            title = val;
        } else if ((key == "author" || key == "creator") && author.isEmpty()) {
            author = val;
        }
    }

    for (const auto& child : root.children) {
        extractMetadataFromSExp(child, title, author);
    }
}

std::optional<BookDocument> DjVuParser::parseBytes(const QByteArray& data,
                                                  const QString& format,
                                                  const QString& filePath) {
    Q_UNUSED(format);

    // 1. Validate AT&T / FORM magic bytes
    bool isMultiPage = false;
    if (!validateMagic(data, &isMultiPage)) {
        return std::nullopt;
    }

    // 2. Determine offset to start parsing top-level IFF chunks
    qsizetype startOffset = 0;
    if (data.startsWith("AT&TFORM")) {
        startOffset = 4; // Start at "FORM"
    } else if (data.startsWith("FORM")) {
        startOffset = 0;
    }

    // 3. Parse all IFF chunks in the file
    QVector<Chunk> rootChunks = parseChunks(data, startOffset, data.size() - startOffset);
    if (rootChunks.isEmpty()) {
        return std::nullopt;
    }

    // 4. Locate Multi-page DJVM or Single-page DJVU container
    const Chunk* mainForm = &rootChunks[0];
    if (mainForm->id != "FORM") {
        for (const auto& ch : rootChunks) {
            if (ch.id == "FORM") {
                mainForm = &ch;
                break;
            }
        }
    }

    // Collect all page FORM:DJVU chunks
    QVector<Chunk> pageChunks;
    if (mainForm->formType == "DJVM") {
        for (const auto& child : mainForm->children) {
            if (child.id == "FORM" && (child.formType == "DJVU" || child.formType == "PM44" || child.formType == "BM44")) {
                pageChunks.append(child);
            }
        }
    } else if (mainForm->formType == "DJVU" || mainForm->formType == "PM44" || mainForm->formType == "BM44") {
        pageChunks.append(*mainForm);
    }

    // Fallback: search recursively for all FORM:DJVU containers
    if (pageChunks.isEmpty()) {
        for (const auto& ch : rootChunks) {
            if (ch.id == "FORM" && ch.formType == "DJVU") {
                pageChunks.append(ch);
            }
        }
    }

    // If still empty, treat the whole document as a single page
    if (pageChunks.isEmpty() && !rootChunks.isEmpty()) {
        pageChunks.append(rootChunks[0]);
    }

    // 5. Extract metadata from NAVI and ANNa chunks
    QString docTitle;
    QString docAuthor;
    QVector<TocItem> toc;

    // Search for NAVI chunk (navigation / bookmarks)
    QVector<Chunk> naviChunks;
    findChunksById(rootChunks, "NAVI", naviChunks);
    for (const auto& ch : naviChunks) {
        auto parsedToc = parseBookmarksSExp(ch.data);
        if (!parsedToc.isEmpty()) {
            toc = parsedToc;
            break;
        }
    }

    // Search for ANNa / ANNz chunks (annotations & metadata)
    QVector<Chunk> annaChunks;
    findChunksById(rootChunks, "ANNa", annaChunks);
    for (const auto& ch : annaChunks) {
        QString text = QString::fromUtf8(ch.data);
        qsizetype pos = 0;
        SExpNode annNode = parseSExpression(text, pos);

        // Extract Title and Author
        extractMetadataFromSExp(annNode, docTitle, docAuthor);

        // If NAVI was missing, bookmarks may be inside ANNa
        if (toc.isEmpty()) {
            toc = parseBookmarksSExp(ch.data);
        }
    }

    // Fallback metadata defaults
    QString fallbackTitle = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : QStringLiteral("DjVu Document");
    if (docTitle.trimmed().isEmpty()) {
        docTitle = fallbackTitle;
    }
    if (docAuthor.trimmed().isEmpty()) {
        docAuthor = QStringLiteral("Unknown Author");
    }

    // 6. Map TOC items to page indices
    std::unordered_map<int, QString> pageToTitleMap;
    auto mapTocEntries = [&](auto& self, const QVector<TocItem>& items) -> void {
        for (const auto& item : items) {
            if (item.pageIndex >= 0 && !pageToTitleMap.contains(item.pageIndex)) {
                pageToTitleMap[item.pageIndex] = item.title;
            }
            self(self, item.children);
        }
    };
    mapTocEntries(mapTocEntries, toc);

    // 7. Extract pages and create ChapterContent list
    QVector<ChapterContent> chapters;
    chapters.reserve(pageChunks.size());

    for (int i = 0; i < pageChunks.size(); ++i) {
        const auto& pageForm = pageChunks[i];

        // Find INFO chunk for page dimensions and DPI
        QVector<Chunk> infoList;
        findChunksById(pageForm.children, "INFO", infoList);
        PageInfo info;
        if (!infoList.isEmpty()) {
            info = parseInfoChunk(infoList[0].data);
        } else if (pageForm.data.size() >= 10 && pageForm.id == "INFO") {
            info = parseInfoChunk(pageForm.data);
        }

        // Find TXTa chunk for OCR / text layer
        QVector<Chunk> txtList;
        findChunksById(pageForm.children, "TXTa", txtList);
        QString pageText;
        if (!txtList.isEmpty()) {
            pageText = extractTextFromTXTa(txtList[0].data);
        }

        QString pageTitle = pageToTitleMap.contains(i)
                                ? pageToTitleMap[i]
                                : QStringLiteral("Page %1").arg(i + 1);

        if (pageText.isEmpty()) {
            pageText = QStringLiteral("DjVu Page %1\n\n[Dimensions: %2x%3 pixels @ %4 DPI]")
                           .arg(i + 1)
                           .arg(info.width > 0 ? info.width : 2400)
                           .arg(info.height > 0 ? info.height : 3300)
                           .arg(info.dpi > 0 ? info.dpi : 300);
        }

        ChapterContent chapter;
        chapter.index = i;
        chapter.title = pageTitle;
        chapter.textContent = pageText;
        chapter.htmlContent = QStringLiteral("<h2>%1</h2><p>%2</p>")
                                  .arg(pageTitle.toHtmlEscaped())
                                  .arg(pageText.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br/>")));
        chapter.isImagePage = false;

        chapters.append(chapter);
    }

    // If TOC was empty, build a default page-based TOC
    if (toc.isEmpty()) {
        for (int i = 0; i < chapters.size(); ++i) {
            TocItem item;
            item.id = QStringLiteral("toc_djvu_page_%1").arg(i);
            item.title = QStringLiteral("Page %1").arg(i + 1);
            item.chapterIndex = i;
            item.pageIndex = i;
            item.level = 0;
            toc.append(item);
        }
    }

    // 8. Assemble BookDocument
    BookDocument doc;
    doc.id = HashUtils::calculateBytesMd5(data);
    doc.title = docTitle;
    doc.author = docAuthor;
    doc.format = QStringLiteral("djvu");
    doc.totalChapters = static_cast<int>(chapters.size());
    doc.chapters = std::move(chapters);
    doc.toc = std::move(toc);

    return doc;
}

} // namespace LunaRead
