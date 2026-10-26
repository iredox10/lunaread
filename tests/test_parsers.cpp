#include <QObject>
#include <QTest>
#include "../src/parsers/TxtParser.h"
#include "../src/parsers/MarkdownParser.h"
#include "../src/parsers/HtmlParser.h"
#include "../src/parsers/RtfParser.h"
#include "../src/parsers/OdtParser.h"
#include "../src/parsers/ChmParser.h"
#include "../src/parsers/UmdParser.h"
#include "../src/parsers/ComicParser.h"
#include "../src/parsers/ParserFactory.h"

using namespace LunaRead;

class TestParsers : public QObject {
    Q_OBJECT
private slots:
    void testTxtParserRegex() {
        TxtParser parser;
        QByteArray content = "Chapter 1: The Beginning\n\nIt was the best of times.\n\nChapter 2: The End\n\nIt was the worst of times.";
        auto doc = parser.parseBytes(content, "txt", "novel.txt");

        QVERIFY(doc.has_value());
        QCOMPARE(doc->title, "novel");
        QCOMPARE(doc->chapters.size(), 2);
        QCOMPARE(doc->chapters[0].title, "Chapter 1: The Beginning");
    }

    void testMarkdownParser() {
        MarkdownParser parser;
        QByteArray content = "# Introduction\n\nWelcome to LunaRead.\n\n## Architecture\n\nQt 6 and C++20 desktop engine.";
        auto doc = parser.parseBytes(content, "md", "readme.md");

        QVERIFY(doc.has_value());
        QCOMPARE(doc->chapters.size(), 2);
        QCOMPARE(doc->chapters[0].title, "Introduction");
        QCOMPARE(doc->chapters[1].title, "Architecture");
    }

    void testHtmlParser() {
        HtmlParser parser;
        QByteArray content =
            "<!DOCTYPE html><html><head><title>Online Novel</title><meta name=\"author\" content=\"Jane Doe\"></head>"
            "<body><h1>Chapter I: Arrival</h1><p>She arrived at dusk.</p>"
            "<h1>Chapter II: Departure</h1><p>She left at dawn.</p></body></html>";

        auto doc = parser.parseBytes(content, "html", "novel.html");
        QVERIFY(doc.has_value());
        QCOMPARE(doc->title, "Online Novel");
        QCOMPARE(doc->author, "Jane Doe");
        QCOMPARE(doc->chapters.size(), 2);
        QCOMPARE(doc->chapters[0].title, "Chapter I: Arrival");
        QCOMPARE(doc->chapters[1].title, "Chapter II: Departure");
        QCOMPARE(doc->toc.size(), 2);
    }

    void testRtfParser() {
        RtfParser parser;
        QByteArray content = "{\\rtf1\\ansi\\deff0 {\\fonttbl{\\f0 Arial;}}\\fs24 Chapter 1\\line Hello LunaRead RTF!}";
        auto doc = parser.parseBytes(content, "rtf", "sample.rtf");

        QVERIFY(doc.has_value());
        QCOMPARE(doc->title, "sample");
        QVERIFY(doc->chapters.size() >= 1);
        QVERIFY(doc->chapters[0].textContent.contains("Hello LunaRead RTF!"));
    }

    void testChmParser() {
        ChmParser parser;
        // ITSF container header with embedded HTML
        QByteArray content;
        content.append("ITSF\x03\x00\x00\x00", 8);
        content.append("<html><head><title>Help Manual</title></head>"
                       "<body><h1>Quickstart Guide</h1><p>This is the LunaRead CHM guide.</p></body></html>");
        auto doc = parser.parseBytes(content, "chm", "help.chm");

        QVERIFY(doc.has_value());
        QCOMPARE(doc->title, "Help Manual");
        QVERIFY(doc->chapters.size() >= 1);
        QCOMPARE(doc->chapters[0].title, "Quickstart Guide");
    }

    void testUmdParser() {
        UmdParser parser;
        // 0xDE9A9B89 magic header + basic text chunk
        QByteArray content;
        content.append("\x89\x9B\x9A\xDE", 4); // Little-endian 0xDE9A9B89
        content.append("Sample UMD novel text content.");

        auto doc = parser.parseBytes(content, "umd", "story.umd");
        QVERIFY(doc.has_value());
        QCOMPARE(doc->title, "story");
        QVERIFY(doc->chapters.size() >= 1);
        QVERIFY(doc->chapters[0].textContent.contains("Sample UMD novel text content."));
    }

    void testComicParser() {
        ComicParser parser;
        QVERIFY(parser.supportedExtensions().contains("cbz"));
        QVERIFY(parser.supportedExtensions().contains("cbr"));
    }

    void testParserFactorySupportAllFormats() {
        // Moon+ Reader Full Universal Format List:
        // EPUB, PDF, DJVU, MOBI, AZW, AZW3, PRC, FB2, DOCX, ODT, RTF, TXT, MD, CBZ, CBR, HTML, HTM, XHTML, CHM, UMD
        QVERIFY(ParserFactory::isSupported("book.epub"));
        QVERIFY(ParserFactory::isSupported("book.pdf"));
        QVERIFY(ParserFactory::isSupported("book.djvu"));
        QVERIFY(ParserFactory::isSupported("book.mobi"));
        QVERIFY(ParserFactory::isSupported("book.azw"));
        QVERIFY(ParserFactory::isSupported("book.azw3"));
        QVERIFY(ParserFactory::isSupported("book.prc"));
        QVERIFY(ParserFactory::isSupported("book.fb2"));
        QVERIFY(ParserFactory::isSupported("book.docx"));
        QVERIFY(ParserFactory::isSupported("book.odt"));
        QVERIFY(ParserFactory::isSupported("book.rtf"));
        QVERIFY(ParserFactory::isSupported("book.txt"));
        QVERIFY(ParserFactory::isSupported("notes.md"));
        QVERIFY(ParserFactory::isSupported("comic.cbz"));
        QVERIFY(ParserFactory::isSupported("comic.cbr"));
        QVERIFY(ParserFactory::isSupported("page.html"));
        QVERIFY(ParserFactory::isSupported("page.htm"));
        QVERIFY(ParserFactory::isSupported("page.xhtml"));
        QVERIFY(ParserFactory::isSupported("manual.chm"));
        QVERIFY(ParserFactory::isSupported("novel.umd"));

        // Non-supported format
        QCOMPARE(ParserFactory::isSupported("binary.exe"), false);
        QCOMPARE(ParserFactory::isSupported("music.mp3"), false);
    }
};

int runParsersTests() {
    TestParsers tc;
    return QTest::qExec(&tc);
}

#include "test_parsers.moc"
