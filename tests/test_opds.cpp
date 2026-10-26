#include <QObject>
#include <QTest>
#include <QUrl>
#include <QDateTime>
#include "../src/opds/OpdsClient.h"

using namespace LunaRead;

class TestOpds : public QObject {
    Q_OBJECT

private slots:
    void testStandardEbooksFeedParsing();
    void testProjectGutenbergFeedParsing();
    void testNavigationFeedAndSubsections();
    void testPaginationNextPrevLinks();
    void testRelativeUrlResolution();
    void testAcquisitionFormatPrioritization();
    void testAuthorExtraction();
    void testCategoryParsing();
    void testMalformedXmlHandling();
    void testCoverAndThumbnailFallbacks();
};

void TestOpds::testStandardEbooksFeedParsing() {
    const QString standardEbooksXml = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom"
      xmlns:dc="http://purl.org/dc/terms/"
      xmlns:opds="http://opds-spec.org/2010/catalog">
  <id>https://standardebooks.org/opds/all</id>
  <title>Standard Ebooks - All</title>
  <subtitle>Free and liberated ebooks, carefully produced for the true book lover.</subtitle>
  <icon>https://standardebooks.org/images/logo.png</icon>
  <updated>2024-01-15T10:30:00Z</updated>
  <author>
    <name>Standard Ebooks</name>
    <uri>https://standardebooks.org</uri>
    <email>feeds@standardebooks.org</email>
  </author>
  <link rel="self" href="https://standardebooks.org/opds/all" type="application/atom+xml;profile=opds-catalog;kind=acquisition"/>
  <link rel="start" href="https://standardebooks.org/opds" type="application/atom+xml;profile=opds-catalog;kind=navigation" title="Standard Ebooks Root"/>
  <link rel="next" href="https://standardebooks.org/opds/all?page=2" type="application/atom+xml;profile=opds-catalog;kind=acquisition"/>
  <link rel="search" href="https://standardebooks.org/opds/search" type="application/opensearchdescription+xml" title="Search Standard Ebooks"/>
  <link rel="subsection" href="https://standardebooks.org/opds/subjects" type="application/atom+xml;profile=opds-catalog;kind=navigation" title="Subjects"/>

  <entry>
    <id>https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice</id>
    <title>Pride and Prejudice</title>
    <author>
      <name>Jane Austen</name>
      <uri>https://en.wikipedia.org/wiki/Jane_Austen</uri>
    </author>
    <updated>2023-11-20T18:45:00Z</updated>
    <published>1813-01-28</published>
    <summary>A romantic novel of manners written by Jane Austen in 1813.</summary>
    <content type="html">&lt;p&gt;Pride and Prejudice is the second published novel by Jane Austen.&lt;/p&gt;</content>
    <dc:language>en</dc:language>
    <dc:publisher>Standard Ebooks</dc:publisher>
    <dc:issued>1813-01-28</dc:issued>
    <category term="Fiction" scheme="http://www.bisg.org/standards/bisac_subject/" label="Fiction"/>
    <category term="Romance" label="Romance"/>
    <category term="Classics" label="Classics"/>
    
    <link rel="http://opds-spec.org/acquisition"
          href="https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/jane-austen_pride-and-prejudice.epub"
          type="application/epub+zip"
          title="Recommended compatible epub"
          length="1285320"/>
    <link rel="http://opds-spec.org/acquisition/open-access"
          href="https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/jane-austen_pride-and-prejudice.azw3"
          type="application/x-mobipocket-ebook"
          title="Amazon Kindle format"
          length="1498112"/>
    <link rel="http://opds-spec.org/acquisition"
          href="https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/jane-austen_pride-and-prejudice.kepub.epub"
          type="application/kepub+zip"
          title="Kobo kepub format"
          length="1301044"/>
    
    <link rel="http://opds-spec.org/image"
          href="https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/cover.jpg"
          type="image/jpeg"/>
    <link rel="http://opds-spec.org/image/thumbnail"
          href="https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/thumbnail.jpg"
          type="image/jpeg"/>
  </entry>
</feed>
)xml");

    OpdsFeed feed = OpdsClient::parseFeedXml(standardEbooksXml);

    // Verify Feed Level Metadata
    QCOMPARE(feed.id, QStringLiteral("https://standardebooks.org/opds/all"));
    QCOMPARE(feed.title, QStringLiteral("Standard Ebooks - All"));
    QCOMPARE(feed.subtitle, QStringLiteral("Free and liberated ebooks, carefully produced for the true book lover."));
    QCOMPARE(feed.iconUrl, QStringLiteral("https://standardebooks.org/images/logo.png"));
    QCOMPARE(feed.author.name, QStringLiteral("Standard Ebooks"));
    QCOMPARE(feed.author.uri, QStringLiteral("https://standardebooks.org"));
    QCOMPARE(feed.author.email, QStringLiteral("feeds@standardebooks.org"));

    // Verify Feed Links and Pagination
    QCOMPARE(feed.selfUrl, QStringLiteral("https://standardebooks.org/opds/all"));
    QCOMPARE(feed.startUrl, QStringLiteral("https://standardebooks.org/opds"));
    QCOMPARE(feed.nextUrl, QStringLiteral("https://standardebooks.org/opds/all?page=2"));
    QCOMPARE(feed.searchUrl, QStringLiteral("https://standardebooks.org/opds/search"));
    QVERIFY(feed.hasNext());
    QVERIFY(!feed.hasPrev());
    QVERIFY(feed.isAcquisitionFeed());
    QCOMPARE(feed.totalEntries(), 1);

    // Verify Book Entry Metadata
    const OpdsBookEntry& book = feed.entries.first();
    QCOMPARE(book.id, QStringLiteral("https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice"));
    QCOMPARE(book.title, QStringLiteral("Pride and Prejudice"));
    QCOMPARE(book.author, QStringLiteral("Jane Austen"));
    QCOMPARE(book.authors.size(), 1);
    QCOMPARE(book.authors.first().name, QStringLiteral("Jane Austen"));
    QCOMPARE(book.authors.first().uri, QStringLiteral("https://en.wikipedia.org/wiki/Jane_Austen"));
    QCOMPARE(book.summary, QStringLiteral("A romantic novel of manners written by Jane Austen in 1813."));
    QCOMPARE(book.contentType, QStringLiteral("html"));
    QVERIFY(book.content.contains(QStringLiteral("second published novel")));
    QCOMPARE(book.language, QStringLiteral("en"));
    QCOMPARE(book.publisher, QStringLiteral("Standard Ebooks"));

    // Verify Categories
    QCOMPARE(book.categories.size(), 3);
    QCOMPARE(book.categories[0].term, QStringLiteral("Fiction"));
    QCOMPARE(book.categories[0].label, QStringLiteral("Fiction"));
    QCOMPARE(book.categories[0].scheme, QStringLiteral("http://www.bisg.org/standards/bisac_subject/"));
    QCOMPARE(book.categoryLabels(), QStringList({QStringLiteral("Fiction"), QStringLiteral("Romance"), QStringLiteral("Classics")}));

    // Verify Images
    QCOMPARE(book.coverUrl, QStringLiteral("https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/cover.jpg"));
    QCOMPARE(book.thumbnailUrl, QStringLiteral("https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/thumbnail.jpg"));

    // Verify Acquisition Links & Best Download Selection
    QCOMPARE(book.acquisitionLinks.size(), 3);
    QCOMPARE(book.downloadUrl, QStringLiteral("https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/jane-austen_pride-and-prejudice.epub"));
    QCOMPARE(book.acquisitionUrlForType("application/x-mobipocket-ebook"),
             QStringLiteral("https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/dist/jane-austen_pride-and-prejudice.azw3"));
    QCOMPARE(book.acquisitionLinks[0].length, 1285320);
}

void TestOpds::testProjectGutenbergFeedParsing() {
    const QString gutenbergXml = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom"
      xmlns:dc="http://purl.org/dc/elements/1.1/"
      xmlns:opds="http://opds-spec.org/2010/catalog">
  <id>http://m.gutenberg.org/ebooks/</id>
  <title>Project Gutenberg Mobile Catalog</title>
  <updated>2024-02-01T00:00:00Z</updated>
  <link rel="self" href="http://m.gutenberg.org/ebooks/?format=opds" type="application/atom+xml"/>
  <link rel="next" href="http://m.gutenberg.org/ebooks/?format=opds&amp;page=2" type="application/atom+xml"/>
  
  <entry>
    <id>http://www.gutenberg.org/ebooks/84</id>
    <title>Frankenstein; Or, The Modern Prometheus</title>
    <author>
      <name>Shelley, Mary Wollstonecraft</name>
      <uri>http://www.gutenberg.org/authors/61</uri>
    </author>
    <published>1993-10-01</published>
    <updated>2024-01-20T12:00:00Z</updated>
    <summary>Frankenstein is a Gothic thriller and science fiction novel.</summary>
    <dc:language>en</dc:language>
    <dc:publisher>Project Gutenberg</dc:publisher>
    <category term="Science fiction" label="Science fiction"/>
    <category term="Gothic fiction" label="Gothic fiction"/>

    <link rel="http://opds-spec.org/acquisition"
          href="https://www.gutenberg.org/ebooks/84.epub3.images"
          type="application/epub+zip"
          title="EPUB3 (with images)"
          length="1053210"/>
    <link rel="http://opds-spec.org/acquisition"
          href="https://www.gutenberg.org/ebooks/84.epub.noimages"
          type="application/epub+zip"
          title="EPUB (no images)"
          length="358240"/>
    <link rel="http://opds-spec.org/acquisition"
          href="https://www.gutenberg.org/ebooks/84.kindle.images"
          type="application/x-mobipocket-ebook"
          title="Kindle (with images)"
          length="1583200"/>
    <link rel="http://opds-spec.org/acquisition"
          href="https://www.gutenberg.org/files/84/84-pdf.pdf"
          type="application/pdf"
          title="PDF document"
          length="2100500"/>
          
    <link rel="http://opds-spec.org/image"
          href="https://www.gutenberg.org/cache/epub/84/pg84.cover.medium.jpg"
          type="image/jpeg"/>
    <link rel="http://opds-spec.org/image/thumbnail"
          href="https://www.gutenberg.org/cache/epub/84/pg84.cover.small.jpg"
          type="image/jpeg"/>
  </entry>
</feed>
)xml");

    OpdsFeed feed = OpdsClient::parseFeedXml(gutenbergXml);

    QCOMPARE(feed.title, QStringLiteral("Project Gutenberg Mobile Catalog"));
    QCOMPARE(feed.entries.size(), 1);

    const OpdsBookEntry& book = feed.entries.first();
    QCOMPARE(book.title, QStringLiteral("Frankenstein; Or, The Modern Prometheus"));
    QCOMPARE(book.author, QStringLiteral("Shelley, Mary Wollstonecraft"));
    QCOMPARE(book.language, QStringLiteral("en"));
    QCOMPARE(book.publisher, QStringLiteral("Project Gutenberg"));
    QCOMPARE(book.coverUrl, QStringLiteral("https://www.gutenberg.org/cache/epub/84/pg84.cover.medium.jpg"));
    QCOMPARE(book.thumbnailUrl, QStringLiteral("https://www.gutenberg.org/cache/epub/84/pg84.cover.small.jpg"));

    // EPUB should be prioritized for downloadUrl
    QCOMPARE(book.downloadUrl, QStringLiteral("https://www.gutenberg.org/ebooks/84.epub3.images"));

    // Specific format acquisition check
    QCOMPARE(book.acquisitionUrlForType("application/pdf"),
             QStringLiteral("https://www.gutenberg.org/files/84/84-pdf.pdf"));
}

void TestOpds::testNavigationFeedAndSubsections() {
    const QString navFeedXml = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom"
      xmlns:opds="http://opds-spec.org/2010/catalog">
  <id>urn:catalog:root</id>
  <title>Moon+ Reader OPDS Catalog Root</title>
  <updated>2024-01-01T00:00:00Z</updated>
  <link rel="self" href="https://books.example.com/opds" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  <link rel="start" href="https://books.example.com/opds" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  <link rel="subsection" href="https://books.example.com/opds/featured" type="application/atom+xml;profile=opds-catalog;kind=acquisition" title="Featured Releases"/>

  <entry>
    <id>urn:catalog:section:fiction</id>
    <title>Fiction &amp; Literature</title>
    <summary>Novels, short stories, and classical literature</summary>
    <updated>2024-01-01T00:00:00Z</updated>
    <link rel="subsection"
          href="https://books.example.com/opds/fiction"
          type="application/atom+xml;profile=opds-catalog;kind=acquisition"
          title="Fiction Catalog"/>
  </entry>
  <entry>
    <id>urn:catalog:section:scifi</id>
    <title>Science Fiction &amp; Fantasy</title>
    <summary>Cyberpunk, space opera, and epic fantasy</summary>
    <updated>2024-01-01T00:00:00Z</updated>
    <link rel="subsection"
          href="https://books.example.com/opds/scifi"
          type="application/atom+xml;profile=opds-catalog;kind=acquisition"
          title="Sci-Fi Catalog"/>
  </entry>
  <entry>
    <id>urn:catalog:section:popular</id>
    <title>Popular Downloads</title>
    <summary>Most downloaded books</summary>
    <updated>2024-01-01T00:00:00Z</updated>
    <link rel="http://opds-spec.org/sort/popular"
          href="https://books.example.com/opds/popular"
          type="application/atom+xml;profile=opds-catalog;kind=acquisition"
          title="Popular Books"/>
  </entry>
</feed>
)xml");

    OpdsFeed feed = OpdsClient::parseFeedXml(navFeedXml);

    QCOMPARE(feed.title, QStringLiteral("Moon+ Reader OPDS Catalog Root"));
    QCOMPARE(feed.entries.size(), 3);

    // Feed level subsection + 3 entry subsections = 4 navigation links total
    QCOMPARE(feed.navigationLinks.size(), 4);
    QCOMPARE(feed.navigationLinks[0].title, QStringLiteral("Featured Releases"));
    QCOMPARE(feed.navigationLinks[0].href, QStringLiteral("https://books.example.com/opds/featured"));

    QCOMPARE(feed.navigationLinks[1].title, QStringLiteral("Fiction & Literature"));
    QCOMPARE(feed.navigationLinks[1].href, QStringLiteral("https://books.example.com/opds/fiction"));
    QCOMPARE(feed.navigationLinks[1].rel, QStringLiteral("subsection"));

    QCOMPARE(feed.navigationLinks[2].title, QStringLiteral("Science Fiction & Fantasy"));
    QCOMPARE(feed.navigationLinks[2].href, QStringLiteral("https://books.example.com/opds/scifi"));

    QCOMPARE(feed.navigationLinks[3].title, QStringLiteral("Popular Downloads"));
    QCOMPARE(feed.navigationLinks[3].href, QStringLiteral("https://books.example.com/opds/popular"));

    // Check entry properties in navigation feed
    QVERIFY(feed.entries[0].isNavigation);
    QVERIFY(feed.entries[1].isNavigation);
    QVERIFY(feed.entries[2].isNavigation);
    QVERIFY(feed.entries[0].acquisitionLinks.isEmpty());
}

void TestOpds::testPaginationNextPrevLinks() {
    const QString paginatedXml = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <id>urn:test:page2</id>
  <title>Page 2 of Books</title>
  <link rel="first" href="https://example.com/opds?page=1" type="application/atom+xml"/>
  <link rel="previous" href="https://example.com/opds?page=1" type="application/atom+xml"/>
  <link rel="self" href="https://example.com/opds?page=2" type="application/atom+xml"/>
  <link rel="next" href="https://example.com/opds?page=3" type="application/atom+xml"/>
  <link rel="last" href="https://example.com/opds?page=10" type="application/atom+xml"/>
</feed>
)xml");

    OpdsFeed feed = OpdsClient::parseFeedXml(paginatedXml);

    QCOMPARE(feed.firstUrl, QStringLiteral("https://example.com/opds?page=1"));
    QCOMPARE(feed.prevUrl, QStringLiteral("https://example.com/opds?page=1"));
    QCOMPARE(feed.selfUrl, QStringLiteral("https://example.com/opds?page=2"));
    QCOMPARE(feed.nextUrl, QStringLiteral("https://example.com/opds?page=3"));
    QCOMPARE(feed.lastUrl, QStringLiteral("https://example.com/opds?page=10"));
    QVERIFY(feed.hasNext());
    QVERIFY(feed.hasPrev());
}

void TestOpds::testRelativeUrlResolution() {
    const QString relativeXml = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <id>urn:catalog:relative</id>
  <title>Catalog with Relative URLs</title>
  <icon>icons/logo.png</icon>
  <link rel="self" href="feed.xml"/>
  <link rel="next" href="page2.xml"/>
  <link rel="subsection" href="/categories/scifi"/>

  <entry>
    <id>urn:book:1</id>
    <title>Relative Book</title>
    <link rel="http://opds-spec.org/acquisition" href="downloads/book1.epub" type="application/epub+zip"/>
    <link rel="http://opds-spec.org/image" href="covers/book1.jpg" type="image/jpeg"/>
  </entry>
</feed>
)xml");

    QUrl baseUrl(QStringLiteral("https://catalog.example.org/opds/feed.xml"));
    OpdsFeed feed = OpdsClient::parseFeedXml(relativeXml, baseUrl);

    QCOMPARE(feed.iconUrl, QStringLiteral("https://catalog.example.org/opds/icons/logo.png"));
    QCOMPARE(feed.selfUrl, QStringLiteral("https://catalog.example.org/opds/feed.xml"));
    QCOMPARE(feed.nextUrl, QStringLiteral("https://catalog.example.org/opds/page2.xml"));
    QCOMPARE(feed.navigationLinks.first().href, QStringLiteral("https://catalog.example.org/categories/scifi"));

    const OpdsBookEntry& book = feed.entries.first();
    QCOMPARE(book.downloadUrl, QStringLiteral("https://catalog.example.org/opds/downloads/book1.epub"));
    QCOMPARE(book.coverUrl, QStringLiteral("https://catalog.example.org/opds/covers/book1.jpg"));
    QCOMPARE(book.thumbnailUrl, QStringLiteral("https://catalog.example.org/opds/covers/book1.jpg"));
}

void TestOpds::testAcquisitionFormatPrioritization() {
    OpdsBookEntry entry;
    
    OpdsLink pdfLink;
    pdfLink.rel = QStringLiteral("http://opds-spec.org/acquisition");
    pdfLink.href = QStringLiteral("https://example.com/book.pdf");
    pdfLink.type = QStringLiteral("application/pdf");

    OpdsLink epubLink;
    epubLink.rel = QStringLiteral("http://opds-spec.org/acquisition");
    epubLink.href = QStringLiteral("https://example.com/book.epub");
    epubLink.type = QStringLiteral("application/epub+zip");

    OpdsLink mobiLink;
    mobiLink.rel = QStringLiteral("http://opds-spec.org/acquisition");
    mobiLink.href = QStringLiteral("https://example.com/book.mobi");
    mobiLink.type = QStringLiteral("application/x-mobipocket-ebook");

    // Order added: PDF, MOBI, EPUB
    entry.acquisitionLinks = {pdfLink, mobiLink, epubLink};

    // EPUB should be chosen by default despite being added last
    QCOMPARE(entry.bestDownloadUrl(), QStringLiteral("https://example.com/book.epub"));

    // Explicit preference override
    QCOMPARE(entry.bestDownloadUrl("application/pdf"), QStringLiteral("https://example.com/book.pdf"));
    QCOMPARE(entry.bestDownloadUrl("application/x-mobipocket-ebook"), QStringLiteral("https://example.com/book.mobi"));
}

void TestOpds::testAuthorExtraction() {
    const QString xml = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:dc="http://purl.org/dc/terms/">
  <entry>
    <id>urn:multi:author</id>
    <title>Co-authored Masterpiece</title>
    <author>
      <name>Alice Smith</name>
      <uri>https://alice.example.com</uri>
      <email>alice@example.com</email>
    </author>
    <author>
      <name>Bob Jones</name>
    </author>
  </entry>
  <entry>
    <id>urn:dc:creator</id>
    <title>Dublin Core Book</title>
    <dc:creator>Charles Dickens</dc:creator>
  </entry>
</feed>
)xml");

    OpdsFeed feed = OpdsClient::parseFeedXml(xml);
    QCOMPARE(feed.entries.size(), 2);

    // Multiple authors joined
    const auto& entry1 = feed.entries[0];
    QCOMPARE(entry1.authors.size(), 2);
    QCOMPARE(entry1.authors[0].name, QStringLiteral("Alice Smith"));
    QCOMPARE(entry1.authors[0].uri, QStringLiteral("https://alice.example.com"));
    QCOMPARE(entry1.authors[0].email, QStringLiteral("alice@example.com"));
    QCOMPARE(entry1.authors[1].name, QStringLiteral("Bob Jones"));
    QCOMPARE(entry1.author, QStringLiteral("Alice Smith, Bob Jones"));

    // DC creator fallback
    const auto& entry2 = feed.entries[1];
    QCOMPARE(entry2.author, QStringLiteral("Charles Dickens"));
}

void TestOpds::testCategoryParsing() {
    const QString xml = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <entry>
    <id>urn:category:test</id>
    <title>Categorized Book</title>
    <category term="FIC000000" scheme="http://bisg.org" label="General Fiction"/>
    <category term="Mystery"/>
  </entry>
</feed>
)xml");

    OpdsFeed feed = OpdsClient::parseFeedXml(xml);
    QCOMPARE(feed.entries.size(), 1);

    const auto& entry = feed.entries.first();
    QCOMPARE(entry.categories.size(), 2);
    QCOMPARE(entry.categories[0].term, QStringLiteral("FIC000000"));
    QCOMPARE(entry.categories[0].label, QStringLiteral("General Fiction"));
    QCOMPARE(entry.categories[0].scheme, QStringLiteral("http://bisg.org"));

    // Second category has term only, label should default to term
    QCOMPARE(entry.categories[1].term, QStringLiteral("Mystery"));
    QCOMPARE(entry.categories[1].label, QStringLiteral("Mystery"));

    QCOMPARE(entry.categoryLabels(), QStringList({QStringLiteral("General Fiction"), QStringLiteral("Mystery")}));
}

void TestOpds::testMalformedXmlHandling() {
    // Empty data
    OpdsFeed emptyFeed = OpdsClient::parseFeedXml(QByteArray());
    QVERIFY(emptyFeed.entries.isEmpty());
    QVERIFY(emptyFeed.title.isEmpty());

    // Non-XML garbage
    OpdsFeed garbageFeed = OpdsClient::parseFeedXml(QByteArray("This is not XML at all!"));
    QVERIFY(garbageFeed.entries.isEmpty());

    // Incomplete / truncated XML
    OpdsFeed truncatedFeed = OpdsClient::parseFeedXml(QByteArray("<feed><title>Truncated<entry><id>1"));
    // Should safely parse without crash
    QCOMPARE(truncatedFeed.title, QStringLiteral("Truncated"));
}

void TestOpds::testCoverAndThumbnailFallbacks() {
    // Only Cover specified -> Thumbnail falls back to Cover
    const QString xmlCoverOnly = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <entry>
    <id>1</id>
    <title>Cover Only</title>
    <link rel="http://opds-spec.org/image" href="https://example.com/cover.jpg" type="image/jpeg"/>
  </entry>
</feed>
)xml");

    OpdsFeed feed1 = OpdsClient::parseFeedXml(xmlCoverOnly);
    QCOMPARE(feed1.entries.first().coverUrl, QStringLiteral("https://example.com/cover.jpg"));
    QCOMPARE(feed1.entries.first().thumbnailUrl, QStringLiteral("https://example.com/cover.jpg"));

    // Only Thumbnail specified -> Cover falls back to Thumbnail
    const QString xmlThumbOnly = QStringLiteral(R"xml(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <entry>
    <id>2</id>
    <title>Thumbnail Only</title>
    <link rel="http://opds-spec.org/image/thumbnail" href="https://example.com/thumb.jpg" type="image/jpeg"/>
  </entry>
</feed>
)xml");

    OpdsFeed feed2 = OpdsClient::parseFeedXml(xmlThumbOnly);
    QCOMPARE(feed2.entries.first().thumbnailUrl, QStringLiteral("https://example.com/thumb.jpg"));
    QCOMPARE(feed2.entries.first().coverUrl, QStringLiteral("https://example.com/thumb.jpg"));
}

#ifdef STANDALONE_OPDS_TEST
QTEST_MAIN(TestOpds)
#else
int runOpdsTests() {
    TestOpds tc;
    return QTest::qExec(&tc);
}
#endif

#include "test_opds.moc"
