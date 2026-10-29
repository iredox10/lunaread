#include "PdfParser.h"
#include "../features/PdfPageService.h"
#include "../utils/HashUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QDir>
#include <QXmlStreamReader>
#include <QtEndian>
#include <zlib.h>
#include <cstring>
#include <algorithm>

namespace LunaRead {

namespace {

QString sanitizePdfLine(const QString& rawLine) {
    QString line = rawLine;
    line.remove(QChar(0x00AD)); // soft hyphen
    line.replace(QChar(0x00A0), QChar(' '));
    line.replace(QChar(0x200B), QChar(' ')); // zero-width space
    return line.trimmed();
}

QString canonicalPdfLine(const QString& line) {
    static const QRegularExpression nonAlnum(QStringLiteral(R"([^a-z0-9 ]+)"));
    QString key = line.toLower();
    key.remove(nonAlnum);
    return key.simplified();
}

bool isPageNumberLine(const QString& line) {
    static const QRegularExpression pageNumRe(
        QStringLiteral(R"(^[\s\-–—•|.]*(\d{1,4}|[ivxlcdm]{1,7})[\s\-–—•|.]*$)"),
        QRegularExpression::CaseInsensitiveOption);
    return pageNumRe.match(line).hasMatch();
}

bool looksLikeCodeLine(const QString& rawLine, bool insideCodeBlock) {
    static const QRegularExpression codeStart(
        QStringLiteral(R"(^(\$|>|#|//|/\*|\*|const\b|let\b|var\b|function\b|class\b|import\b|export\b|if\b|for\b|while\b|return\b|switch\b|await\b|async\b|\}|\{))"));
    const QString trimmed = rawLine.trimmed();
    if (trimmed.isEmpty()) return false;
    if (rawLine.size() - trimmed.size() >= 2) return true; // indented source line
    if (codeStart.match(trimmed).hasMatch()) return true;
    if (trimmed.endsWith(QLatin1Char(';')) || trimmed.endsWith(QLatin1Char('{')) ||
        trimmed.endsWith(QLatin1Char('}')) || trimmed.endsWith(QStringLiteral("=>"))) {
        return true;
    }
    if (insideCodeBlock) {
        // Short unpunctuated lines inside a code block are continuation lines
        const int words = static_cast<int>(trimmed.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts).size());
        return words <= 6 && !trimmed.endsWith(QLatin1Char('.'));
    }
    return false;
}

bool endsWithSoftHyphen(const QString& text) {
    return text.size() > 2 && text.endsWith(QLatin1Char('-')) &&
           text.at(text.size() - 2).isLetter();
}

bool isCjkChar(QChar ch) {
    const ushort u = ch.unicode();
    return (u >= 0x3000 && u <= 0x9FFF)    // CJK punctuation, kana, ideographs
        || (u >= 0xAC00 && u <= 0xD7AF)    // Hangul syllables
        || (u >= 0xF900 && u <= 0xFAFF);   // CJK compatibility ideographs
}

/// Collects short lines that repeat at page boundaries (running headers/footers)
QSet<QString> detectRunningFurniture(const QVector<QStringList>& pages) {
    QHash<QString, int> counts;
    for (const QStringList& lines : pages) {
        QStringList nonEmpty;
        nonEmpty.reserve(lines.size());
        for (const QString& raw : lines) {
            const QString line = sanitizePdfLine(raw);
            if (!line.isEmpty()) nonEmpty.append(line);
        }
        if (nonEmpty.size() < 4) continue;

        QStringList boundary = nonEmpty.mid(0, 2);
        boundary += nonEmpty.mid(std::max(0, static_cast<int>(nonEmpty.size()) - 2));

        QSet<QString> seen;
        for (const QString& line : boundary) {
            if (line.size() > 90) continue;
            const QString key = canonicalPdfLine(line);
            if (key.size() < 4 || seen.contains(key)) continue;
            seen.insert(key);
            counts[key]++;
        }
    }

    QSet<QString> furniture;
    const int threshold = std::max(4, static_cast<int>(pages.size()) / 2);
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() >= threshold) furniture.insert(it.key());
    }
    return furniture;
}

/// Unwraps a page's text layer into paragraph-separated plain text.
/// The page itself still renders as its original artwork; this text is only
/// used for search, TTS, and word counts.
QString unwrapPageText(const QStringList& rawLines, const QSet<QString>& furniture) {
    QString result;

    QVector<int> contentIndices;
    contentIndices.reserve(rawLines.size());
    for (int i = 0; i < rawLines.size(); ++i) {
        if (!sanitizePdfLine(rawLines[i]).isEmpty()) contentIndices.append(i);
    }
    if (contentIndices.isEmpty()) return result;

    QSet<int> boundary;
    const int contentCount = static_cast<int>(contentIndices.size());
    for (int k = 0; k < contentCount && k < 3; ++k) boundary.insert(contentIndices[k]);
    for (int k = std::max(0, contentCount - 3); k < contentCount; ++k) boundary.insert(contentIndices[k]);

    QStringList paragraphs;
    QString current;
    bool currentIsCode = false;

    auto flush = [&]() {
        if (!current.trimmed().isEmpty()) paragraphs.append(current);
        current.clear();
        currentIsCode = false;
    };

    for (int i = 0; i < rawLines.size(); ++i) {
        const QString line = sanitizePdfLine(rawLines[i]);
        if (line.isEmpty()) {
            flush();
            continue;
        }
        if (boundary.contains(i) && (isPageNumberLine(line) || furniture.contains(canonicalPdfLine(line)))) {
            continue;
        }

        const bool codeLine = looksLikeCodeLine(rawLines[i], currentIsCode);
        if (current.isEmpty()) {
            current = line;
            currentIsCode = codeLine;
        } else if (codeLine != currentIsCode) {
            // Keep code lines in their own block instead of welding them to prose
            flush();
            current = line;
            currentIsCode = codeLine;
        } else if (currentIsCode) {
            current += QLatin1Char('\n') + line;
        } else if (endsWithSoftHyphen(current) && line.at(0).isLower()) {
            current.chop(1);
            current += line;
        } else if (isCjkChar(current.at(current.size() - 1)) && isCjkChar(line.at(0))) {
            current += line; // CJK lines wrap without word spacing
        } else {
            current += QLatin1Char(' ') + line;
        }
    }
    flush();

    if (paragraphs.isEmpty()) return result;

    return paragraphs.join(QStringLiteral("\n\n"));
}

/// First meaningful line of a page, skipping folios and running furniture.
QString pageHeadingLine(const QStringList& rawLines, const QSet<QString>& furniture) {
    for (const QString& raw : rawLines) {
        const QString line = sanitizePdfLine(raw);
        if (line.isEmpty() || isPageNumberLine(line)) continue;
        const QString key = canonicalPdfLine(line);
        if (key.isEmpty() || furniture.contains(key)) continue;
        return line;
    }
    return {};
}

QString shortenHeading(const QString& heading) {
    constexpr qsizetype maxChars = 72;
    if (heading.size() <= maxChars) return heading;
    qsizetype cut = heading.lastIndexOf(QLatin1Char(' '), maxChars);
    if (cut < maxChars / 2) cut = maxChars;
    return heading.left(cut).trimmed() + QStringLiteral("…");
}

bool looksLikeHeading(const QString& line) {
    const QString text = line.trimmed();
    if (text.size() < 3 || text.size() > 90) return false;
    static const QRegularExpression rejectedChars(QStringLiteral(R"([(){}\[\]|=<>\\"“”])"));
    if (text.contains(rejectedChars)) return false;
    const QChar last = text.at(text.size() - 1);
    if (last == QLatin1Char('.') || last == QLatin1Char(',') ||
        last == QLatin1Char(';') || last == QLatin1Char(':')) {
        return false;
    }
    int letters = 0;
    for (const QChar ch : text) {
        if (ch.isLetter()) ++letters;
    }
    if (letters < 3) return false;
    const int words = static_cast<int>(text.split(QLatin1Char(' '), Qt::SkipEmptyParts).size());
    return words <= 12;
}

/// Builds a table of contents from the text layer for PDFs without bookmarks.
/// A run of consecutive pages that open with the same heading is one section
/// named after that heading; documents without such running headers keep the
/// plain per-page listing.
QVector<TocItem> buildTextOutline(const QVector<QStringList>& pages, const QSet<QString>& furniture) {
    QVector<QString> headings;
    headings.reserve(pages.size());
    for (const QStringList& lines : pages) {
        QString heading = pageHeadingLine(lines, furniture);
        if (!looksLikeHeading(heading)) heading.clear();
        headings.append(heading);
    }

    QVector<TocItem> items;
    int i = 0;
    while (i < headings.size()) {
        if (headings[i].isEmpty()) {
            ++i;
            continue;
        }
        const QString key = canonicalPdfLine(headings[i]);
        int end = i + 1;
        while (end < headings.size() && canonicalPdfLine(headings[end]) == key) ++end;

        // One-off lines are captions or callout boxes, not section headers.
        if (end - i >= 2) {
            TocItem item;
            item.id = QStringLiteral("toc_text_%1").arg(i);
            item.title = shortenHeading(headings[i]);
            item.chapterIndex = i;
            item.pageIndex = i;
            if (items.isEmpty() || items.last().title != item.title) {
                items.append(item);
            }
        }
        i = end;
    }
    return items;
}

/// Finds likely section headings through the text layer's font sizes, which
/// pdftohtml reports as XML. Used when a PDF has neither bookmarks nor reliable
/// running headers.
QVector<TocItem> buildFontOutline(const QString& filePath, const QString& bookTitle) {
    const QString pdftohtml = QStandardPaths::findExecutable("pdftohtml");
    if (pdftohtml.isEmpty()) return {};

    QProcess proc;
    proc.start(pdftohtml, QStringList() << QStringLiteral("-xml") << QStringLiteral("-i")
                                        << QStringLiteral("-q") << QStringLiteral("-stdout")
                                        << filePath);
    if (!proc.waitForFinished(120000)) {
        proc.kill();
        return {};
    }
    if (proc.exitCode() != 0) return {};
    const QByteArray xml = proc.readAllStandardOutput();
    if (xml.isEmpty()) return {};

    struct Block {
        int page = -1;
        qreal top = 0.0;
        int size = 0;
        QString text;
    };

    QVector<Block> blocks;
    QHash<int, int> fontSizes;
    QHash<int, qreal> pageHeights;
    int currentPage = -1;

    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) continue;
        if (reader.name() == QLatin1String("page")) {
            currentPage = reader.attributes().value(QLatin1String("number")).toInt() - 1;
            pageHeights[currentPage] = reader.attributes().value(QLatin1String("height")).toDouble();
        } else if (reader.name() == QLatin1String("fontspec")) {
            fontSizes[reader.attributes().value(QLatin1String("id")).toInt()] =
                qRound(reader.attributes().value(QLatin1String("size")).toDouble());
        } else if (reader.name() == QLatin1String("text")) {
            const int fontId = reader.attributes().value(QLatin1String("font")).toInt();
            const qreal top = reader.attributes().value(QLatin1String("top")).toDouble();
            const QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
            if (currentPage >= 0 && !text.isEmpty()) {
                blocks.append({currentPage, top, fontSizes.value(fontId, 0), text});
            }
        }
    }
    if (reader.hasError() && blocks.isEmpty()) return {};

    // Body text is the font size covering most characters; larger runs are
    // headings.
    QHash<int, int> charsPerSize;
    for (const Block& block : blocks) {
        charsPerSize[block.size] += static_cast<int>(block.text.size());
    }
    int bodySize = 0;
    int bodyChars = 0;
    for (auto it = charsPerSize.cbegin(); it != charsPerSize.cend(); ++it) {
        if (it.value() > bodyChars) {
            bodyChars = it.value();
            bodySize = it.key();
        }
    }
    if (bodySize <= 0) return {};

    // Covers and title pages are almost entirely display type; their text is
    // not a section heading.
    QHash<int, int> pageChars;
    QHash<int, int> pageBodyChars;
    for (const Block& block : blocks) {
        pageChars[block.page] += static_cast<int>(block.text.size());
        if (block.size <= bodySize + 1) {
            pageBodyChars[block.page] += static_cast<int>(block.text.size());
        }
    }

    std::sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b) {
        return a.page != b.page ? a.page < b.page : a.top < b.top;
    });

    struct Candidate {
        int page;
        QString title;
    };

    QVector<Candidate> candidates;
    QString previousTitle;
    for (const Block& block : blocks) {
        if (block.size < bodySize + 3) continue;
        if (!looksLikeHeading(block.text)) continue;
        if (block.text.startsWith(QLatin1String("Figure "), Qt::CaseInsensitive) ||
            block.text.startsWith(QLatin1String("Table "), Qt::CaseInsensitive)) {
            continue;
        }
        const qreal pageHeight = pageHeights.value(block.page, 0.0);
        if (pageHeight > 0.0 && (block.top < pageHeight * 0.05 || block.top > pageHeight * 0.88)) {
            continue;
        }
        const int total = pageChars.value(block.page);
        if (total > 0 && pageBodyChars.value(block.page) < total * 0.25) {
            continue; // display page, not prose
        }

        const QString title = shortenHeading(block.text);
        if (title == previousTitle) continue;
        previousTitle = title;
        // Title pages and running heads repeat the book title, not a section.
        if (canonicalPdfLine(title) == canonicalPdfLine(bookTitle)) continue;
        candidates.append({block.page, title});
    }

    // Running heads set in a large font repeat on nearly every page; only
    // chapter and section titles are worth listing.
    QHash<QString, int> titleCounts;
    for (const Candidate& candidate : candidates) {
        ++titleCounts[candidate.title];
    }

    QVector<TocItem> items;
    QString lastTitle;
    for (const Candidate& candidate : candidates) {
        if (titleCounts.value(candidate.title) > 3) continue;
        if (candidate.title == lastTitle) continue;
        lastTitle = candidate.title;

        TocItem item;
        item.id = QStringLiteral("toc_font_%1").arg(items.size());
        item.title = candidate.title;
        item.chapterIndex = candidate.page;
        item.pageIndex = candidate.page;
        items.append(item);
    }

    // More headings than pages means the font heuristic misfired.
    if (items.size() > pageHeights.size()) return {};
    return items;
}

/// Extracts per-page plain text for the whole document in one pdftotext pass
QVector<QStringList> extractPdfPageLines(const QString& filePath, int pageCount) {
    QVector<QStringList> pageLines;
    const QString pdftotext = QStandardPaths::findExecutable("pdftotext");
    if (pdftotext.isEmpty() || pageCount <= 0) return pageLines;

    QProcess proc;
    proc.start(pdftotext, QStringList() << QStringLiteral("-enc") << QStringLiteral("UTF-8")
                                        << filePath << QStringLiteral("-"));
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        return pageLines;
    }
    if (proc.exitCode() != 0) return pageLines;

    const QByteArray rawOutput = proc.readAllStandardOutput();
    if (rawOutput.isEmpty()) return pageLines;

    QString text = QString::fromUtf8(rawOutput);
    text.remove(QLatin1Char('\r'));
    const QStringList chunks = text.split(QLatin1Char('\f'));
    pageLines.reserve(pageCount);
    for (int i = 0; i < pageCount; ++i) {
        pageLines.append(i < chunks.size() ? chunks[i].split(QLatin1Char('\n')) : QStringList());
    }
    return pageLines;
}

} // namespace

bool PdfParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains('.')) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == QStringLiteral("pdf");
}

QVector<TocItem> PdfParser::readEmbeddedOutline(const QByteArray& data) const {
    if (!validateHeader(data)) return {};

    const auto objMap = buildObjectIndex(data);
    if (objMap.empty()) return {};

    TrailerInfo trailer;
    parseTrailerAndXRef(data, objMap, trailer);

    int catalogObjNum = trailer.rootObjNum;
    if (catalogObjNum <= 0) {
        for (const auto& [num, offset] : objMap) {
            const QByteArray obj = getObjectData(data, objMap, num);
            if (obj.contains("/Type /Catalog") || obj.contains("/Type/Catalog")) {
                catalogObjNum = num;
                break;
            }
        }
    }
    if (catalogObjNum <= 0) return {};

    const QByteArray catalogDict = getObjectData(data, objMap, catalogObjNum);
    const int pagesRootObjNum = parseIndirectRef(findDictValue(catalogDict, "/Pages"));

    QVector<int> pageObjNums;
    std::unordered_set<int> visitedNodes;
    if (pagesRootObjNum > 0) {
        collectPagesFromNode(data, objMap, pagesRootObjNum, pageObjNums, visitedNodes);
    }
    if (pageObjNums.isEmpty()) {
        pageObjNums = scanPagesDirectly(data, objMap);
    }
    if (pageObjNums.isEmpty()) return {};

    std::unordered_map<int, int> pageObjToIndex;
    for (int i = 0; i < pageObjNums.size(); ++i) {
        pageObjToIndex[pageObjNums[i]] = i;
    }

    const int outlinesObjNum = parseIndirectRef(findDictValue(catalogDict, "/Outlines"));
    if (outlinesObjNum <= 0) return {};
    const QByteArray outlinesDict = getObjectData(data, objMap, outlinesObjNum);
    const int firstOutlineObj = parseIndirectRef(findDictValue(outlinesDict, "/First"));
    if (firstOutlineObj <= 0) return {};

    std::unordered_set<int> visitedOutlines;
    return parseOutlineSiblings(data, objMap, firstOutlineObj, 0, pageObjToIndex, visitedOutlines);
}

std::optional<BookDocument> PdfParser::parseFile(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return std::nullopt;
    }

    QFileInfo info(filePath);
    BookDocument doc;
    doc.id = HashUtils::calculateFileMd5(filePath);
    doc.filePath = filePath;
    doc.title = info.completeBaseName();
    doc.author = "Unknown Author";
    doc.format = "pdf";

    // Fast metadata & page count extraction via a single pdfinfo call (<15ms)
    int pageCount = 0;
    QString pdfinfo = QStandardPaths::findExecutable("pdfinfo");
    if (!pdfinfo.isEmpty()) {
        QProcess infoProc;
        infoProc.start(pdfinfo, QStringList() << filePath);
        if (infoProc.waitForFinished(1500) && infoProc.exitCode() == 0) {
            QString infoText = QString::fromUtf8(infoProc.readAllStandardOutput());
            static const QRegularExpression pageRegex(R"(Pages:\s+(\d+))");
            auto match = pageRegex.match(infoText);
            if (match.hasMatch()) {
                pageCount = match.captured(1).toInt();
            }
            for (const QString& line : infoText.split('\n')) {
                if (line.startsWith("Title:") && line.mid(6).trimmed().length() > 2) {
                    doc.title = line.mid(6).trimmed();
                } else if (line.startsWith("Author:") && line.mid(7).trimmed().length() > 2) {
                    doc.author = line.mid(7).trimmed();
                }
            }
        }
    }

    if (pageCount <= 0) {
        pageCount = PdfPageService::instance().getPageCount(filePath);
    }

    if (pageCount > 0) {
        doc.totalChapters = pageCount;
        doc.chapters.reserve(pageCount);

        // Text layer extraction for searching / TTS / statistics
        const QVector<QStringList> pageLines = extractPdfPageLines(filePath, pageCount);
        const QSet<QString> furniture = pageLines.isEmpty()
            ? QSet<QString>()
            : detectRunningFurniture(pageLines);

        // Prefer embedded bookmarks; otherwise fall back to running headers or font headings
        QVector<TocItem> outline;
        QFile pdfFile(filePath);
        if (pdfFile.size() > 0 && pdfFile.size() <= 256LL * 1024 * 1024 && pdfFile.open(QIODevice::ReadOnly)) {
            outline = readEmbeddedOutline(pdfFile.readAll());
        }
        if (outline.size() < 2 && !pageLines.isEmpty()) {
            const QVector<TocItem> textOutline = buildTextOutline(pageLines, furniture);
            if (textOutline.size() >= 2) outline = textOutline;
        }
        if (outline.size() < 2) {
            const QVector<TocItem> fontOutline = buildFontOutline(filePath, doc.title);
            if (fontOutline.size() >= 2) outline = fontOutline;
        }
        if (outline.size() < 2) outline.clear();

        // Map section titles to pages if outline exists
        QHash<int, QString> titleByPage;
        auto collectTitles = [&](auto& self, const QVector<TocItem>& items) -> void {
            for (const auto& item : items) {
                if (item.pageIndex >= 0 && !titleByPage.contains(item.pageIndex) && !item.title.isEmpty()) {
                    titleByPage[item.pageIndex] = item.title;
                }
                self(self, item.children);
            }
        };
        collectTitles(collectTitles, outline);

        QString runningTitle;
        for (int i = 0; i < pageCount; ++i) {
            ChapterContent c;
            c.index = i;
            if (const auto found = titleByPage.constFind(i); found != titleByPage.constEnd()) {
                runningTitle = found.value();
            }
            c.title = runningTitle.isEmpty() ? QString("Page %1").arg(i + 1) : runningTitle;
            c.isImagePage = true;
            if (i < pageLines.size()) {
                c.textContent = unwrapPageText(pageLines[i], furniture);
                if (c.textContent.isEmpty()) c.textContent = c.title;
                c.htmlContent = QString("<p>%1</p>").arg(c.textContent.toHtmlEscaped());
            } else {
                c.textContent = c.title;
                c.htmlContent = QString("<p>%1</p>").arg(c.title);
            }
            doc.chapters.append(c);
        }

        doc.toc = std::move(outline);
        if (doc.toc.isEmpty()) {
            doc.toc.reserve(pageCount);
            for (int i = 0; i < doc.chapters.size(); ++i) {
                TocItem tocItem;
                tocItem.id = QString("toc_%1").arg(i);
                tocItem.title = doc.chapters[i].title;
                tocItem.chapterIndex = i;
                tocItem.pageIndex = i;
                doc.toc.append(tocItem);
            }
        }
        doc.totalChapters = static_cast<int>(doc.chapters.size());

        return doc;
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

    QString runningTitle;
    for (int i = 0; i < pageObjNums.size(); ++i) {
        int pageObjNum = pageObjNums[i];
        QByteArray pageDict = getObjectData(data, objMap, pageObjNum);

        if (const auto found = pageToTitleMap.find(i); found != pageToTitleMap.end()) {
            runningTitle = found->second;
        }
        QString pageTitle = runningTitle.isEmpty()
                                ? QStringLiteral("Page %1").arg(i + 1)
                                : runningTitle;

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
        chapter.isImagePage = !filePath.isEmpty();

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
    doc.filePath = filePath;
    doc.title = title;
    doc.author = author;
    doc.format = QStringLiteral("pdf");
    doc.totalChapters = static_cast<int>(chapters.size());
    doc.chapters = std::move(chapters);
    doc.toc = std::move(toc);

    return doc;
}

} // namespace LunaRead
