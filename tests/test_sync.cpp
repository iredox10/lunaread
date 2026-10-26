#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include "../src/sync/WebDavSyncService.h"
#include "../src/utils/HashUtils.h"

using namespace LunaRead;

class TestSync : public QObject {
    Q_OBJECT

private slots:
    // 1. Verify Moon+ Reader sync file path format
    void testSyncFilePathFormat() {
        const QString hash = QStringLiteral("4a7d1ed414474e4033ac29ccb8653d9b");
        const QString expectedPath = QStringLiteral(".moonreader/4a7d1ed414474e4033ac29ccb8653d9b.po");

        QString path = WebDavSyncService::syncFilePath(hash);
        QCOMPARE(path, expectedPath);

        // Verify consistency with HashUtils helper
        QString utilsPath = HashUtils::getMoonReaderSyncFileName(hash);
        QCOMPARE(path, utilsPath);
    }

    // 2. Verify JSON payload generation, schema, and deserialization
    void testJsonPayloadGenerationAndDeserialization() {
        SyncPayload original;
        original.bookHash = QStringLiteral("4a7d1ed414474e4033ac29ccb8653d9b");
        original.title = QStringLiteral("The Count of Monte Cristo");
        original.chapterIndex = 14;
        original.pageIndex = 3;
        original.characterOffset = 1520;
        original.progressPercentage = 42.875;
        original.percentage = 42.875;
        original.timestamp = 1761314400000ULL; // UTC timestamp
        original.deviceName = QStringLiteral("LunaRead Desktop Qt");

        QByteArray jsonData = WebDavSyncService::serializeJson(original);
        QVERIFY(!jsonData.isEmpty());

        // Validate JSON structure directly
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseErr);
        QCOMPARE(parseErr.error, QJsonParseError::NoError);
        QVERIFY(doc.isObject());

        QJsonObject obj = doc.object();
        QCOMPARE(obj.value(QStringLiteral("bookHash")).toString(), original.bookHash);
        QCOMPARE(obj.value(QStringLiteral("title")).toString(), original.title);
        QCOMPARE(obj.value(QStringLiteral("chapterIndex")).toInt(), original.chapterIndex);
        QCOMPARE(obj.value(QStringLiteral("pageIndex")).toInt(), original.pageIndex);
        QCOMPARE(obj.value(QStringLiteral("characterOffset")).toInt(), original.characterOffset);
        QCOMPARE(obj.value(QStringLiteral("progressPercentage")).toDouble(), original.progressPercentage);
        QCOMPARE(static_cast<qint64>(obj.value(QStringLiteral("timestamp")).toDouble()), original.timestamp);
        QCOMPARE(obj.value(QStringLiteral("deviceName")).toString(), original.deviceName);

        // Deserialize back into SyncPayload
        auto deserializedOpt = WebDavSyncService::deserializeJson(jsonData);
        QVERIFY(deserializedOpt.has_value());

        const SyncPayload& parsed = *deserializedOpt;
        QCOMPARE(parsed.bookHash, original.bookHash);
        QCOMPARE(parsed.title, original.title);
        QCOMPARE(parsed.chapterIndex, original.chapterIndex);
        QCOMPARE(parsed.pageIndex, original.pageIndex);
        QCOMPARE(parsed.characterOffset, original.characterOffset);
        QCOMPARE(parsed.progressPercentage, original.progressPercentage);
        QCOMPARE(parsed.percentage, original.percentage);
        QCOMPARE(parsed.timestamp, original.timestamp);
        QCOMPARE(parsed.deviceName, original.deviceName);
    }

    // 3. Verify snake_case JSON deserialization compatibility
    void testSnakeCaseJsonDeserialization() {
        QByteArray snakeJson = R"({
            "book_hash": "e99a18c428cb38d5f260853678922e03",
            "book_title": "Moby Dick",
            "chapter_index": 5,
            "page_index": 2,
            "character_offset": 840,
            "progress_percentage": 18.5,
            "timestamp": 1761315000000,
            "device_name": "Moon+ Reader Pro Android"
        })";

        auto opt = WebDavSyncService::deserializeJson(snakeJson);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->bookHash, QStringLiteral("e99a18c428cb38d5f260853678922e03"));
        QCOMPARE(opt->title, QStringLiteral("Moby Dick"));
        QCOMPARE(opt->chapterIndex, 5);
        QCOMPARE(opt->pageIndex, 2);
        QCOMPARE(opt->characterOffset, 840);
        QCOMPARE(opt->progressPercentage, 18.5);
        QCOMPARE(opt->percentage, 18.5);
        QCOMPARE(opt->timestamp, 1761315000000LL);
        QCOMPARE(opt->deviceName, QStringLiteral("Moon+ Reader Pro Android"));
    }

    // 4. Verify ISO-8601 timestamp string parsing in JSON
    void testIsoTimestampJsonDeserialization() {
        QByteArray isoJson = R"({
            "bookHash": "hash123",
            "progressPercentage": 50.0,
            "chapterIndex": 1,
            "pageIndex": 0,
            "characterOffset": 0,
            "timestamp": "2026-10-24T16:00:00Z",
            "deviceName": "WebClient"
        })";

        auto opt = WebDavSyncService::deserializeJson(isoJson);
        QVERIFY(opt.has_value());
        qint64 expectedMs = QDateTime::fromString(QStringLiteral("2026-10-24T16:00:00Z"), Qt::ISODate).toMSecsSinceEpoch();
        QCOMPARE(opt->timestamp, expectedMs);
    }

    // 5. Verify Key-Value payload generation and deserialization
    void testKeyValuePayloadGenerationAndDeserialization() {
        SyncPayload original;
        original.bookHash = QStringLiteral("7d0a8b3c1e2f4a5b6c7d8e9f0a1b2c3d");
        original.title = QStringLiteral("Pride and Prejudice");
        original.chapterIndex = 8;
        original.pageIndex = 4;
        original.characterOffset = 620;
        original.progressPercentage = 35.5;
        original.timestamp = 1761316000000LL;
        original.deviceName = QStringLiteral("Moon+ Phone");

        QByteArray kvData = WebDavSyncService::serializeKeyValue(original);
        QVERIFY(!kvData.isEmpty());
        QVERIFY(kvData.contains("bookHash=7d0a8b3c1e2f4a5b6c7d8e9f0a1b2c3d"));
        QVERIFY(kvData.contains("chapterIndex=8"));
        QVERIFY(kvData.contains("progressPercentage=35.5000"));
        QVERIFY(kvData.contains("timestamp=1761316000000"));

        auto opt = WebDavSyncService::deserializeKeyValue(kvData);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->bookHash, original.bookHash);
        QCOMPARE(opt->title, original.title);
        QCOMPARE(opt->chapterIndex, original.chapterIndex);
        QCOMPARE(opt->pageIndex, original.pageIndex);
        QCOMPARE(opt->characterOffset, original.characterOffset);
        QCOMPARE(opt->progressPercentage, original.progressPercentage);
        QCOMPARE(opt->timestamp, original.timestamp);
        QCOMPARE(opt->deviceName, original.deviceName);
    }

    // 6. Verify colon-separated Key-Value payload parsing
    void testColonSeparatedKeyValueDeserialization() {
        QByteArray colonKv =
            "BookHash: test_hash_999\n"
            "ChapterIndex: 12\n"
            "PageIndex: 7\n"
            "CharacterOffset: 340\n"
            "ProgressPercentage: 88.2\n"
            "Timestamp: 1761317000000\n"
            "DeviceName: Tablet\n";

        auto opt = WebDavSyncService::deserializeKeyValue(colonKv);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->bookHash, QStringLiteral("test_hash_999"));
        QCOMPARE(opt->chapterIndex, 12);
        QCOMPARE(opt->pageIndex, 7);
        QCOMPARE(opt->characterOffset, 340);
        QCOMPARE(opt->progressPercentage, 88.2);
        QCOMPARE(opt->timestamp, 1761317000000LL);
        QCOMPARE(opt->deviceName, QStringLiteral("Tablet"));
    }

    // 7. Verify Auto-detect deserialization (JSON vs Key-Value)
    void testAutoDetectDeserialization() {
        // Test JSON auto-detection
        QByteArray json = R"({"bookHash":"abc","chapterIndex":1,"pageIndex":2,"characterOffset":30,"progressPercentage":10.0,"timestamp":1000,"deviceName":"DevA"})";
        auto optJson = WebDavSyncService::deserialize(json);
        QVERIFY(optJson.has_value());
        QCOMPARE(optJson->bookHash, QStringLiteral("abc"));
        QCOMPARE(optJson->chapterIndex, 1);

        // Test Key-Value auto-detection
        QByteArray kv = "bookHash=xyz\nchapterIndex=4\npageIndex=1\ncharacterOffset=90\nprogressPercentage=50.0\ntimestamp=2000\ndeviceName=DevB\n";
        auto optKv = WebDavSyncService::deserialize(kv);
        QVERIFY(optKv.has_value());
        QCOMPARE(optKv->bookHash, QStringLiteral("xyz"));
        QCOMPARE(optKv->chapterIndex, 4);

        // Test Empty / Invalid
        QVERIFY(!WebDavSyncService::deserialize("").has_value());
        QVERIFY(!WebDavSyncService::deserialize("   ").has_value());
    }

    // 8. Verify Last-Write-Wins (LWW) conflict resolution logic
    void testConflictResolutionLWW() {
        SyncPayload local;
        local.bookHash = QStringLiteral("book_hash_123");
        local.chapterIndex = 2;
        local.pageIndex = 5;
        local.characterOffset = 100;
        local.progressPercentage = 25.0;
        local.timestamp = 1761310000000LL; // Earlier
        local.deviceName = QStringLiteral("Local Desktop");

        SyncPayload remote;
        remote.bookHash = QStringLiteral("book_hash_123");
        remote.chapterIndex = 4;
        remote.pageIndex = 1;
        remote.characterOffset = 250;
        remote.progressPercentage = 45.0;
        remote.timestamp = 1761320000000LL; // Later (newer)
        remote.deviceName = QStringLiteral("Remote Android");

        // Case A: Remote has newer timestamp -> Remote wins
        QCOMPARE(WebDavSyncService::resolveConflict(local, remote), ConflictResolution::UseRemote);
        QCOMPARE(WebDavSyncService::resolveLww(local, remote), remote);
        QVERIFY(WebDavSyncService::shouldApplyRemote(local, remote));

        // Case B: Local has newer timestamp -> Local wins
        local.timestamp = 1761330000000LL; // Newer than remote
        QCOMPARE(WebDavSyncService::resolveConflict(local, remote), ConflictResolution::UseLocal);
        QCOMPARE(WebDavSyncService::resolveLww(local, remote), local);
        QVERIFY(!WebDavSyncService::shouldApplyRemote(local, remote));

        // Case C: Timestamps identical, progress identical -> InSync
        remote.timestamp = local.timestamp;
        remote.chapterIndex = local.chapterIndex;
        remote.pageIndex = local.pageIndex;
        remote.characterOffset = local.characterOffset;
        remote.progressPercentage = local.progressPercentage;
        QCOMPARE(WebDavSyncService::resolveConflict(local, remote), ConflictResolution::InSync);

        // Case D: Timestamps identical, but remote has higher progress -> Remote wins
        remote.progressPercentage = local.progressPercentage + 5.0;
        QCOMPARE(WebDavSyncService::resolveConflict(local, remote), ConflictResolution::UseRemote);

        // Case E: Timestamps identical, but local has higher progress -> Local wins
        local.progressPercentage = remote.progressPercentage + 10.0;
        QCOMPARE(WebDavSyncService::resolveConflict(local, remote), ConflictResolution::UseLocal);
    }

    // 9. Verify Credentials and Base URL formatting
    void testCredentialsAndUrl() {
        WebDavSyncService service;
        service.setCredentials(QStringLiteral("https://cloud.example.com/webdav"),
                               QStringLiteral("testuser"),
                               QStringLiteral("p@ssword123"));

        QCOMPARE(service.baseUrl(), QStringLiteral("https://cloud.example.com/webdav/"));

        // Basic auth = base64("testuser:p@ssword123")
        QString expectedToken = QStringLiteral("Basic ") + QStringLiteral("testuser:p@ssword123").toUtf8().toBase64();
        QCOMPARE(service.authHeader(), expectedToken);
    }

    // 10. Verify Payload Normalization and Helper Methods
    void testPayloadNormalization() {
        SyncPayload p1;
        p1.progressPercentage = 75.5;
        p1.normalize();
        QCOMPARE(p1.percentage, 75.5);

        SyncPayload p2;
        p2.percentage = 82.3;
        p2.normalize();
        QCOMPARE(p2.progressPercentage, 82.3);

        SyncPayload p3;
        p3.setProgress(91.0);
        QCOMPARE(p3.progressPercentage, 91.0);
        QCOMPARE(p3.percentage, 91.0);
    }
};

int runSyncTests() {
    TestSync tc;
    return QTest::qExec(&tc);
}

#include "test_sync.moc"
