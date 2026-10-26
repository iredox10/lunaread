#include "PdfParser.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QtEndian>
#include <zlib.h>
#include <cstring>
#include <algorithm>

namespace LunaRead {

bool PdfParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == QStringLiteral("pdf");
}

std::optional<BookDocument> PdfParser::parseFile(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return std::nullopt;
    }

    // High performance authentic extraction using pdftotext
    QString pdftotext = QStandardPaths::findExecutable("pdftotext");
    if (!pdftotext.isEmpty()) {
        QProcess proc;
        proc.start(pdftotext, QStringList() << "-layout" << filePath << "-");
        if (proc.waitForFinished(15000)) {
            QByteArray out = proc.readAllStandardOutput();
            if (!out.isEmpty()) {
                QList<QByteArray> rawPages = out.split('\x0c');
                if (!rawPages.isEmpty() && rawPages.last().trimmed().isEmpty()) {
                    rawPages.removeLast();
                }

                if (!rawPages.isEmpty()) {
                    QFileInfo info(filePath);
                    BookDocument doc;
                    doc.id = HashUtils::calculateFileMd5(filePath);
                    doc.title = info.completeBaseName();
                    doc.author = "Unknown Author";
                    doc.format = "pdf";

                    // Try to get title & author from pdfinfo
                    QString pdfinfo = QStandardPaths::findExecutable("pdfinfo");
                    if (!pdfinfo.isEmpty()) {
                        QProcess infoProc;
                        infoProc.start(pdfinfo, QStringList() << filePath);
                        if (infoProc.waitForFinished(3000)) {
                            QString infoText = QString::fromUtf8(infoProc.readAllStandardOutput());
                            for (const QString& line : infoText.split('\n')) {
                                if (line.startsWith("Title:") && line.mid(6).trimmed().length() > 2) {
                                    doc.title = line.mid(6).trimmed();
                                } else if (line.startsWith("Author:") && line.mid(7).trimmed().length() > 2) {
                                    doc.author = line.mid(7).trimmed();
                                }
                            }
                        }
                    }

                    for (int i = 0; i < rawPages.size(); ++i) {
                        QString pageStr = QString::fromUtf8(rawPages[i]);
                        ChapterContent c;
                        c.index = i;
                        c.title = QString("Page %1").arg(i + 1);
                        c.textContent = pageStr.trimmed();
                        if (c.textContent.isEmpty()) {
                            c.textContent = QString("Page %1").arg(i + 1);
                        }
                        c.htmlContent = QString("<pre>%1</pre>").arg(pageStr.toHtmlEscaped());
                        doc.chapters.append(c);

                        TocItem tocItem;
                        tocItem.id = QString("toc_%1").arg(i);
                        tocItem.title = c.title;
                        tocItem.chapterIndex = i;
                        tocItem.pageIndex = i;
                        doc.toc.append(tocItem);
                    }
                    doc.totalChapters = static_cast<int>(doc.chapters.size());

                    // Extract cover image using pdftoppm if available
                    QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
                    if (!pdftoppm.isEmpty()) {
                        QString coverBase = QDir::tempPath() + QString("/lunaread_cov_%1").arg(doc.id);
                        QProcess ppmProc;
                        ppmProc.start(pdftoppm, QStringList() << "-png" << "-r" << "100" << "-f" << "1" << "-l" << "1" << filePath << coverBase);
                        if (ppmProc.waitForFinished(5000)) {
                            QString coverFile = coverBase + "-001.png";
                            if (!QFile::exists(coverFile)) coverFile = coverBase + "-1.png";
                            if (QFile::exists(coverFile)) {
                                QFile f(coverFile);
                                if (f.open(QIODevice::ReadOnly)) {
                                    doc.coverImageBytes = f.readAll();
                                    if (!doc.chapters.isEmpty()) {
                                        doc.chapters[0].isImagePage = true;
                                        doc.chapters[0].imageBytes = doc.coverImageBytes;
                                    }
                                }
                                QFile::remove(coverFile);
                            }
                        }
                    }

                    return doc;
                }
            }
        }
    }

    // Fallback to internal parser
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), QStringLiteral("pdf"), filePath);
}

bool PdfParser::validateHeader(const QByteArray& data, QString* outVersion) {
    if (data.size() < 8) {
        return false;
    }

    // PDF header %PDF- usually starts at offset 0, but per spec may occur within first 1024 bytes
    const qsizetype searchLimit = std::min<qsizetype>(data.size(), 1024);
    const qsizetype magicPos = data.left(searchLimit).indexOf("%PDF-");
    if (magicPos < 0) {
        return false;
    }

    if (outVersion) {
        qsizetype verStart = magicPos + 5;
        qsizetype verEnd = verStart;
        while (verEnd < data.size() && (data[verEnd] == '.' || (data[verEnd] >= '0' && data[verEnd] <= '9'))) {
            verEnd++;
        }
        if (verEnd > verStart) {
            *outVersion = QString::fromLatin1(data.mid(verStart, verEnd - verStart));
        } else {
            *outVersion = QStringLiteral("1.4");
        }
    }

    return true;
}

QString PdfParser::decodePdfString(const QByteArray& raw) {
    QByteArray trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QByteArray rawBytes;

    if (trimmed.startsWith('<') && trimmed.endsWith('>')) {
        // Hexadecimal string: <48656C6C6F>
        QByteArray hexContent = trimmed.mid(1, trimmed.size() - 2).trimmed();
        // PDF spec: if hex string has an odd number of digits, append trailing '0'
        if (hexContent.size() % 2 != 0) {
            hexContent.append('0');
        }
        rawBytes = QByteArray::fromHex(hexContent);
    } else if (trimmed.startsWith('(') && trimmed.endsWith(')')) {
        // Literal string: (Hello \(World\))
        const char* p = trimmed.constData() + 1;
        const qsizetype len = trimmed.size() - 2;
        qsizetype i = 0;
        while (i < len) {
            char c = p[i++];
            if (c == '\\' && i < len) {
                char next = p[i++];
                switch (next) {
                    case 'n': rawBytes.append('\n'); break;
                    case 'r': rawBytes.append('\r'); break;
                    case 't': rawBytes.append('\t'); break;
                    case 'b': rawBytes.append('\b'); break;
                    case 'f': rawBytes.append('\f'); break;
                    case '(': rawBytes.append('('); break;
                    case ')': rawBytes.append(')'); break;
                    case '\\': rawBytes.append('\\'); break;
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7': {
                        // Octal character escape (1 to 3 octal digits)
                        int octalVal = next - '0';
                        if (i < len && p[i] >= '0' && p[i] <= '7') {
                            octalVal = (octalVal << 3) + (p[i++] - '0');
                            if (i < len && p[i] >= '0' && p[i] <= '7') {
                                octalVal = (octalVal << 3) + (p[i++] - '0');
                            }
                        }
                        rawBytes.append(static_cast<char>(octalVal & 0xFF));
                        break;
                    }
                    default:
                        rawBytes.append(next);
                        break;
                }
            } else {
                rawBytes.append(c);
            }
        }
    } else {
        rawBytes = trimmed;
    }

    if (rawBytes.isEmpty()) {
        return {};
    }

    // Check for UTF-16BE BOM: 0xFE, 0xFF
    if (rawBytes.size() >= 2 && static_cast<unsigned char>(rawBytes[0]) == 0xFE && static_cast<unsigned char>(rawBytes[1]) == 0xFF) {
        qsizetype charCount = (rawBytes.size() - 2) / 2;
        QString result;
        result.reserve(static_cast<int>(charCount));
        const auto* ptr = reinterpret_cast<const uchar*>(rawBytes.constData() + 2);
        for (qsizetype k = 0; k < charCount; ++k) {
            ushort ch = qFromBigEndian<ushort>(ptr + (k * 2));
            result.append(QChar(ch));
        }
        return result;
    }

    // Check for UTF-8 BOM: 0xEF, 0xBB, 0xBF
    if (rawBytes.size() >= 3 && static_cast<unsigned char>(rawBytes[0]) == 0xEF &&
        static_cast<unsigned char>(rawBytes[1]) == 0xBB && static_cast<unsigned char>(rawBytes[2]) == 0xBF) {
        return QString::fromUtf8(rawBytes.constData() + 3, rawBytes.size() - 3);
    }

    // Try UTF-8 first; if valid, return UTF-8
    QString utf8Candidate = QString::fromUtf8(rawBytes);
    if (!utf8Candidate.contains(QChar::ReplacementCharacter)) {
        return utf8Candidate;
    }

    // Fallback to Latin1 / PDFDocEncoding
    return QString::fromLatin1(rawBytes);
}

QString PdfParser::decodePdfName(const QByteArray& raw) {
    QByteArray name = raw.trimmed();
    if (name.startsWith('/')) {
        name = name.mid(1);
    }

    QByteArray decoded;
    qsizetype i = 0;
    while (i < name.size()) {
        if (name[i] == '#' && i + 2 < name.size()) {
            bool ok = false;
            int hex = name.mid(i + 1, 2).toInt(&ok, 16);
            if (ok) {
                decoded.append(static_cast<char>(hex));
                i += 3;
                continue;
            }
        }
        decoded.append(name[i++]);
    }
    return QString::fromLatin1(decoded);
}

QByteArray PdfParser::decompressFlate(const QByteArray& compressed) {
    if (compressed.isEmpty()) {
        return {};
    }

    // Use zlib inflate to decompress standard RFC 1950 zlib / RFC 1951 raw deflate data
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    // 32 + MAX_WBITS enables zlib or gzip header detection
    if (inflateInit2(&strm, 32 + MAX_WBITS) != Z_OK) {
        if (inflateInit(&strm) != Z_OK) {
            return {};
        }
    }

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    strm.avail_in = static_cast<uInt>(compressed.size());

    QByteArray decompressed;
    char buffer[16384];
    int ret = Z_OK;

    while (ret == Z_OK) {
        strm.next_out = reinterpret_cast<Bytef*>(buffer);
        strm.avail_out = sizeof(buffer);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_OK || ret == Z_STREAM_END) {
            int have = sizeof(buffer) - static_cast<int>(strm.avail_out);
            decompressed.append(buffer, have);
        }
    }
    inflateEnd(&strm);

    // If standard inflate failed, try raw inflate without zlib header (-MAX_WBITS)
    if (ret != Z_STREAM_END && decompressed.isEmpty()) {
        std::memset(&strm, 0, sizeof(strm));
        if (inflateInit2(&strm, -MAX_WBITS) == Z_OK) {
            strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
            strm.avail_in = static_cast<uInt>(compressed.size());
            ret = Z_OK;
            while (ret == Z_OK) {
                strm.next_out = reinterpret_cast<Bytef*>(buffer);
                strm.avail_out = sizeof(buffer);
                ret = inflate(&strm, Z_NO_FLUSH);
                if (ret == Z_OK || ret == Z_STREAM_END) {
                    int have = sizeof(buffer) - static_cast<int>(strm.avail_out);
                    decompressed.append(buffer, have);
                }
            }
            inflateEnd(&strm);
        }
    }

    return decompressed;
}

std::unordered_map<int, qsizetype> PdfParser::buildObjectIndex(const QByteArray& data) const {
    std::unordered_map<int, qsizetype> objMap;

    // Fast regex scanning for indirect objects: "<obj_id> <gen> obj"
    static const QRegularExpression objRegex(QStringLiteral(R"((\d+)\s+(\d+)\s+obj\b)"));
    QString content = QString::fromLatin1(data);
    auto it = objRegex.globalMatch(content);
    while (it.hasNext()) {
        auto match = it.next();
        int objId = match.captured(1).toInt();
        qsizetype offset = static_cast<qsizetype>(match.capturedStart());
        // If object was updated in an incremental update, the later offset will take precedence
        objMap[objId] = offset;
    }

    return objMap;
}

int PdfParser::parseIndirectRef(const QByteArray& value) {
    static const QRegularExpression refRegex(QStringLiteral(R"((\d+)\s+\d+\s+R)"));
    auto match = refRegex.match(QString::fromLatin1(value));
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    return -1;
}

QByteArray PdfParser::getObjectData(const QByteArray& data,
                                   const std::unordered_map<int, qsizetype>& objMap,
                                   int objNum) const {
    auto it = objMap.find(objNum);
    if (it == objMap.end()) {
        return {};
    }

    qsizetype startPos = it->second;
    qsizetype endPos = data.indexOf("endobj", startPos);
    if (endPos < 0) {
        endPos = data.size();
    }

    return data.mid(startPos, endPos - startPos);
}

QByteArray PdfParser::findDictValue(const QByteArray& dictBytes, const QByteArray& key) const {
    qsizetype keyPos = dictBytes.indexOf(key);
    while (keyPos >= 0) {
        // Verify keyPos is preceded by delimiter or slash
        if (keyPos == 0 || dictBytes[keyPos - 1] == '/' || dictBytes[keyPos - 1] == '<' ||
            dictBytes[keyPos - 1] == ' ' || dictBytes[keyPos - 1] == '\n' || dictBytes[keyPos - 1] == '\r') {
            
            // Check character following key
            qsizetype valPos = keyPos + key.size();
            if (valPos < dictBytes.size() && (dictBytes[valPos] == ' ' || dictBytes[valPos] == '\r' ||
                                              dictBytes[valPos] == '\n' || dictBytes[valPos] == '/' ||
                                              dictBytes[valPos] == '(' || dictBytes[valPos] == '<' ||
                                              dictBytes[valPos] == '[')) {
                // Skip leading whitespace
                while (valPos < dictBytes.size() && (dictBytes[valPos] == ' ' || dictBytes[valPos] == '\r' || dictBytes[valPos] == '\n')) {
                    valPos++;
                }

                if (valPos >= dictBytes.size()) return {};

                // Parse value depending on type
                char startChar = dictBytes[valPos];
                if (startChar == '(') {
                    // String literal with nested parenthesis handling
                    int depth = 1;
                    qsizetype p = valPos + 1;
                    while (p < dictBytes.size() && depth > 0) {
                        if (dictBytes[p] == '\\') {
                            p += 2;
                            continue;
                        }
                        if (dictBytes[p] == '(') depth++;
                        else if (dictBytes[p] == ')') depth--;
                        p++;
                    }
                    return dictBytes.mid(valPos, p - valPos);
                } else if (startChar == '<' && valPos + 1 < dictBytes.size() && dictBytes[valPos + 1] != '<') {
                    // Hexadecimal string <...>
                    qsizetype endP = dictBytes.indexOf('>', valPos);
                    if (endP >= 0) {
                        return dictBytes.mid(valPos, endP - valPos + 1);
                    }
                } else if (startChar == '<' && valPos + 1 < dictBytes.size() && dictBytes[valPos + 1] == '<') {
                    // Nested dictionary << ... >>
                    int depth = 1;
                    qsizetype p = valPos + 2;
                    while (p + 1 < dictBytes.size() && depth > 0) {
                        if (dictBytes[p] == '<' && dictBytes[p + 1] == '<') {
                            depth++;
                            p += 2;
                        } else if (dictBytes[p] == '>' && dictBytes[p + 1] == '>') {
                            depth--;
                            p += 2;
                        } else {
                            p++;
                        }
                    }
                    return dictBytes.mid(valPos, p - valPos);
                } else if (startChar == '[') {
                    // Array [ ... ]
                    int depth = 1;
                    qsizetype p = valPos + 1;
                    while (p < dictBytes.size() && depth > 0) {
                        if (dictBytes[p] == '[') depth++;
                        else if (dictBytes[p] == ']') depth--;
                        p++;
                    }
                    return dictBytes.mid(valPos, p - valPos);
                } else {
                    // Name, number, or indirect reference
                    qsizetype p = valPos;
                    while (p < dictBytes.size() && dictBytes[p] != '/' && dictBytes[p] != '>' &&
                           dictBytes[p] != ']' && dictBytes[p] != ')' && dictBytes[p] != '\r' && dictBytes[p] != '\n') {
                        p++;
                    }
                    return dictBytes.mid(valPos, p - valPos).trimmed();
                }
            }
        }
        keyPos = dictBytes.indexOf(key, keyPos + 1);
    }
    return {};
}

bool PdfParser::parseTrailerAndXRef(const QByteArray& data,
                                   const std::unordered_map<int, qsizetype>& objMap,
                                   TrailerInfo& trailer) const {
    Q_UNUSED(objMap);

    // Locate "startxref" near the end of the file
    qsizetype searchStart = std::max<qsizetype>(0, data.size() - 4096);
    qsizetype startxrefPos = data.lastIndexOf("startxref", data.size() - 1);
    if (startxrefPos < searchStart && startxrefPos >= 0) {
        // found before searchStart
    }
    if (startxrefPos >= 0) {
        qsizetype p = startxrefPos + 9;
        while (p < data.size() && (data[p] == ' ' || data[p] == '\r' || data[p] == '\n')) {
            p++;
        }
        qsizetype numStart = p;
        while (p < data.size() && data[p] >= '0' && data[p] <= '9') {
            p++;
        }
        if (p > numStart) {
            trailer.startxref = data.mid(numStart, p - numStart).toLongLong();
        }
    }

    // Locate "trailer" dictionary
    qsizetype trailerPos = data.lastIndexOf("trailer", data.size() - 1);
    QByteArray trailerDict;
    if (trailerPos >= 0) {
        qsizetype dictStart = data.indexOf("<<", trailerPos);
        if (dictStart >= 0) {
            qsizetype dictEnd = data.indexOf(">>", dictStart);
            if (dictEnd >= 0) {
                trailerDict = data.mid(dictStart, dictEnd - dictStart + 2);
            }
        }
    }

    // If standard trailer wasn't found (e.g. PDF 1.5+ XRef stream), search for any << /Root ... >>
    if (trailerDict.isEmpty()) {
        static const QRegularExpression rootSearch(QStringLiteral(R"(<<[^>]*?/Root\s+(\d+)\s+\d+\s+R[^>]*?>>)"));
        auto match = rootSearch.match(QString::fromLatin1(data));
        if (match.hasMatch()) {
            trailerDict = match.captured(0).toLatin1();
        }
    }

    if (!trailerDict.isEmpty()) {
        QByteArray rootVal = findDictValue(trailerDict, "/Root");
        trailer.rootObjNum = parseIndirectRef(rootVal);

        QByteArray infoVal = findDictValue(trailerDict, "/Info");
        trailer.infoObjNum = parseIndirectRef(infoVal);

        QByteArray sizeVal = findDictValue(trailerDict, "/Size");
        if (!sizeVal.isEmpty()) {
            trailer.size = sizeVal.toInt();
        }

        QByteArray prevVal = findDictValue(trailerDict, "/Prev");
        if (!prevVal.isEmpty()) {
            trailer.prev = prevVal.toInt();
        }
    }

    return trailer.rootObjNum > 0;
}

void PdfParser::collectPagesFromNode(const QByteArray& data,
                                    const std::unordered_map<int, qsizetype>& objMap,
                                    int nodeObjNum,
                                    QVector<int>& pageObjNums,
                                    std::unordered_set<int>& visitedNodes) const {
    if (visitedNodes.contains(nodeObjNum)) {
        return; // Guard against circular structures
    }
    visitedNodes.insert(nodeObjNum);

    QByteArray nodeData = getObjectData(data, objMap, nodeObjNum);
    if (nodeData.isEmpty()) {
        return;
    }

    QByteArray typeVal = findDictValue(nodeData, "/Type");

    if (typeVal.contains("/Pages")) {
        // Node is a page tree node: parse /Kids [ ... ]
        QByteArray kidsVal = findDictValue(nodeData, "/Kids");
        static const QRegularExpression refRegex(QStringLiteral(R"((\d+)\s+\d+\s+R)"));
        auto it = refRegex.globalMatch(QString::fromLatin1(kidsVal));
        while (it.hasNext()) {
            int childObjNum = it.next().captured(1).toInt();
            collectPagesFromNode(data, objMap, childObjNum, pageObjNums, visitedNodes);
        }
    } else if (typeVal.contains("/Page") || (!typeVal.contains("/Pages") && nodeData.contains("/Parent"))) {
        // Leaf page node
        pageObjNums.append(nodeObjNum);
    }
}

QVector<int> PdfParser::scanPagesDirectly(const QByteArray& data,
                                         const std::unordered_map<int, qsizetype>& objMap) const {
    QVector<std::pair<qsizetype, int>> offsetAndObj;

    // Scan for all indirect objects containing "/Type /Page" but NOT "/Type /Pages"
    for (const auto& [objNum, offset] : objMap) {
        QByteArray objData = getObjectData(data, objMap, objNum);
        // Look for /Type /Page followed by whitespace, slash, or closing angle bracket
        static const QRegularExpression pageRegex(QStringLiteral(R"(/Type\s*/Page\b(?!\s*s))"));
        if (pageRegex.match(QString::fromLatin1(objData)).hasMatch()) {
            offsetAndObj.emplace_back(offset, objNum);
        }
    }

    // Sort page objects by file byte offset to preserve reading order
    std::sort(offsetAndObj.begin(), offsetAndObj.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    QVector<int> pageObjNums;
    pageObjNums.reserve(static_cast<int>(offsetAndObj.size()));
    for (const auto& item : offsetAndObj) {
        pageObjNums.append(item.second);
    }

    return pageObjNums;
}

int PdfParser::extractDestPageIndex(const QByteArray& itemDict,
                                    const QByteArray& data,
                                    const std::unordered_map<int, qsizetype>& objMap,
                                    const std::unordered_map<int, int>& pageObjToIndex) const {
    // 1. Direct /Dest array: e.g. /Dest [ 4 0 R /XYZ 0 792 null ]
    QByteArray destVal = findDictValue(itemDict, "/Dest");
    if (!destVal.isEmpty()) {
        int refObj = parseIndirectRef(destVal);
        if (refObj > 0 && pageObjToIndex.contains(refObj)) {
            return pageObjToIndex.at(refObj);
        }
    }

    // 2. Action dictionary /A << /S /GoTo /D [ 4 0 R ... ] >>
    QByteArray aVal = findDictValue(itemDict, "/A");
    if (!aVal.isEmpty()) {
        QByteArray actionDict = aVal;
        int actionRef = parseIndirectRef(aVal);
        if (actionRef > 0) {
            actionDict = getObjectData(data, objMap, actionRef);
        }

        QByteArray dVal = findDictValue(actionDict, "/D");
        if (!dVal.isEmpty()) {
            int refObj = parseIndirectRef(dVal);
            if (refObj > 0 && pageObjToIndex.contains(refObj)) {
                return pageObjToIndex.at(refObj);
            }
        }
    }

    return 0;
}

QVector<TocItem> PdfParser::parseOutlineSiblings(const QByteArray& data,
                                                const std::unordered_map<int, qsizetype>& objMap,
                                                int firstObjNum,
                                                int level,
                                                const std::unordered_map<int, int>& pageObjToIndex,
                                                std::unordered_set<int>& visitedOutlines) const {
    QVector<TocItem> items;
    int currentObjNum = firstObjNum;

    while (currentObjNum > 0 && !visitedOutlines.contains(currentObjNum)) {
        visitedOutlines.insert(currentObjNum);

        QByteArray itemDict = getObjectData(data, objMap, currentObjNum);
        if (itemDict.isEmpty()) {
            break;
        }

        TocItem item;
        item.id = QStringLiteral("toc_pdf_%1").arg(currentObjNum);
        item.level = level;

        // Extract title
        QByteArray titleVal = findDictValue(itemDict, "/Title");
        item.title = decodePdfString(titleVal);
        if (item.title.trimmed().isEmpty()) {
            item.title = QStringLiteral("Section %1").arg(items.size() + 1);
        }

        // Extract destination page index
        int pageIndex = extractDestPageIndex(itemDict, data, objMap, pageObjToIndex);
        item.pageIndex = pageIndex;
        item.chapterIndex = pageIndex;

        // Check for child outline items (/First)
        QByteArray childVal = findDictValue(itemDict, "/First");
        int childObjNum = parseIndirectRef(childVal);
        if (childObjNum > 0) {
            item.children = parseOutlineSiblings(data, objMap, childObjNum, level + 1, pageObjToIndex, visitedOutlines);
        }

        items.append(item);

        // Advance to next sibling (/Next)
        QByteArray nextVal = findDictValue(itemDict, "/Next");
        currentObjNum = parseIndirectRef(nextVal);
    }

    return items;
}

QString PdfParser::parseContentStreamText(const QByteArray& streamBytes) {
    QString extracted;
    qsizetype pos = 0;
    const qsizetype total = streamBytes.size();

    // Look for BT (Begin Text) to ET (End Text) blocks
    while (pos < total) {
        qsizetype btPos = streamBytes.indexOf("BT", pos);
        if (btPos < 0) {
            break;
        }

        qsizetype etPos = streamBytes.indexOf("ET", btPos);
        if (etPos < 0) {
            etPos = total;
        }

        QByteArray textBlock = streamBytes.mid(btPos, etPos - btPos);
        QString blockString = QString::fromLatin1(textBlock);

        // Extract (string) Tj
        static const QRegularExpression tjRegex(QStringLiteral(R"(\((.*?)\)\s*Tj)"));
        auto itTj = tjRegex.globalMatch(blockString);
        while (itTj.hasNext()) {
            auto m = itTj.next();
            QString t = decodePdfString("(" + m.captured(1).toLatin1() + ")");
            extracted.append(t).append(' ');
        }

        // Extract <hex> Tj
        static const QRegularExpression tjHexRegex(QStringLiteral(R"(<([0-9a-fA-F\s]+)>\s*Tj)"));
        auto itHex = tjHexRegex.globalMatch(blockString);
        while (itHex.hasNext()) {
            auto m = itHex.next();
            QString t = decodePdfString("<" + m.captured(1).toLatin1() + ">");
            extracted.append(t).append(' ');
        }

        // Extract [(array)] TJ
        static const QRegularExpression tjArrayRegex(QStringLiteral(R"(\[(.*?)\]\s*TJ)"));
        auto itArray = tjArrayRegex.globalMatch(blockString);
        while (itArray.hasNext()) {
            auto m = itArray.next();
            QString arrayContent = m.captured(1);

            static const QRegularExpression itemRegex(QStringLiteral(R"(\((.*?)\)|<([0-9a-fA-F\s]+)>)"));
            auto itItem = itemRegex.globalMatch(arrayContent);
            while (itItem.hasNext()) {
                auto mItem = itItem.next();
                if (!mItem.captured(1).isEmpty()) {
                    extracted.append(decodePdfString("(" + mItem.captured(1).toLatin1() + ")"));
                } else if (!mItem.captured(2).isEmpty()) {
                    extracted.append(decodePdfString("<" + mItem.captured(2).toLatin1() + ">"));
                }
            }
            extracted.append(' ');
        }

        // Extract (string) '
        static const QRegularExpression singleQuoteRegex(QStringLiteral(R"(\((.*?)\)\s*')"));
        auto itQuote = singleQuoteRegex.globalMatch(blockString);
        while (itQuote.hasNext()) {
            auto m = itQuote.next();
            extracted.append("\n").append(decodePdfString("(" + m.captured(1).toLatin1() + ")")).append(' ');
        }

        extracted.append("\n\n");
        pos = etPos + 2;
    }

    return extracted.trimmed();
}

QString PdfParser::extractPageText(const QByteArray& data,
                                  const std::unordered_map<int, qsizetype>& objMap,
                                  const QByteArray& pageDict) const {
    QByteArray contentsVal = findDictValue(pageDict, "/Contents");
    if (contentsVal.isEmpty()) {
        return {};
    }

    QVector<int> streamObjNums;
    static const QRegularExpression refRegex(QStringLiteral(R"((\d+)\s+\d+\s+R)"));
    auto it = refRegex.globalMatch(QString::fromLatin1(contentsVal));
    while (it.hasNext()) {
        streamObjNums.append(it.next().captured(1).toInt());
    }

    if (streamObjNums.isEmpty()) {
        int singleRef = parseIndirectRef(contentsVal);
        if (singleRef > 0) {
            streamObjNums.append(singleRef);
        }
    }

    QString pageText;
    for (int streamObjNum : streamObjNums) {
        QByteArray objData = getObjectData(data, objMap, streamObjNum);
        if (objData.isEmpty()) {
            continue;
        }

        // Find stream...endstream
        qsizetype streamStart = objData.indexOf("stream");
        if (streamStart < 0) {
            continue;
        }

        streamStart += 6;
        if (streamStart < objData.size() && objData[streamStart] == '\r') streamStart++;
        if (streamStart < objData.size() && objData[streamStart] == '\n') streamStart++;

        qsizetype streamEnd = objData.indexOf("endstream", streamStart);
        if (streamEnd < 0) {
            streamEnd = objData.size();
        }

        QByteArray rawStream = objData.mid(streamStart, streamEnd - streamStart);
        QByteArray decompressed = rawStream;

        if (objData.contains("/FlateDecode")) {
            QByteArray decomp = decompressFlate(rawStream);
            if (!decomp.isEmpty()) {
                decompressed = decomp;
            }
        }

        QString text = parseContentStreamText(decompressed);
        if (!text.isEmpty()) {
            if (!pageText.isEmpty()) pageText.append("\n\n");
            pageText.append(text);
        }
    }

    return pageText;
}

std::optional<BookDocument> PdfParser::parseBytes(const QByteArray& data,
                                                 const QString& format,
                                                 const QString& filePath) {
    Q_UNUSED(format);

    // 1. Validate %PDF- header and extract version
    QString pdfVersion;
    if (!validateHeader(data, &pdfVersion)) {
        return std::nullopt;
    }

    // 2. Build index of all indirect objects
    auto objMap = buildObjectIndex(data);
    if (objMap.empty()) {
        return std::nullopt;
    }

    // 3. Scan cross-reference (XRef) and trailer dictionary
    TrailerInfo trailer;
    parseTrailerAndXRef(data, objMap, trailer);

    // Resolve Document Information dictionary (/Info)
    QString title;
    QString author;

    if (trailer.infoObjNum > 0) {
        QByteArray infoData = getObjectData(data, objMap, trailer.infoObjNum);
        QByteArray titleVal = findDictValue(infoData, "/Title");
        if (!titleVal.isEmpty()) {
            title = decodePdfString(titleVal);
        }

        QByteArray authorVal = findDictValue(infoData, "/Author");
        if (!authorVal.isEmpty()) {
            author = decodePdfString(authorVal);
        }
    }

    // Fallback metadata search if /Info was not present or empty
    if (title.isEmpty()) {
        static const QRegularExpression titleRegex(QStringLiteral(R"(/Title\s*(\([^\)]+\)|<[0-9a-fA-F]+>))"));
        auto m = titleRegex.match(QString::fromLatin1(data));
        if (m.hasMatch()) {
            title = decodePdfString(m.captured(1).toLatin1());
        }
    }

    if (author.isEmpty()) {
        static const QRegularExpression authorRegex(QStringLiteral(R"(/Author\s*(\([^\)]+\)|<[0-9a-fA-F]+>))"));
        auto m = authorRegex.match(QString::fromLatin1(data));
        if (m.hasMatch()) {
            author = decodePdfString(m.captured(1).toLatin1());
        }
    }

    // Default metadata
    QString fallbackTitle = !filePath.isEmpty() ? QFileInfo(filePath).completeBaseName() : QStringLiteral("PDF Document");
    if (title.trimmed().isEmpty()) {
        title = fallbackTitle;
    }
    if (author.trimmed().isEmpty()) {
        author = QStringLiteral("Unknown Author");
    }

    // 4. Resolve Document Catalog (/Root) and Page Tree (/Pages)
    int catalogObjNum = trailer.rootObjNum;
    if (catalogObjNum <= 0) {
        // Fallback: locate catalog object directly
        for (const auto& [num, offset] : objMap) {
            QByteArray obj = getObjectData(data, objMap, num);
            if (obj.contains("/Type /Catalog") || obj.contains("/Type/Catalog")) {
                catalogObjNum = num;
                break;
            }
        }
    }

    QByteArray catalogDict = getObjectData(data, objMap, catalogObjNum);
    QByteArray pagesRefVal = findDictValue(catalogDict, "/Pages");
    int pagesRootObjNum = parseIndirectRef(pagesRefVal);

    // Collect pages from page tree
    QVector<int> pageObjNums;
    std::unordered_set<int> visitedNodes;
    if (pagesRootObjNum > 0) {
        collectPagesFromNode(data, objMap, pagesRootObjNum, pageObjNums, visitedNodes);
    }

    // Verify / supplement with direct /Type /Page scanning
    if (pageObjNums.isEmpty()) {
        pageObjNums = scanPagesDirectly(data, objMap);
    }

    if (pageObjNums.isEmpty()) {
        // If still no page objects resolved, create at least 1 dummy page so document is readable
        pageObjNums.append(1);
    }

    // Build page object number to 0-based page index map
    std::unordered_map<int, int> pageObjToIndex;
    for (int i = 0; i < pageObjNums.size(); ++i) {
        pageObjToIndex[pageObjNums[i]] = i;
    }

    // 5. Extract Bookmarks / Outline Tree (/Outlines -> /First -> /Next -> /Title -> /Dest)
    QVector<TocItem> toc;
    QByteArray outlinesRefVal = findDictValue(catalogDict, "/Outlines");
    int outlinesObjNum = parseIndirectRef(outlinesRefVal);
    if (outlinesObjNum > 0) {
        QByteArray outlinesDict = getObjectData(data, objMap, outlinesObjNum);
        QByteArray firstOutlineVal = findDictValue(outlinesDict, "/First");
        int firstOutlineObj = parseIndirectRef(firstOutlineVal);
        if (firstOutlineObj > 0) {
            std::unordered_set<int> visitedOutlines;
            toc = parseOutlineSiblings(data, objMap, firstOutlineObj, 0, pageObjToIndex, visitedOutlines);
        }
    }

    // Map each page to a TOC title if an outline entry exists
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

    // 6. Build Chapters (one ChapterContent per page)
    QVector<ChapterContent> chapters;
    chapters.reserve(pageObjNums.size());

    for (int i = 0; i < pageObjNums.size(); ++i) {
        int pageObjNum = pageObjNums[i];
        QByteArray pageDict = getObjectData(data, objMap, pageObjNum);

        QString pageTitle = pageToTitleMap.contains(i)
                                ? pageToTitleMap[i]
                                : QStringLiteral("Page %1").arg(i + 1);

        QString pageText = extractPageText(data, objMap, pageDict);
        if (pageText.isEmpty()) {
            pageText = QStringLiteral("Page %1\n\n[PDF Page %1 - Rendered Document Content]").arg(i + 1);
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

    // If TOC was empty, generate a flat page-based TOC
    if (toc.isEmpty()) {
        for (int i = 0; i < chapters.size(); ++i) {
            TocItem item;
            item.id = QStringLiteral("toc_page_%1").arg(i);
            item.title = QStringLiteral("Page %1").arg(i + 1);
            item.chapterIndex = i;
            item.pageIndex = i;
            item.level = 0;
            toc.append(item);
        }
    }

    // 7. Assemble BookDocument
    BookDocument doc;
    doc.id = HashUtils::calculateBytesMd5(data);
    doc.title = title;
    doc.author = author;
    doc.format = QStringLiteral("pdf");
    doc.totalChapters = static_cast<int>(chapters.size());
    doc.chapters = std::move(chapters);
    doc.toc = std::move(toc);

    return doc;
}

} // namespace LunaRead
