#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include <QPushButton>
#include "../src/utils/NaturalSort.h"
#include "../src/utils/EncodingDetector.h"
#include "../src/utils/HashUtils.h"
#include "../src/features/DictionaryService.h"
#include "../src/ui/DictionaryDialog.h"
#include "../src/reader/ReadingHeaderFooterWidget.h"

using namespace LunaRead;

class TestUtils : public QObject {
    Q_OBJECT
private slots:
    void testNaturalSort() {
        QCOMPARE(NaturalSort::naturalCompare("page_1.png", "page_2.png"), true);
        QCOMPARE(NaturalSort::naturalCompare("page_2.png", "page_10.png"), true);
        QCOMPARE(NaturalSort::naturalCompare("page_10.png", "page_2.png"), false);
        QCOMPARE(NaturalSort::naturalCompare("Chapter 1", "Chapter 01"), false);
    }

    void testEncodingDetector() {
        QByteArray utf8Bom("\xEF\xBB\xBFHello World", 14);
        QString dec = EncodingDetector::decodeBytes(utf8Bom);
        QCOMPARE(dec, "Hello World");

        QByteArray ascii("Standard ASCII text");
        QCOMPARE(EncodingDetector::decodeBytes(ascii), "Standard ASCII text");
    }

    void testHashUtils() {
        QByteArray data("Hello LunaRead");
        QString md5 = HashUtils::calculateBytesMd5(data);
        QCOMPARE(md5.length(), 32);

        QString syncName = HashUtils::getMoonReaderSyncFileName(md5);
        QCOMPARE(syncName, QString(".moonreader/%1.po").arg(md5));
    }

    void testDictionaryService() {
        DictionaryService service;
        auto entry = service.lookup("serendipity");
        QCOMPARE(entry.found, true);
        QCOMPARE(entry.word, "serendipity");
        QCOMPARE(entry.partOfSpeech, "noun");
        QVERIFY(!entry.phonetic.isEmpty());
        QVERIFY(!entry.definition.isEmpty());
        QVERIFY(!entry.example.isEmpty());
        QVERIFY(!entry.synonyms.isEmpty());

        auto unknown = service.lookup("nonexistentword123");
        QCOMPARE(unknown.found, false);

        auto frEntry = service.lookup("flâneur", "fr");
        QCOMPARE(frEntry.found, true);
        QCOMPARE(frEntry.language, "fr");
        QVERIFY(frEntry.synonyms.contains("promeneur"));
    }

    void testDictionaryDialog() {
        DictionaryService service;

        // Constructor 1: word + service ref
        DictionaryDialog dlg1("serendipity", service);
        QCOMPARE(dlg1.word(), "serendipity");
        QCOMPARE(dlg1.currentEntry().found, true);
        QVERIFY(!dlg1.currentEntry().definition.isEmpty());

        // Constructor 2: word + lang + service ref
        DictionaryDialog dlg2("flâneur", "fr", service);
        QCOMPARE(dlg2.word(), "flâneur");
        QCOMPARE(dlg2.language(), "fr");
        QCOMPARE(dlg2.currentEntry().found, true);

        // Constructor 3: word + service ptr
        DictionaryDialog dlg3("melancholy", &service);
        QCOMPARE(dlg3.word(), "melancholy");
        QCOMPARE(dlg3.currentEntry().found, true);

        // Signal verification
        QSignalSpy spy(&dlg1, &DictionaryDialog::wordLookedUp);
        dlg1.lookup("ephemeral", "en");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(dlg1.word(), "ephemeral");

        // Pronounce button signal
        QSignalSpy pronounceSpy(&dlg1, &DictionaryDialog::pronounceRequested);
        auto* pronounceBtn = dlg1.findChild<QPushButton*>("pronounceBtn");
        QVERIFY(pronounceBtn != nullptr);
        pronounceBtn->click();
        QCOMPARE(pronounceSpy.count(), 1);

        // Theme switching
        dlg1.applyTheme(ReaderThemeTokens::night());
        dlg1.applyTheme(ReaderThemeTokens::sepia());
        dlg1.applyTheme(ReaderThemeTokens::parchment());
        dlg1.applyTheme(ReaderThemeTokens::oledBlack());
        dlg1.applyTheme(ReaderThemeTokens::modernLight());
    }

    void testReadingHeaderFooterWidget() {
        ReadingHeaderFooterWidget hud;
        hud.resize(800, 600);

        // 1. Initial State
        QCOMPARE(hud.bookTitle(), QString());
        QCOMPARE(hud.chapterTitle(), QString());
        QCOMPARE(hud.theme().id, QStringLiteral("parchment"));

        // 2. Titles
        hud.setBookTitle("Moby Dick");
        hud.setChapterTitle("Chapter 1: Loomings");
        QCOMPARE(hud.bookTitle(), QStringLiteral("Moby Dick"));
        QCOMPARE(hud.chapterTitle(), QStringLiteral("Chapter 1: Loomings"));

        // 3. updateStats: curPage=42, totalPages=310, curChapter=3, totalChapters=20, pct=34.5, wpm=200, wordsLeft=2400
        hud.updateStats(42, 310, 3, 20, 34.5, 200, 2400);
        QCOMPARE(hud.currentPage(), 42);
        QCOMPARE(hud.totalPages(), 310);
        QCOMPARE(hud.currentChapter(), 3);
        QCOMPARE(hud.totalChapters(), 20);
        QCOMPARE(hud.progressPercentage(), 34.5);
        QCOMPARE(hud.wordsPerMinute(), 200);
        QCOMPARE(hud.wordsLeftInChapter(), 2400);

        // Verify Header Status Bar time left ("12 mins left")
        QCOMPARE(hud.timeLeftText(), QStringLiteral("12 mins left"));

        // Verify Footer Status Bar page progress ("Page 42 of 310")
        QCOMPARE(hud.pageProgressText(), QStringLiteral("Page 42 of 310"));

        // Verify Footer Status Bar total progress ("34.5%")
        QCOMPARE(hud.totalProgressText(), QStringLiteral("34.5%"));

        // 4. Time left edge cases
        hud.updateStats(1, 10, 1, 1, 10.0, 200, 0);
        QCOMPARE(hud.timeLeftText(), QStringLiteral("0 mins left"));

        hud.updateStats(1, 10, 1, 1, 10.0, 200, 50);
        QCOMPARE(hud.timeLeftText(), QStringLiteral("< 1 min left"));

        hud.updateStats(1, 10, 1, 1, 10.0, 200, 200);
        QCOMPARE(hud.timeLeftText(), QStringLiteral("1 min left"));

        hud.updateStats(1, 10, 1, 1, 10.0, 200, 15000); // 75 mins -> 1h 15m left
        QCOMPARE(hud.timeLeftText(), QStringLiteral("1h 15m left"));

        // 5. System Clock & Battery
        hud.setClockOverride("14:35");
        QCOMPARE(hud.clockText(), QStringLiteral("14:35"));
        hud.clearClockOverride();
        QVERIFY(!hud.clockText().isEmpty());

        hud.setBatteryOverride(75, false, false, true);
        QCOMPARE(hud.batteryInfo().percentage, 75);
        QCOMPARE(hud.batteryInfo().isCharging, false);
        QCOMPARE(hud.batteryInfo().hasBattery, true);

        hud.setBatteryOverride(100, false, true, false); // 100% AC
        QCOMPARE(hud.batteryInfo().percentage, 100);
        QCOMPARE(hud.batteryInfo().isAcConnected, true);
        QCOMPARE(hud.batteryInfo().hasBattery, false);

        hud.clearBatteryOverride();

        // 6. Theme Adaptation
        hud.applyTheme(ReaderThemeTokens::oledBlack());
        QCOMPARE(hud.theme().id, QStringLiteral("oledBlack"));

        hud.setThemeMode(ReaderThemeMode::Sepia);
        QCOMPARE(hud.theme().id, QStringLiteral("sepia"));

        hud.setThemeMode(ReaderThemeMode::Night);
        QCOMPARE(hud.theme().id, QStringLiteral("night"));

        hud.setThemeMode(ReaderThemeMode::ModernLight);
        QCOMPARE(hud.theme().id, QStringLiteral("modernLight"));

        // 7. Visibility & Sub-widgets
        hud.setHeaderVisible(false);
        QCOMPARE(hud.isHeaderVisible(), false);
        hud.setHeaderVisible(true);
        QCOMPARE(hud.isHeaderVisible(), true);

        hud.setFooterVisible(false);
        QCOMPARE(hud.isFooterVisible(), false);
        hud.setFooterVisible(true);
        QCOMPARE(hud.isFooterVisible(), true);

        // 8. Geometry & Painting Test
        hud.resize(1024, 768);
        QVERIFY(hud.headerWidget() != nullptr);
        QVERIFY(hud.footerWidget() != nullptr);
        QCOMPARE(hud.headerWidget()->geometry().y(), 0);
        QCOMPARE(hud.footerWidget()->geometry().bottom(), 767);

        QPixmap testPix(1024, 768);
        testPix.fill(Qt::transparent);
        hud.render(&testPix);
        QVERIFY(!testPix.isNull());

        // 9. Progress seek signal
        QSignalSpy seekSpy(&hud, &ReadingHeaderFooterWidget::progressSeekRequested);
        // Simulate click at x=512, y=height-4 on footer
        QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(512, 24), QPointF(512, 764), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(hud.footerWidget(), &pressEv);
        QCOMPARE(seekSpy.count(), 1);
        double soughtPct = seekSpy.takeFirst().at(0).toDouble();
        QCOMPARE(std::round(soughtPct), 50.0);
    }
};

int runUtilsTests() {
    TestUtils tc;
    return QTest::qExec(&tc);
}

#include "test_utils.moc"
