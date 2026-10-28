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
#include "../src/parsers/Fb2Parser.h"
#include "../src/parsers/MobiParser.h"
#include "../src/parsers/PdfParser.h"
#include "../src/features/AnnotationExporter.h"
#include "../src/features/PdfPageService.h"
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

        // 4. Test Rendering to Paint Device & Export Visual Proof
        TxtParser aliceParser;
        auto aliceDoc = aliceParser.parseFile("/home/iredox/Desktop/space/lunaread-desktop-qt/samples/alice_in_wonderland.txt");
        if (aliceDoc.has_value() && !aliceDoc->chapters.isEmpty()) {
            const auto& aliceCh = aliceDoc->chapters[0];
            const QSize pageSize(720, 1000);
            const QSize spreadSize(1440, 1000);

            // A. Single Page Parchment
            auto parchmentTokens = ReaderThemeTokens::parchment();
            auto alicePages = ts.paginate(aliceCh, pageSize, 0, parchmentTokens);
            if (!alicePages.isEmpty()) {
                QPixmap pm(pageSize);
                pm.fill(parchmentTokens.backgroundColor);
                QPainter p(&pm);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::TextAntialiasing, true);
                ts.renderPage(&p, QRectF(0, 0, pageSize.width(), pageSize.height()), alicePages[0], parchmentTokens, aliceDoc->title, aliceCh.title, false);
                p.end();
                pm.save("/home/iredox/.gemini/antigravity-cli/brain/622a2e99-f95e-47a2-9196-72a33b1081da/render_parchment_page1.png");
            }

            // B. Dual Page Spread (Verso / Recto with Spine Valley Shadow)
            auto sepiaTokens = ReaderThemeTokens::sepia();
            auto spreadPages = ts.paginate(aliceCh, QSize(spreadSize.width() / 2, spreadSize.height()), 0, sepiaTokens);
            if (spreadPages.size() >= 2) {
                QPixmap pm(spreadSize);
                pm.fill(sepiaTokens.backgroundColor);
                QPainter p(&pm);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::TextAntialiasing, true);

                qreal halfW = spreadSize.width() / 2.0;
                QRectF leftRect(0, 0, halfW, spreadSize.height());
                QRectF rightRect(halfW, 0, halfW, spreadSize.height());

                // Left (Verso): Book Title Header
                ts.renderPage(&p, leftRect, spreadPages[0], sepiaTokens, aliceDoc->title, aliceCh.title, true);
                // Right (Recto): Chapter Title Header
                ts.renderPage(&p, rightRect, spreadPages[1], sepiaTokens, aliceDoc->title, aliceCh.title, false);

                // Central Spine Valley Shadow
                QLinearGradient gutterGrad(halfW - 24.0, 0, halfW + 24.0, 0);
                gutterGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
                gutterGrad.setColorAt(0.5, QColor(0, 0, 0, 50));
                gutterGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
                p.fillRect(QRectF(halfW - 24.0, 0, 48.0, spreadSize.height()), gutterGrad);

                // Central Crease Line
                p.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
                p.drawLine(QPointF(halfW, 0), QPointF(halfW, spreadSize.height()));

                p.end();
                pm.save("/home/iredox/.gemini/antigravity-cli/brain/622a2e99-f95e-47a2-9196-72a33b1081da/render_dual_spread.png");
            }

            // C. OLED Pure Black Night Mode
            auto oledTokens = ReaderThemeTokens::oledBlack();
            auto oledPages = ts.paginate(aliceCh, pageSize, 0, oledTokens);
            if (!oledPages.isEmpty()) {
                QPixmap pm(pageSize);
                pm.fill(oledTokens.backgroundColor);
                QPainter p(&pm);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::TextAntialiasing, true);
                ts.renderPage(&p, QRectF(0, 0, pageSize.width(), pageSize.height()), oledPages[0], oledTokens, aliceDoc->title, aliceCh.title, false);
                p.end();
                pm.save("/home/iredox/.gemini/antigravity-cli/brain/622a2e99-f95e-47a2-9196-72a33b1081da/render_oled_black.png");
            }
        }

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

    void testPdfPageServiceAndRendering() {
        QString chessPath = "/home/iredox/books/1001 Chess Exercises for Beginners (Franco Masetti, Roberto Messa) (Z-Library).pdf";
        if (!QFile::exists(chessPath)) return;

        // 1. Verify Page Count via PdfPageService
        int pageCount = PdfPageService::instance().getPageCount(chessPath);
        QCOMPARE(pageCount, 126);

        // 2. Verify Rendering of Page 45 (the user's chessboard page)
        QByteArray page45Png = PdfPageService::instance().getPageImage(chessPath, "chess_test", 45, 120);
        QVERIFY(!page45Png.isEmpty());

        QPixmap pm;
        QVERIFY(pm.loadFromData(page45Png));
        QVERIFY(pm.width() > 400);
        QVERIFY(pm.height() > 500);

        // 3. Verify Parsing through PdfParser produces isImagePage for all pages
        PdfParser pdfParser;
        auto doc = pdfParser.parseFile(chessPath);
        QVERIFY(doc.has_value());
        QCOMPARE(doc->format, "pdf");
        QCOMPARE(doc->chapters.size(), 126);
        QVERIFY(doc->chapters[44].isImagePage); // Chapter 44 = Page 45
        QCOMPARE(doc->chapters[44].title, "Page 45");

        // 4. Verify Typesetter renders the chessboard page beautifully onto viewport
        Typesetter ts;
        ChapterContent ch44 = doc->chapters[44];
        ch44.imageBytes = page45Png;

        QSize viewSize(900, 1100);
        auto pages = ts.paginate(ch44, viewSize, 44, ReaderThemeTokens::parchment());
        QCOMPARE(pages.size(), 1);
        QVERIFY(pages[0].isImagePage);
        QVERIFY(!pages[0].imageBytes.isEmpty());

        QPixmap renderedView(viewSize);
        renderedView.fill(Qt::white);
        QPainter p(&renderedView);
        ts.renderPage(&p, QRectF(0, 0, viewSize.width(), viewSize.height()), pages[0], ReaderThemeTokens::parchment(), doc->title, ch44.title, false);
        p.end();

        // Save visual verification artifact
        renderedView.save("/home/iredox/.gemini/antigravity-cli/brain/622a2e99-f95e-47a2-9196-72a33b1081da/render_pdf_page_45_verified.png");
    }

    void testFb2MultiSectionAndMetadata() {
        QByteArray fb2Xml = 
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" xmlns:l=\"http://www.w3.org/1999/xlink\">\n"
            "  <description>\n"
            "    <title-info>\n"
            "      <author>\n"
            "        <first-name>Leo</first-name>\n"
            "        <middle-name>Nikolayevich</middle-name>\n"
            "        <last-name>Tolstoy</last-name>\n"
            "      </author>\n"
            "      <book-title>War and Peace</book-title>\n"
            "      <coverpage>\n"
            "        <image l:href=\"#cover.jpg\"/>\n"
            "      </coverpage>\n"
            "    </title-info>\n"
            "  </description>\n"
            "  <body>\n"
            "    <section>\n"
            "      <title><p>Chapter One</p></title>\n"
            "      <p>Well, Prince, so Genoa and Lucca are now just family estates of the Buonapartes.</p>\n"
            "    </section>\n"
            "    <section>\n"
            "      <title><p>Chapter Two</p></title>\n"
            "      <p>Anna Pavlovna drawing room was gradually filling.</p>\n"
            "      <image l:href=\"#drawing_room.png\"/>\n"
            "    </section>\n"
            "  </body>\n"
            "  <binary id=\"cover.jpg\" content-type=\"image/jpeg\">/9j/4AAQSkZJRg==</binary>\n"
            "  <binary id=\"drawing_room.png\" content-type=\"image/png\">iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=</binary>\n"
            "</FictionBook>";

        Fb2Parser parser;
        auto doc = parser.parseBytes(fb2Xml, "fb2", "war_and_peace.fb2");
        QVERIFY(doc.has_value());
        QCOMPARE(doc->title, "War and Peace");
        QCOMPARE(doc->author, "Leo Nikolayevich Tolstoy");
        QCOMPARE(doc->chapters.size(), 2);
        QCOMPARE(doc->chapters[0].title, "Chapter One");
        QCOMPARE(doc->chapters[1].title, "Chapter Two");
        QVERIFY(doc->chapters[1].htmlContent.contains("data:image/png;base64,"));
        QVERIFY(!doc->coverImageBytes.isEmpty());
        QCOMPARE(doc->toc.size(), 2);
        QCOMPARE(doc->toc[0].title, "Chapter One");
        QCOMPARE(doc->toc[1].title, "Chapter Two");
    }

    void testAnnotationExporterAnkiAndObsidian() {
        QList<AnnotationRecord> list;

        AnnotationRecord a1;
        a1.type = "highlight";
        a1.selectedText = "All happy families are alike; each unhappy family is unhappy in its own way.";
        a1.note = "Famous opening line.";
        a1.chapterIndex = 0;
        a1.pageIndex = 0;
        list.append(a1);

        AnnotationRecord a2;
        a2.type = "note";
        a2.selectedText = "Everything was in confusion in the Oblonskys' house.";
        a2.note = "Establishes chaos early on.";
        a2.chapterIndex = 0;
        a2.pageIndex = 1;
        list.append(a2);

        AnnotationRecord a3;
        a3.type = "strikethrough";
        a3.selectedText = "Outdated premise";
        a3.chapterIndex = 1;
        a3.pageIndex = 4;
        list.append(a3);

        QString csv = AnnotationExporter::exportToCsv("Anna Karenina", list);
        QVERIFY(csv.contains("Front,Back,Deck,Reference,Type"));
        QVERIFY(csv.contains("All happy families are alike"));
        QVERIFY(csv.contains("Anna Karenina"));
        QVERIFY(csv.contains("Ch 1, Pg 1"));

        QString obsidian = AnnotationExporter::exportToObsidian("Anna Karenina", list);
        QVERIFY(obsidian.contains("# 📖 Anna Karenina - Literature Notes"));
        QVERIFY(obsidian.contains("> [!quote] Chapter 1, Page 1"));
        QVERIFY(obsidian.contains("> [!note] Chapter 1, Page 2"));
        QVERIFY(obsidian.contains("> [!caution] Discarded/Contradicted"));
    }

    void testTypesetterSearchHighlighting() {
        Typesetter ts;
        ChapterContent ch;
        ch.index = 0;
        ch.title = "Chapter 1";
        ch.textContent = "The quick brown fox jumps over the lazy dog. The quick fox was nimble.";

        QSize viewSize(800, 600);
        auto pages = ts.paginate(ch, viewSize, 0, ReaderThemeTokens::modernLight());
        QVERIFY(!pages.isEmpty());

        QPixmap pm(viewSize);
        pm.fill(Qt::white);
        QPainter p(&pm);
        // Test rendering with active search term "quick"
        ts.renderPage(&p, QRectF(0, 0, viewSize.width(), viewSize.height()), pages[0], ReaderThemeTokens::modernLight(), "Test Book", "Chapter 1", false, "quick");
        p.end();

        QVERIFY(!pm.isNull());
    }
};

int runParsersTests() {
    TestParsers tc;
    return QTest::qExec(&tc);
}

#include "test_parsers.moc"
