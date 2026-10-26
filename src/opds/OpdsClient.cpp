#include "OpdsClient.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QRegularExpression>

namespace LunaRead {

// --- OpdsBookEntry Helper Methods ---

QString OpdsBookEntry::bestDownloadUrl(const QString& preferredMime) const {
    if (!preferredMime.isEmpty()) {
        for (const auto& link : acquisitionLinks) {
            if (link.type.compare(preferredMime, Qt::CaseInsensitive) == 0) {
                return link.href;
            }
        }
    }

    // 1. EPUB formats (standard reflowable format for e-readers)
    for (const auto& link : acquisitionLinks) {
        if (link.type.contains(QStringLiteral("epub"), Qt::CaseInsensitive)) {
            return link.href;
        }
    }

    // 2. MOBI / AZW / AZW3 formats
    for (const auto& link : acquisitionLinks) {
        if (link.type.contains(QStringLiteral("mobi"), Qt::CaseInsensitive) ||
            link.type.contains(QStringLiteral("azw"), Qt::CaseInsensitive) ||
            link.type.contains(QStringLiteral("amazon"), Qt::CaseInsensitive)) {
            return link.href;
        }
    }

    // 3. PDF formats
    for (const auto& link : acquisitionLinks) {
        if (link.type.contains(QStringLiteral("pdf"), Qt::CaseInsensitive)) {
            return link.href;
        }
    }

    // 4. FictionBook (FB2)
    for (const auto& link : acquisitionLinks) {
        if (link.type.contains(QStringLiteral("fb2"), Qt::CaseInsensitive) ||
            link.type.contains(QStringLiteral("fictionbook"), Qt::CaseInsensitive)) {
            return link.href;
        }
    }

    // 5. Comic formats (CBZ / CBR)
    for (const auto& link : acquisitionLinks) {
        if (link.type.contains(QStringLiteral("cbz"), Qt::CaseInsensitive) ||
            link.type.contains(QStringLiteral("cbr"), Qt::CaseInsensitive) ||
            link.type.contains(QStringLiteral("comicbook"), Qt::CaseInsensitive)) {
            return link.href;
        }
    }

    // 6. Plain text / HTML documents
    for (const auto& link : acquisitionLinks) {
        if (link.type.contains(QStringLiteral("text/plain"), Qt::CaseInsensitive) ||
            link.type.contains(QStringLiteral("text/html"), Qt::CaseInsensitive)) {
            return link.href;
        }
    }

    // 7. Fallback to first available acquisition link
    if (!acquisitionLinks.isEmpty()) {
        return acquisitionLinks.first().href;
    }

    return QString();
}

QString OpdsBookEntry::acquisitionUrlForType(const QString& mimeType) const {
    if (mimeType.isEmpty()) return QString();
    for (const auto& link : acquisitionLinks) {
        if (link.type.compare(mimeType, Qt::CaseInsensitive) == 0) {
            return link.href;
        }
    }
    return QString();
}

QStringList OpdsBookEntry::categoryLabels() const {
    QStringList labels;
    for (const auto& cat : categories) {
        if (!cat.label.isEmpty()) {
            labels.append(cat.label);
        } else if (!cat.term.isEmpty()) {
            labels.append(cat.term);
        }
    }
    return labels;
}

// --- OpdsClient Implementation ---

OpdsClient::OpdsClient(QObject* parent) : QObject(parent) {
    m_net = new QNetworkAccessManager(this);
}

QString OpdsClient::resolveUrl(const QString& rawUrl, const QUrl& baseUrl) {
    if (rawUrl.isEmpty()) return QString();
    QUrl target(rawUrl);
    if (!target.isRelative()) {
        return rawUrl;
    }
    if (baseUrl.isValid() && !baseUrl.isEmpty()) {
        return baseUrl.resolved(target).toString();
    }
    return rawUrl;
}

OpdsAuthor OpdsClient::parseAuthorElement(QXmlStreamReader& xml) {
    OpdsAuthor author;
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::EndElement && xml.name() == QLatin1String("author")) {
            break;
        }
        if (token != QXmlStreamReader::StartElement) continue;

        QStringView name = xml.name();
        if (name == QLatin1String("name")) {
            author.name = xml.readElementText().trimmed();
        } else if (name == QLatin1String("uri") || name == QLatin1String("url")) {
            author.uri = xml.readElementText().trimmed();
        } else if (name == QLatin1String("email")) {
            author.email = xml.readElementText().trimmed();
        } else {
            xml.skipCurrentElement();
        }
    }
    return author;
}

OpdsLink OpdsClient::parseLinkElement(QXmlStreamReader& xml, const QUrl& baseUrl) {
    OpdsLink link;
    const auto attrs = xml.attributes();
    link.rel = attrs.value(QLatin1String("rel")).toString().trimmed();
    QString href = attrs.value(QLatin1String("href")).toString().trimmed();
    link.href = resolveUrl(href, baseUrl);
    link.type = attrs.value(QLatin1String("type")).toString().trimmed();
    link.title = attrs.value(QLatin1String("title")).toString().trimmed();

    if (attrs.hasAttribute(QLatin1String("length"))) {
        bool ok = false;
        qint64 len = attrs.value(QLatin1String("length")).toLongLong(&ok);
        if (ok) link.length = len;
    }

    xml.skipCurrentElement();
    return link;
}

OpdsBookEntry OpdsClient::parseEntryElement(QXmlStreamReader& xml, const QUrl& baseUrl) {
    OpdsBookEntry entry;
    QUrl entryBaseUrl = baseUrl;

    if (xml.attributes().hasAttribute(QLatin1String("xml:base"))) {
        entryBaseUrl = entryBaseUrl.resolved(QUrl(xml.attributes().value(QLatin1String("xml:base")).toString()));
    }

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::EndElement && xml.name() == QLatin1String("entry")) {
            break;
        }
        if (token != QXmlStreamReader::StartElement) continue;

        QStringView name = xml.name();
        QString qualifiedName = xml.qualifiedName().toString();

        if (name == QLatin1String("id")) {
            entry.id = xml.readElementText().trimmed();
        } else if (name == QLatin1String("title")) {
            entry.title = xml.readElementText().trimmed();
        } else if (name == QLatin1String("author")) {
            entry.authors.append(parseAuthorElement(xml));
        } else if (name == QLatin1String("creator") || qualifiedName == QLatin1String("dc:creator")) {
            QString creator = xml.readElementText().trimmed();
            if (!creator.isEmpty()) {
                OpdsAuthor a;
                a.name = creator;
                entry.authors.append(a);
            }
        } else if (name == QLatin1String("summary")) {
            entry.summary = xml.readElementText().trimmed();
        } else if (name == QLatin1String("content")) {
            entry.contentType = xml.attributes().value(QLatin1String("type")).toString().trimmed();
            entry.content = xml.readElementText().trimmed();
            if (entry.summary.isEmpty()) {
                entry.summary = entry.content;
            }
        } else if (name == QLatin1String("updated")) {
            QString dt = xml.readElementText().trimmed();
            entry.updated = QDateTime::fromString(dt, Qt::ISODate);
            if (!entry.updated.isValid()) {
                entry.updated = QDateTime::fromString(dt, Qt::ISODateWithMs);
            }
        } else if (name == QLatin1String("published") || name == QLatin1String("issued") || qualifiedName == QLatin1String("dc:issued")) {
            QString dt = xml.readElementText().trimmed();
            entry.published = QDateTime::fromString(dt, Qt::ISODate);
            if (!entry.published.isValid()) {
                entry.published = QDateTime::fromString(dt, QStringLiteral("yyyy-MM-dd"));
                if (!entry.published.isValid()) {
                    entry.published = QDateTime::fromString(dt, QStringLiteral("yyyy"));
                }
            }
        } else if (name == QLatin1String("category")) {
            OpdsCategory cat;
            const auto attrs = xml.attributes();
            cat.term = attrs.value(QLatin1String("term")).toString().trimmed();
            cat.label = attrs.value(QLatin1String("label")).toString().trimmed();
            if (cat.label.isEmpty()) {
                cat.label = cat.term;
            }
            cat.scheme = attrs.value(QLatin1String("scheme")).toString().trimmed();
            entry.categories.append(cat);
            xml.skipCurrentElement();
        } else if (name == QLatin1String("language") || qualifiedName == QLatin1String("dc:language")) {
            entry.language = xml.readElementText().trimmed();
        } else if (name == QLatin1String("publisher") || qualifiedName == QLatin1String("dc:publisher")) {
            entry.publisher = xml.readElementText().trimmed();
        } else if (name == QLatin1String("rights") || qualifiedName == QLatin1String("dc:rights")) {
            entry.rights = xml.readElementText().trimmed();
        } else if (name == QLatin1String("identifier") || qualifiedName == QLatin1String("dc:identifier")) {
            entry.identifier = xml.readElementText().trimmed();
        } else if (name == QLatin1String("link")) {
            OpdsLink link = parseLinkElement(xml, entryBaseUrl);
            entry.links.append(link);

            if (link.isAcquisition()) {
                entry.acquisitionLinks.append(link);
            } else if (link.isThumbnail()) {
                entry.thumbnailUrl = link.href;
            } else if (link.isImage()) {
                entry.coverUrl = link.href;
            }

            if (link.isSubsection() || link.isNavigation()) {
                entry.isNavigation = true;
            }
        } else {
            xml.skipCurrentElement();
        }
    }

    // Construct primary author string
    QStringList authorNames;
    for (const auto& a : entry.authors) {
        if (!a.name.isEmpty()) {
            authorNames.append(a.name);
        }
    }
    entry.author = authorNames.join(QStringLiteral(", "));

    // Cover and thumbnail fallback synchronization
    if (entry.coverUrl.isEmpty() && !entry.thumbnailUrl.isEmpty()) {
        entry.coverUrl = entry.thumbnailUrl;
    } else if (entry.thumbnailUrl.isEmpty() && !entry.coverUrl.isEmpty()) {
        entry.thumbnailUrl = entry.coverUrl;
    }

    // Select primary download URL
    entry.downloadUrl = entry.bestDownloadUrl();

    return entry;
}

void OpdsClient::categorizeFeedLink(OpdsFeed& feed, const OpdsLink& link) {
    if (link.rel == QLatin1String("self")) {
        feed.selfUrl = link.href;
    } else if (link.rel == QLatin1String("next")) {
        feed.nextUrl = link.href;
    } else if (link.rel == QLatin1String("prev") || link.rel == QLatin1String("previous")) {
        feed.prevUrl = link.href;
    } else if (link.rel == QLatin1String("first")) {
        feed.firstUrl = link.href;
    } else if (link.rel == QLatin1String("last")) {
        feed.lastUrl = link.href;
    } else if (link.isSearch()) {
        feed.searchUrl = link.href;
    } else if (link.rel == QLatin1String("start")) {
        feed.startUrl = link.href;
    } else if (link.rel == QLatin1String("up")) {
        feed.upUrl = link.href;
    }

    // Add navigation subsection link (excluding self, start, up, search, and pagination)
    if (link.isSubsection() ||
        link.rel.contains(QLatin1String("sort"), Qt::CaseInsensitive) ||
        link.rel.contains(QLatin1String("facet"), Qt::CaseInsensitive)) {
        OpdsNavigationLink nav;
        nav.title = link.title.isEmpty() ? link.rel : link.title;
        nav.href = link.href;
        nav.type = link.type;
        nav.rel = link.rel;
        feed.navigationLinks.append(nav);
    }
}

OpdsFeed OpdsClient::parseFeedXml(const QByteArray& xmlData, const QUrl& baseUrl) {
    OpdsFeed feed;
    if (xmlData.isEmpty()) {
        return feed;
    }

    QXmlStreamReader xml(xmlData);
    QUrl currentBaseUrl = baseUrl;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token != QXmlStreamReader::StartElement) continue;

        QStringView name = xml.name();
        if (name == QLatin1String("feed")) {
            if (xml.attributes().hasAttribute(QLatin1String("xml:base"))) {
                currentBaseUrl = currentBaseUrl.resolved(QUrl(xml.attributes().value(QLatin1String("xml:base")).toString()));
            }

            while (!xml.atEnd() && !xml.hasError()) {
                QXmlStreamReader::TokenType feedToken = xml.readNext();
                if (feedToken == QXmlStreamReader::EndElement && xml.name() == QLatin1String("feed")) {
                    break;
                }
                if (feedToken != QXmlStreamReader::StartElement) continue;

                QStringView feedElem = xml.name();
                if (feedElem == QLatin1String("id")) {
                    feed.id = xml.readElementText().trimmed();
                } else if (feedElem == QLatin1String("title")) {
                    feed.title = xml.readElementText().trimmed();
                } else if (feedElem == QLatin1String("subtitle")) {
                    feed.subtitle = xml.readElementText().trimmed();
                } else if (feedElem == QLatin1String("icon") || feedElem == QLatin1String("logo")) {
                    QString iconHref = xml.readElementText().trimmed();
                    feed.iconUrl = resolveUrl(iconHref, currentBaseUrl);
                } else if (feedElem == QLatin1String("updated")) {
                    QString dt = xml.readElementText().trimmed();
                    feed.updated = QDateTime::fromString(dt, Qt::ISODate);
                    if (!feed.updated.isValid()) {
                        feed.updated = QDateTime::fromString(dt, Qt::ISODateWithMs);
                    }
                } else if (feedElem == QLatin1String("author")) {
                    feed.author = parseAuthorElement(xml);
                } else if (feedElem == QLatin1String("link")) {
                    OpdsLink link = parseLinkElement(xml, currentBaseUrl);
                    feed.links.append(link);
                    categorizeFeedLink(feed, link);
                } else if (feedElem == QLatin1String("entry")) {
                    OpdsBookEntry entry = parseEntryElement(xml, currentBaseUrl);
                    feed.entries.append(entry);

                    // If entry represents navigation (e.g. subsection or has no acquisition link and has navigation links)
                    if (entry.isNavigation || entry.acquisitionLinks.isEmpty()) {
                        for (const auto& l : entry.links) {
                            if (l.isSubsection() || l.isNavigation() || l.rel == QLatin1String("alternate")) {
                                OpdsNavigationLink nav;
                                nav.id = entry.id;
                                nav.title = entry.title;
                                nav.href = l.href;
                                nav.type = l.type;
                                nav.rel = l.rel;
                                nav.summary = entry.summary;
                                nav.iconUrl = entry.thumbnailUrl.isEmpty() ? entry.coverUrl : entry.thumbnailUrl;
                                feed.navigationLinks.append(nav);
                                break;
                            }
                        }
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else {
            xml.skipCurrentElement();
        }
    }

    return feed;
}

OpdsFeed OpdsClient::parseFeedXml(const QString& xmlString, const QUrl& baseUrl) {
    return parseFeedXml(xmlString.toUtf8(), baseUrl);
}

void OpdsClient::fetchCatalog(const QString& url, const QString& user, const QString& pass) {
    fetchCatalog(QUrl(url), user, pass);
}

void OpdsClient::fetchCatalog(const QUrl& url, const QString& user, const QString& pass) {
    if (!url.isValid()) {
        emit errorOccurred(QStringLiteral("Invalid OPDS catalog URL: %1").arg(url.toString()));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("LunaRead-Qt/1.0 (Moon+ Reader Compatible OPDS Client; Linux)"));
    request.setRawHeader("Accept", "application/atom+xml,application/xml,text/xml,*/*");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    if (!user.isEmpty()) {
        QString token = QStringLiteral("%1:%2").arg(user, pass);
        request.setRawHeader("Authorization", "Basic " + token.toUtf8().toBase64());
    }

    QNetworkReply* reply = m_net->get(request);
    m_activeReplies.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        m_activeReplies.removeAll(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }

        QUrl effectiveUrl = reply->url();
        if (!effectiveUrl.isValid()) {
            effectiveUrl = url;
        }

        QByteArray data = reply->readAll();
        OpdsFeed feed = parseFeedXml(data, effectiveUrl);

        emit feedLoaded(feed);
        emit catalogLoaded(feed.entries);
    });
}

void OpdsClient::fetchNextPage(const OpdsFeed& feed, const QString& user, const QString& pass) {
    if (!feed.hasNext()) {
        emit errorOccurred(QStringLiteral("Feed does not have a next page"));
        return;
    }
    fetchCatalog(QUrl(feed.nextUrl), user, pass);
}

void OpdsClient::downloadBook(const QString& downloadUrl, const QString& destinationFilePath,
                              const QString& user, const QString& pass) {
    QUrl url(downloadUrl);
    if (!url.isValid()) {
        emit errorOccurred(QStringLiteral("Invalid download URL: %1").arg(downloadUrl));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("LunaRead-Qt/1.0 (Moon+ Reader Compatible OPDS Client; Linux)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    if (!user.isEmpty()) {
        QString token = QStringLiteral("%1:%2").arg(user, pass);
        request.setRawHeader("Authorization", "Basic " + token.toUtf8().toBase64());
    }

    QNetworkReply* reply = m_net->get(request);
    m_activeReplies.append(reply);

    connect(reply, &QNetworkReply::downloadProgress, this, &OpdsClient::downloadProgress);

    connect(reply, &QNetworkReply::finished, this, [this, reply, destinationFilePath]() {
        m_activeReplies.removeAll(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QStringLiteral("Download error: %1").arg(reply->errorString()));
            return;
        }

        QFileInfo fi(destinationFilePath);
        QDir().mkpath(fi.absolutePath());

        QFile outFile(destinationFilePath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            emit errorOccurred(QStringLiteral("Could not open destination file for writing: %1").arg(destinationFilePath));
            return;
        }

        outFile.write(reply->readAll());
        outFile.close();

        emit downloadFinished(destinationFilePath);
    });
}

void OpdsClient::cancelAllRequests() {
    for (auto* reply : m_activeReplies) {
        if (reply && reply->isRunning()) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_activeReplies.clear();
}

} // namespace LunaRead
