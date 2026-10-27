#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include "../src/parsers/TxtParser.h"
#include "../src/parsers/MarkdownParser.h"
#include "../src/parsers/HtmlParser.h"
#include "../src/parsers/RtfParser.h"
#include "../src/parsers/OdtParser.h"
#include "../src/parsers/ChmParser.h"
#include "../src/parsers/UmdParser.h"
#include "../src/parsers/ComicParser.h"
#include "../src/parsers/DocxParser.h"
#include "../src/parsers/ParserFactory.h"
#include "../src/reader/Typesetter.h"
#include "../src/reader/DualPageRenderer.h"
#include "../src/reader/ContinuousScrollRenderer.h"
#include "../src/ui/SpeedReadingDialog.h"
#include "../src/ui/AllAnnotationsDialog.h"
#include "../src/ui/VocabularyNotebookDialog.h"
#include "../src/db/DatabaseManager.h"
#include "../src/features/ImageCropService.h"
#include "../src/ui/TapZoneConfigDialog.h"

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

    void testDualPageRendererMangaRtl() {
        DualPageRenderer renderer;
        QCOMPARE(renderer.isRightToLeft(), false);

        renderer.setRightToLeft(true);
        QCOMPARE(renderer.isRightToLeft(), true);

        // Verify pageTurnRequested signal emission
        QSignalSpy spy(&renderer, &DualPageRenderer::pageTurnRequested);
        renderer.resize(800, 600);

        // Click on left side (x=100) in Manga RTL mode -> should request forward (+2)
        QTest::mouseClick(&renderer, Qt::LeftButton, Qt::NoModifier, QPoint(100, 300));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 2);

        // Click on right side (x=700) in Manga RTL mode -> should request backward (-2)
        QTest::mouseClick(&renderer, Qt::LeftButton, Qt::NoModifier, QPoint(700, 300));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), -2);
    }

    void testContinuousScrollRendererWebtoonMode() {
        ContinuousScrollRenderer renderer;
        QCOMPARE(renderer.isWebtoonMode(), false);

        renderer.setWebtoonMode(true);
        QCOMPARE(renderer.isWebtoonMode(), true);

        BookDocument doc;
        doc.id = "comic_1";
        doc.title = "Solo Leveling Webtoon";
        ChapterContent ch;
        ch.title = "Chapter 1";
        ch.textContent = "Webtoon action";
        doc.chapters.append(ch);

        renderer.setDocument(doc, ReaderThemeTokens::parchment());
        QCOMPARE(renderer.isWebtoonMode(), true);
    }

    void testSpeedReadingDialog() {
        QString text = "The quick brown fox jumps over the lazy dog. LunaRead speed reading RSVP mode delivers high-speed comprehension.";
        SpeedReadingDialog dlg(text, 400);

        // Verify initial state
        QSignalSpy spy(&dlg, &SpeedReadingDialog::wordIndexChanged);

        dlg.stepForward(5);
        QVERIFY(spy.count() > 0);

        dlg.stepBack(2);
        QVERIFY(spy.count() > 1);

        dlg.setWpm(600);

        dlg.play();
        dlg.pause();

        dlg.restart();
    }

    void testAllAnnotationsDialog() {
        if (!DatabaseManager::instance().isOpen()) {
            DatabaseManager::instance().initialize(":memory:");
        }
        AnnotationRecord a1;
        a1.id = "ann_test_1";
        a1.bookId = "book_test_ann";
        a1.chapterIndex = 2;
        a1.pageIndex = 5;
        a1.selectedText = "A memorable quote from the classic.";
        a1.note = "Important philosophical idea.";
        a1.color = QColor(255, 235, 59);
        a1.type = "highlight";
        a1.createdAt = QDateTime::currentDateTime();
        DatabaseManager::instance().insertAnnotation(a1);

        AllAnnotationsDialog dlg("book_test_ann", "Test Novel");
        QSignalSpy jumpSpy(&dlg, &AllAnnotationsDialog::jumpToAnnotationRequested);

        dlg.applyTheme(ReaderThemeTokens::oledBlack());

        DatabaseManager::instance().deleteAnnotation("ann_test_1");
    }

    void testImageCropService() {
        auto& crop = ImageCropService::instance();
        crop.setAutoCropEnabled(true);
        crop.setWhiteThreshold(230);
        crop.setMarginPadding(4);

        QCOMPARE(crop.isAutoCropEnabled(), true);
        QCOMPARE(crop.whiteThreshold(), 230);
        QCOMPARE(crop.marginPadding(), 4);

        // Create 200x200 white image with a 100x100 black box in the center
        QImage testImg(200, 200, QImage::Format_RGB32);
        testImg.fill(Qt::white);
        for (int y = 50; y < 150; ++y) {
            for (int x = 50; x < 150; ++x) {
                testImg.setPixel(x, y, qRgb(0, 0, 0));
            }
        }

        QImage cropped = crop.cropMargins(testImg);
        QVERIFY(!cropped.isNull());
        // The cropped width should be significantly smaller than 200 (around 100 + 2*padding = 108)
        QVERIFY(cropped.width() < 150);
        QVERIFY(cropped.height() < 150);
        QVERIFY(cropped.width() >= 100);
        QVERIFY(cropped.height() >= 100);

        // Reset
        crop.setAutoCropEnabled(false);
    }

    void testVocabularyNotebook() {
        if (!DatabaseManager::instance().isOpen()) {
            DatabaseManager::instance().initialize(":memory:");
        }
        VocabularyWord w;
        w.id = "vocab_test_1";
        w.word = "serendipity";
        w.phonetic = "/ˌsɛr(ə)nˈdɪpɪti/";
        w.definition = "The occurrence of events by chance in a happy way.";
        w.example = "A fortunate stroke of serendipity.";
        w.bookTitle = "The Three Princes of Serendip";
        w.dateAdded = QDateTime::currentSecsSinceEpoch();

        DatabaseManager::instance().insertVocabularyWord(w);
        QVERIFY(DatabaseManager::instance().getVocabularyWordCount() >= 1);

        auto words = DatabaseManager::instance().getAllVocabularyWords();
        bool found = false;
        for (const auto& item : words) {
            if (item.word == "serendipity") {
                found = true;
                break;
            }
        }
        QVERIFY(found);

        VocabularyNotebookDialog dlg;
        dlg.applyTheme(ReaderThemeTokens::parchment());

        DatabaseManager::instance().deleteVocabularyWord("vocab_test_1");
    }

    void testMouseAndTapZoneControls() {
        GestureController gestures;
        gestures.setMouseWheelAction(MouseWheelAction::AdjustFontSize);
        QCOMPARE(gestures.mouseWheelAction(), MouseWheelAction::AdjustFontSize);

        gestures.setRightClickAction(MouseButtonAction::ToggleBookmark);
        QCOMPARE(gestures.rightClickAction(), MouseButtonAction::ToggleBookmark);

        gestures.setMiddleClickAction(MouseButtonAction::OpenQuickNav);
        QCOMPARE(gestures.middleClickAction(), MouseButtonAction::OpenQuickNav);

        TapZoneConfigDialog dlg(&gestures);
        gestures.saveConfig();
        gestures.loadConfig();
        QCOMPARE(gestures.mouseWheelAction(), MouseWheelAction::AdjustFontSize);
        QCOMPARE(gestures.rightClickAction(), MouseButtonAction::ToggleBookmark);
        QCOMPARE(gestures.middleClickAction(), MouseButtonAction::OpenQuickNav);
    }

    void testTypesetterClassicalBookRendering() {
        Typesetter ts;
        ts.setFont(QFont("Noto Serif", 14));
        ts.setLineHeight(1.6);
        ts.setParagraphIndent(24.0);
        ts.setDropCapsEnabled(true);
        ts.setAlignment(Qt::AlignJustify);

        // 1. Test HTML transformation
        QString rawNovel = "It was the best of times, it was the worst of times.\n\n"
                           "It was the age of wisdom, it was the age of foolishness.";
        QString bookHtml = Typesetter::formatChapterToBookHtml(rawNovel, "Chapter 1: A Tale of Two Cities", true);

        QVERIFY(bookHtml.contains("chapter-title"));
        QVERIFY(bookHtml.contains("Chapter 1: A Tale of Two Cities"));
        QVERIFY(bookHtml.contains("header-flourish"));
        QVERIFY(bookHtml.contains("❧ &nbsp; ❖ &nbsp; ❧"));
        QVERIFY(bookHtml.contains("class=\"dropcap\""));
        QVERIFY(bookHtml.contains("first-para"));

        // 2. Test CSS generation
        QString css = ts.generateBookStyleSheet(ReaderThemeTokens::parchment());
        QVERIFY(css.contains("Noto Serif"));
        QVERIFY(css.contains("text-align: justify"));
        QVERIFY(css.contains(".dropcap"));
        QVERIFY(css.contains("24px"));

        // 3. Test Pagination
        ChapterContent ch;
        ch.index = 0;
        ch.title = "Chapter 1";
        ch.textContent = rawNovel;
        auto pages = ts.paginate(ch, QSizeF(800, 1000), 0, ReaderThemeTokens::parchment());
        QVERIFY(!pages.isEmpty());
        QCOMPARE(pages[0].chapterIndex, 0);
        QVERIFY(pages[0].totalPagesInChapter >= 1);

        // 4. Test Rendering to Paint Device
        QPixmap surface(800, 1000);
        surface.fill(Qt::white);
        QPainter painter(&surface);
        ts.renderPage(&painter, QRectF(0, 0, 800, 1000), pages[0], ReaderThemeTokens::parchment(), "Novel Title", "Chapter 1", false);
        ts.renderPage(&painter, QRectF(0, 0, 800, 1000), pages[0], ReaderThemeTokens::parchment(), "Novel Title", "Chapter 1", true);
        painter.end();
    }

    void testDocxAndComicParsers() {
        DocxParser docxParser;
        QVERIFY(docxParser.canParse("document.docx"));
        QVERIFY(!docxParser.canParse("document.pdf"));

        QByteArray dummyZip(100, '\0');
        auto docx = docxParser.parseBytes(dummyZip, "docx", "sample.docx");
        QVERIFY(docx.has_value());
        QCOMPARE(docx->format, "docx");
        QCOMPARE(docx->title, "sample");

        ComicParser comicParser;
        QVERIFY(comicParser.canParse("manga.cbz"));
        QVERIFY(comicParser.canParse("comic.cbr"));
        QVERIFY(!comicParser.canParse("novel.epub"));

        auto cbz = comicParser.parseBytes(dummyZip, "cbz", "chapter1.cbz");
        QVERIFY(cbz.has_value());
        QCOMPARE(cbz->format, "cbz");
        QCOMPARE(cbz->title, "chapter1");
        QVERIFY(!cbz->chapters.isEmpty());
        QVERIFY(cbz->chapters[0].isImagePage);
    }
};

int runParsersTests() {
    TestParsers tc;
    return QTest::qExec(&tc);
}

#include "test_parsers.moc"
