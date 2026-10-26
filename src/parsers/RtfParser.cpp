#include "RtfParser.h"
#include "../utils/HashUtils.h"

#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <vector>
#include <algorithm>

namespace LunaRead {

namespace {

enum class RtfDestination {
    Body,
    FontTable,
    ColorTable,
    Stylesheet,
    Picture,
    Info,
    Title,
    Author,
    OtherIgnored
};

struct RtfState {
    RtfDestination destination = RtfDestination::Body;
    bool isIgnored = false;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    int ucCount = 1;
    int codepage = 1252;
};

// Maps Windows-1252 0x80-0x9F range to Unicode UTF-16 code points
ushort decodeCp1252HighByte(uint8_t byteVal) {
    static constexpr ushort cp1252Table[32] = {
        0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
        0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178
    };
    if (byteVal >= 0x80 && byteVal <= 0x9F) {
        ushort u = cp1252Table[byteVal - 0x80];
        if (u != 0) {
            return u;
        }
    }
    return byteVal;
}

} // namespace

bool RtfParser::isValidRtfHeader(const QByteArray& data) {
    if (data.size() < 5) {
        return false;
    }

    qsizetype start = 0;
    const qsizetype totalSize = data.size();

    // Skip optional UTF-8 BOM if present
    if (totalSize >= 3 &&
        static_cast<uint8_t>(data[0]) == 0xEF &&
        static_cast<uint8_t>(data[1]) == 0xBB &&
        static_cast<uint8_t>(data[2]) == 0xBF) {
        start = 3;
    }

    // Skip leading whitespace
    while (start < totalSize && (data[start] == ' ' || data[start] == '\t' ||
                                 data[start] == '\r' || data[start] == '\n')) {
        ++start;
    }

    if (totalSize - start < 5) {
        return false;
    }

    return data.mid(start, 5) == "{\\rtf";
}

bool RtfParser::canParse(const QString& filePathOrExtension) const {
    QString ext = filePathOrExtension.toLower();
    if (ext.contains(QLatin1Char('.'))) {
        ext = QFileInfo(filePathOrExtension).suffix().toLower();
    }
    return ext == QStringLiteral("rtf");
}

std::optional<BookDocument> RtfParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseBytes(file.readAll(), QStringLiteral("rtf"), filePath);
}

std::optional<BookDocument> RtfParser::parseBytes(const QByteArray& data,
                                                 const QString& format,
                                                 const QString& filePath) {
    Q_UNUSED(format);

    if (!isValidRtfHeader(data)) {
        return std::nullopt;
    }

    const QString fileName = !filePath.isEmpty()
        ? QFileInfo(filePath).completeBaseName()
        : QStringLiteral("RTF Document");

    const QString docId = HashUtils::calculateBytesMd5(data);

    // RTF State Machine setup
    std::vector<RtfState> stateStack;
    stateStack.reserve(32);

    RtfState currentState;
    int groupDepth = 0;
    int skipChars = 0;

    QString docTitle;
    QString docAuthor;

    QString currentChapterTitle;
    QString currentParagraphText;
    QString currentParagraphHtml;
    QString currentChapterText;
    QString currentChapterHtml;

    bool currentHtmlBold = false;
    bool currentHtmlItalic = false;

    QVector<ChapterContent> chapters;
    QVector<TocItem> toc;

    auto updateHtmlStyles = [&]() {
        if (currentState.bold != currentHtmlBold) {
            if (currentState.bold) {
                currentParagraphHtml.append(QStringLiteral("<b>"));
                currentHtmlBold = true;
            } else {
                currentParagraphHtml.append(QStringLiteral("</b>"));
                currentHtmlBold = false;
            }
        }
        if (currentState.italic != currentHtmlItalic) {
            if (currentState.italic) {
                currentParagraphHtml.append(QStringLiteral("<i>"));
                currentHtmlItalic = true;
            } else {
                currentParagraphHtml.append(QStringLiteral("</i>"));
                currentHtmlItalic = false;
            }
        }
    };

    auto emitChar = [&](QChar ch) {
        if (currentState.isIgnored) {
            return;
        }

        if (currentState.destination == RtfDestination::Title) {
            docTitle.append(ch);
        } else if (currentState.destination == RtfDestination::Author) {
            docAuthor.append(ch);
        } else if (currentState.destination == RtfDestination::Body) {
            updateHtmlStyles();
            currentParagraphText.append(ch);

            if (ch == QLatin1Char('<')) {
                currentParagraphHtml.append(QStringLiteral("&lt;"));
            } else if (ch == QLatin1Char('>')) {
                currentParagraphHtml.append(QStringLiteral("&gt;"));
            } else if (ch == QLatin1Char('&')) {
                currentParagraphHtml.append(QStringLiteral("&amp;"));
            } else if (ch == QLatin1Char('"')) {
                currentParagraphHtml.append(QStringLiteral("&quot;"));
            } else {
                currentParagraphHtml.append(ch);
            }
        }
    };

    auto flushParagraph = [&]() {
        if (currentHtmlItalic) {
            currentParagraphHtml.append(QStringLiteral("</i>"));
            currentHtmlItalic = false;
        }
        if (currentHtmlBold) {
            currentParagraphHtml.append(QStringLiteral("</b>"));
            currentHtmlBold = false;
        }

        const QString trimmedText = currentParagraphText.trimmed();
        if (!trimmedText.isEmpty()) {
            if (currentChapterTitle.isEmpty() && currentChapterText.isEmpty()) {
                currentChapterTitle = trimmedText;
                if (currentChapterTitle.length() > 80) {
                    const qsizetype dotPos = currentChapterTitle.indexOf(QLatin1Char('.'));
                    if (dotPos > 0 && dotPos <= 80) {
                        currentChapterTitle = currentChapterTitle.left(dotPos).trimmed();
                    } else {
                        currentChapterTitle = currentChapterTitle.left(77).trimmed() + QStringLiteral("...");
                    }
                }
            }

            currentChapterText.append(trimmedText);
            currentChapterText.append(QStringLiteral("\n\n"));
            currentChapterHtml.append(QStringLiteral("<p>") + currentParagraphHtml.trimmed() + QStringLiteral("</p>\n"));
        }

        currentParagraphText.clear();
        currentParagraphHtml.clear();
    };

    auto flushChapter = [&]() {
        flushParagraph();
        const QString trimmedChapterText = currentChapterText.trimmed();
        if (!trimmedChapterText.isEmpty()) {
            const int chIndex = static_cast<int>(chapters.size());
            QString chTitle = currentChapterTitle.trimmed();
            if (chTitle.isEmpty()) {
                chTitle = QStringLiteral("Chapter %1").arg(chIndex + 1);
            }

            ChapterContent chapter;
            chapter.index = chIndex;
            chapter.title = chTitle;
            chapter.textContent = trimmedChapterText;
            chapter.htmlContent = currentChapterHtml.trimmed();
            chapters.append(chapter);

            TocItem item;
            item.id = QStringLiteral("toc_%1").arg(chIndex);
            item.title = chTitle;
            item.chapterIndex = chIndex;
            item.level = 1;
            toc.append(item);

            currentChapterText.clear();
            currentChapterHtml.clear();
            currentChapterTitle.clear();
        }
    };

    auto handleControlWord = [&](const QByteArray& word, bool hasParam, int param) {
        if (word == "par") {
            if (!currentState.isIgnored) {
                flushParagraph();
            }
        } else if (word == "line") {
            if (!currentState.isIgnored) {
                currentParagraphText.append(QLatin1Char('\n'));
                currentParagraphHtml.append(QStringLiteral("<br/>"));
            }
        } else if (word == "page" || word == "sect") {
            if (!currentState.isIgnored) {
                flushChapter();
            }
        } else if (word == "b") {
            currentState.bold = (!hasParam || param != 0);
        } else if (word == "i") {
            currentState.italic = (!hasParam || param != 0);
        } else if (word == "ul") {
            currentState.underline = (!hasParam || param != 0);
        } else if (word == "ulnone") {
            currentState.underline = false;
        } else if (word == "u") {
            if (hasParam) {
                int codeVal = param;
                if (codeVal < 0) {
                    codeVal += 65536;
                }
                emitChar(QChar(static_cast<char16_t>(codeVal)));
                skipChars = currentState.ucCount;
            }
        } else if (word == "uc") {
            if (hasParam && param >= 0) {
                currentState.ucCount = param;
            }
        } else if (word == "ansicpg") {
            if (hasParam) {
                currentState.codepage = param;
            }
        } else if (word == "fonttbl") {
            currentState.destination = RtfDestination::FontTable;
            currentState.isIgnored = true;
        } else if (word == "colortbl") {
            currentState.destination = RtfDestination::ColorTable;
            currentState.isIgnored = true;
        } else if (word == "stylesheet") {
            currentState.destination = RtfDestination::Stylesheet;
            currentState.isIgnored = true;
        } else if (word == "pict") {
            currentState.destination = RtfDestination::Picture;
            currentState.isIgnored = true;
        } else if (word == "info") {
            currentState.destination = RtfDestination::Info;
            currentState.isIgnored = true;
        } else if (word == "title") {
            currentState.destination = RtfDestination::Title;
            currentState.isIgnored = false;
        } else if (word == "author") {
            currentState.destination = RtfDestination::Author;
            currentState.isIgnored = false;
        } else if (word == "header" || word == "footer" ||
                   word == "headerl" || word == "headerr" ||
                   word == "footerl" || word == "footerr") {
            currentState.destination = RtfDestination::OtherIgnored;
            currentState.isIgnored = true;
        } else if (word == "emdash") {
            emitChar(QChar(0x2014));
        } else if (word == "endash") {
            emitChar(QChar(0x2013));
        } else if (word == "bullet") {
            emitChar(QChar(0x2022));
        } else if (word == "lquote") {
            emitChar(QChar(0x2018));
        } else if (word == "rquote") {
            emitChar(QChar(0x2019));
        } else if (word == "ldblquote") {
            emitChar(QChar(0x201C));
        } else if (word == "rdblquote") {
            emitChar(QChar(0x201D));
        } else if (word == "tab") {
            emitChar(QChar(QLatin1Char('\t')));
        }
    };

    const qsizetype totalBytes = data.size();
    qsizetype idx = 0;

    while (idx < totalBytes) {
        const char c = data[idx];

        if (c == '{') {
            ++groupDepth;
            stateStack.push_back(currentState);
            ++idx;

            // Check if group is an unhandled destination beginning with \*
            if (idx + 1 < totalBytes && data[idx] == '\\' && data[idx + 1] == '*') {
                currentState.isIgnored = true;
                currentState.destination = RtfDestination::OtherIgnored;
                idx += 2;
            }
        } else if (c == '}') {
            if (groupDepth > 0) {
                --groupDepth;
                if (!stateStack.empty()) {
                    currentState = stateStack.back();
                    stateStack.pop_back();
                }
            }
            ++idx;
        } else if (c == '\\') {
            ++idx;
            if (idx >= totalBytes) {
                break;
            }

            const char next = data[idx];
            if (next == '{' || next == '}' || next == '\\') {
                if (skipChars > 0) {
                    --skipChars;
                } else {
                    emitChar(QChar(static_cast<uchar>(next)));
                }
                ++idx;
            } else if (next == '~') {
                if (skipChars > 0) {
                    --skipChars;
                } else {
                    emitChar(QChar(0x00A0)); // non-breaking space
                }
                ++idx;
            } else if (next == '-') {
                if (skipChars > 0) {
                    --skipChars;
                }
                // Optional hyphen (soft hyphen) - omit in plain representation
                ++idx;
            } else if (next == '_') {
                if (skipChars > 0) {
                    --skipChars;
                } else {
                    emitChar(QChar(0x2011)); // non-breaking hyphen
                }
                ++idx;
            } else if (next == '\'') {
                ++idx;
                if (idx + 1 < totalBytes) {
                    bool ok = false;
                    const int hexVal = data.mid(idx, 2).toInt(&ok, 16);
                    idx += 2;
                    if (skipChars > 0) {
                        --skipChars;
                    } else if (ok) {
                        const uint8_t byteVal = static_cast<uint8_t>(hexVal);
                        const ushort decodedCode = decodeCp1252HighByte(byteVal);
                        emitChar(QChar(decodedCode));
                    }
                }
            } else if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
                const qsizetype wordStart = idx;
                while (idx < totalBytes &&
                       ((data[idx] >= 'a' && data[idx] <= 'z') || (data[idx] >= 'A' && data[idx] <= 'Z'))) {
                    ++idx;
                }
                const QByteArray word = data.mid(wordStart, idx - wordStart).toLower();

                bool hasParam = false;
                int param = 0;
                if (idx < totalBytes && (data[idx] == '-' || (data[idx] >= '0' && data[idx] <= '9'))) {
                    const qsizetype paramStart = idx;
                    if (data[idx] == '-') {
                        ++idx;
                    }
                    while (idx < totalBytes && (data[idx] >= '0' && data[idx] <= '9')) {
                        ++idx;
                    }
                    param = data.mid(paramStart, idx - paramStart).toInt(&hasParam);
                }

                // RTF specification: trailing single space is a delimiter and consumed
                if (idx < totalBytes && data[idx] == ' ') {
                    ++idx;
                }

                handleControlWord(word, hasParam, param);
            } else {
                // Unknown control symbol, consume
                ++idx;
            }
        } else if (c == '\r' || c == '\n') {
            // Raw carriage returns and line feeds in RTF markup are ignored delimiters
            ++idx;
        } else if (c == '\t') {
            if (skipChars > 0) {
                --skipChars;
            } else {
                emitChar(QChar(QLatin1Char('\t')));
            }
            ++idx;
        } else {
            if (skipChars > 0) {
                --skipChars;
            } else {
                emitChar(QChar(static_cast<uchar>(c)));
            }
            ++idx;
        }
    }

    // Flush any remaining buffered paragraph and chapter
    flushChapter();

    // Fallback if no chapters were generated (e.g., bare RTF without paragraph tags)
    if (chapters.isEmpty()) {
        flushParagraph();
        flushChapter();
    }

    if (chapters.isEmpty()) {
        ChapterContent fallbackChapter;
        fallbackChapter.index = 0;
        fallbackChapter.title = !docTitle.trimmed().isEmpty() ? docTitle.trimmed() : fileName;
        fallbackChapter.textContent = QString();
        fallbackChapter.htmlContent = QString();
        chapters.append(fallbackChapter);

        TocItem item;
        item.id = QStringLiteral("toc_0");
        item.title = fallbackChapter.title;
        item.chapterIndex = 0;
        item.level = 1;
        toc.append(item);
    }

    BookDocument doc;
    doc.id = docId;
    doc.format = QStringLiteral("rtf");
    doc.title = !docTitle.trimmed().isEmpty() ? docTitle.trimmed() : fileName;
    doc.author = !docAuthor.trimmed().isEmpty() ? docAuthor.trimmed() : QStringLiteral("Unknown Author");
    doc.chapters = chapters;
    doc.toc = toc;
    doc.totalChapters = static_cast<int>(chapters.size());

    return doc;
}

} // namespace LunaRead
