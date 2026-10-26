#include <QObject>
#include <QTest>
#include "../src/utils/NaturalSort.h"
#include "../src/utils/EncodingDetector.h"
#include "../src/utils/HashUtils.h"

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
};

int runUtilsTests() {
    TestUtils tc;
    return QTest::qExec(&tc);
}

#include "test_utils.moc"
