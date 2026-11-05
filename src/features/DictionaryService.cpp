#include "DictionaryService.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <algorithm>

namespace LunaRead {

DictionaryService::DictionaryService(QObject* parent) : QObject(parent) {
    initBuiltInLexicon();
    openDatabase();
}

DictionaryService::~DictionaryService() {
    if (m_dbOpen) {
        m_dictDb.close();
    }
    m_dictDb = QSqlDatabase();
    if (!m_connectionName.isEmpty() && QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

void DictionaryService::openDatabase() {
    const QStringList candidatePaths = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/dictionary/lunaread_dict.db"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/assets/dictionary/lunaread_dict.db"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../assets/dictionary/lunaread_dict.db"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../share/lunaread/dictionary/lunaread_dict.db"),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/dictionary/lunaread_dict.db"),
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/dictionary/lunaread_dict.db"),
        QStringLiteral("/home/iredox/Desktop/space/lunaread-desktop-qt/assets/dictionary/lunaread_dict.db"),
        QStringLiteral("/usr/share/lunaread/dictionary/lunaread_dict.db")
    };

    QString foundPath;
    for (const auto& p : candidatePaths) {
        if (QFileInfo::exists(p)) {
            foundPath = p;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        return;
    }

    m_connectionName = QStringLiteral("lunaread_dict_%1").arg(reinterpret_cast<quintptr>(this));
    m_dictDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_dictDb.setDatabaseName(foundPath);
    m_dictDb.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    m_dbOpen = m_dictDb.open();
}

int DictionaryService::totalWordsCount() const {
    if (!m_dbOpen) {
        return static_cast<int>(m_lexicon.size());
    }
    QSqlQuery q(m_dictDb);
    if (q.exec(QStringLiteral("SELECT count(*) FROM entries;")) && q.next()) {
        return q.value(0).toInt();
    }
    return static_cast<int>(m_lexicon.size());
}

QString DictionaryService::sanitizeWord(const QString& rawWord) const {
    QString s = rawWord.trimmed();
    static const QRegularExpression leadTrail(R"(^[\s"“”'‘’\(\)\[\]\{\}\<\>\.,:;!?\-_—*#\\\/]+|[\s"“”'‘’\(\)\[\]\{\}\<\>\.,:;!?\-_—*#\\\/]+$)");
    s.remove(leadTrail);
    return s.trimmed().toLower();
}

QStringList DictionaryService::generateLemmas(const QString& word) const {
    QStringList lemmas;
    if (word.length() <= 3) return lemmas;

    // Possessive 's
    if (word.endsWith(QStringLiteral("'s"))) {
        lemmas.append(word.left(word.length() - 2));
    }

    // -ies -> -y (e.g. technologies -> technology, capabilities -> capability)
    if (word.endsWith(QStringLiteral("ies")) && word.length() > 4) {
        lemmas.append(word.left(word.length() - 3) + QStringLiteral("y"));
    }

    // -es (e.g. processes -> process, boxes -> box)
    if (word.endsWith(QStringLiteral("es")) && word.length() > 3) {
        lemmas.append(word.left(word.length() - 2));
        lemmas.append(word.left(word.length() - 1));
    }

    // -s (e.g. models -> model)
    if (word.endsWith(QLatin1Char('s')) && !word.endsWith(QStringLiteral("ss")) && word.length() > 3) {
        lemmas.append(word.left(word.length() - 1));
    }

    // -ing (e.g. training -> train, reading -> read, computing -> compute)
    if (word.endsWith(QStringLiteral("ing")) && word.length() > 4) {
        QString stem = word.left(word.length() - 3);
        lemmas.append(stem);
        lemmas.append(stem + QLatin1Char('e'));
        if (stem.length() >= 3 && stem.at(stem.length() - 1) == stem.at(stem.length() - 2)) {
            lemmas.append(stem.left(stem.length() - 1)); // stopping -> stop
        }
    }

    // -ed (e.g. trained -> train, applied -> apply)
    if (word.endsWith(QStringLiteral("ed")) && word.length() > 3) {
        if (word.endsWith(QStringLiteral("ied")) && word.length() > 4) {
            lemmas.append(word.left(word.length() - 3) + QStringLiteral("y"));
        }
        lemmas.append(word.left(word.length() - 1)); // -d (e.g. compute -> computed)
        lemmas.append(word.left(word.length() - 2)); // -ed (e.g. train -> trained)
        QString stem = word.left(word.length() - 2);
        if (stem.length() >= 3 && stem.at(stem.length() - 1) == stem.at(stem.length() - 2)) {
            lemmas.append(stem.left(stem.length() - 1)); // stopped -> stop
        }
    }

    // -ly (e.g. quickly -> quick, happily -> happy)
    if (word.endsWith(QStringLiteral("ly")) && word.length() > 4) {
        if (word.endsWith(QStringLiteral("ily")) && word.length() > 4) {
            lemmas.append(word.left(word.length() - 3) + QStringLiteral("y"));
        }
        lemmas.append(word.left(word.length() - 2));
    }

    // -er / -est
    if (word.endsWith(QStringLiteral("er")) && word.length() > 4) {
        lemmas.append(word.left(word.length() - 2));
        lemmas.append(word.left(word.length() - 1));
    }
    if (word.endsWith(QStringLiteral("est")) && word.length() > 5) {
        lemmas.append(word.left(word.length() - 3));
        lemmas.append(word.left(word.length() - 2));
    }

    lemmas.removeDuplicates();
    return lemmas;
}

DictEntry DictionaryService::queryDatabase(const QString& cleanWord, const QString& lang) {
    if (!m_dbOpen) return {};

    QSqlQuery q(m_dictDb);
    q.prepare(QStringLiteral(
        "SELECT word, pos, phonetic, definition, example, synonyms, language "
        "FROM entries WHERE word = ? AND (language = ? OR language = 'en') LIMIT 1;"
    ));
    q.addBindValue(cleanWord);
    q.addBindValue(lang);

    if (q.exec() && q.next()) {
        DictEntry entry;
        entry.word = q.value(0).toString();
        entry.partOfSpeech = q.value(1).toString();
        entry.phonetic = q.value(2).toString();
        entry.definition = q.value(3).toString();
        entry.example = q.value(4).toString();
        const QString synStr = q.value(5).toString();
        if (!synStr.isEmpty()) {
            for (const auto& s : synStr.split(QLatin1Char(';'))) {
                QString item = s.trimmed();
                if (!item.isEmpty()) entry.synonyms.append(item);
            }
        }
        entry.language = q.value(6).toString();
        entry.found = true;
        return entry;
    }

    return {};
}

void DictionaryService::initBuiltInLexicon() {
    // Curated high-frequency English vocabulary with rich phonetics, examples, and synonyms
    m_lexicon["serendipity"] = {
        "serendipity", "/ˌsɛrənˈdɪpɪti/", "noun",
        "The occurrence and development of events by chance in a happy or beneficial way.",
        "A fortunate stroke of serendipity brought them together.",
        {"fluke", "happy chance", "providence", "coincidence", "fortune"},
        "en", true
    };

    m_lexicon["ephemeral"] = {
        "ephemeral", "/ɪˈfɛmərəl/", "adjective",
        "Lasting for a very short time; transient.",
        "Fashions are ephemeral, but true style endures.",
        {"transient", "fleeting", "momentary", "short-lived", "passing"},
        "en", true
    };

    m_lexicon["soliloquy"] = {
        "soliloquy", "/səˈlɪləkwi/", "noun",
        "An act of speaking one's thoughts aloud when by oneself, especially by a character in a play.",
        "Hamlet's famous soliloquy begins with 'To be, or not to be'.",
        {"monologue", "speech", "aside", "address"},
        "en", true
    };

    m_lexicon["melancholy"] = {
        "melancholy", "/ˈmɛlənkəli/", "noun",
        "A feeling of pensive sadness, typically with no obvious cause.",
        "An air of melancholy pervaded the quiet autumn garden.",
        {"sadness", "sorrow", "despondency", "gloom", "pensiveness"},
        "en", true
    };

    m_lexicon["petrichor"] = {
        "petrichor", "/ˈpɛtrɪkɔːr/", "noun",
        "A pleasant, earthy scent that frequently accompanies the first rain after a long period of warm, dry weather.",
        "The sweet petrichor rose from the dry pavement as the summer storm broke.",
        {"earthy fragrance", "rain scent", "ozone"},
        "en", true
    };

    m_lexicon["eloquence"] = {
        "eloquence", "/ˈɛləkwəns/", "noun",
        "Fluent or persuasive speaking or writing.",
        "The senator spoke with fiery eloquence that swayed the entire auditorium.",
        {"fluency", "articulation", "oratory", "rhetoric", "expressiveness"},
        "en", true
    };

    m_lexicon["resilient"] = {
        "resilient", "/rɪˈzɪliənt/", "adjective",
        "Able to withstand or recover quickly from difficult conditions.",
        "The resilient villagers rebuilt their homes after the harsh winter.",
        {"tough", "hardy", "tenacious", "adaptable", "buoyant"},
        "en", true
    };

    m_lexicon["luminous"] = {
        "luminous", "/ˈluːmɪnəs/", "adjective",
        "Full of or shedding light; bright or shining, especially in the dark.",
        "The luminous dials on the instrument panel glowed softly in the cockpit.",
        {"radiant", "shining", "brilliant", "glowing", "gleaming"},
        "en", true
    };

    m_lexicon["nostalgia"] = {
        "nostalgia", "/nɒˈstældʒə/", "noun",
        "A sentimental longing or wistful affection for a period in the past.",
        "A wave of nostalgic longing swept over him as he opened the high school yearbook.",
        {"reminiscence", "wistfulness", "yearning", "longing"},
        "en", true
    };

    m_lexicon["quintessential"] = {
        "quintessential", "/ˌkwɪntɪˈsɛnʃəl/", "adjective",
        "Representing the most perfect or typical example of a quality or class.",
        "The quaint tea shop was the quintessential English country cottage.",
        {"archetypal", "prototypical", "exemplary", "classic", "ideal"},
        "en", true
    };

    m_lexicon["transformer"] = {
        "transformer", "/trænsˈfɔːrmər/", "noun",
        "In deep learning, a neural network architecture based on self-attention mechanisms that processes sequential data in parallel, forming the foundation of modern Large Language Models.",
        "The transformer architecture replaced recurrent models in natural language processing.",
        {"attention network", "foundation model"},
        "en", true
    };

    m_lexicon["token"] = {
        "token", "/ˈtoʊkən/", "noun",
        "A fundamental unit of text (such as a word, subword, or character) processed by a language model during training and inference.",
        "The prompt was split into 256 tokens by the byte-pair encoder.",
        {"subword", "symbol", "linguistic unit"},
        "en", true
    };

    m_multiLangLexicon["en"] = m_lexicon;

    // French Lexicon ("fr")
    QMap<QString, DictEntry> frLexicon;
    frLexicon["flâneur"] = {
        "flâneur", "/flɑːˈnœːʁ/", "nom",
        "Personne qui se promène sans but précis, pour le plaisir d'observer le spectacle de la rue.",
        "Le flâneur parcourait les boulevards parisiens à la tombée de la nuit.",
        {"promeneur", "badaud", "marcheur"},
        "fr", true
    };
    frLexicon["flaneur"] = frLexicon["flâneur"];

    frLexicon["dépaysement"] = {
        "dépaysement", "/de.pɛ.i.zmɑ̃/", "nom",
        "Sentiment d'étrangeté ou de rupture éprouvé lorsqu'on change de milieu, de pays ou d'habitudes.",
        "Ce voyage au bout du monde lui a procuré un véritable dépaysement.",
        {"changement", "exotisme", "désorientation"},
        "fr", true
    };
    frLexicon["depaysement"] = frLexicon["dépaysement"];

    frLexicon["retrouvailles"] = {
        "retrouvailles", "/ʁə.tʁu.vaj/", "nom féminin pluriel",
        "Fait de retrouver des proches, des amis après une longue absence.",
        "Les retrouvailles de la famille étaient empreintes d'émotion et de joie.",
        {"réunion", "revoir", "rassemblement"},
        "fr", true
    };
    m_multiLangLexicon["fr"] = frLexicon;

    // Spanish Lexicon ("es")
    QMap<QString, DictEntry> esLexicon;
    esLexicon["sobremesa"] = {
        "sobremesa", "/so.βɾeˈme.sa/", "sustantivo",
        "Tiempo que se pasa sentado a la mesa conversando plácidamente después de comer.",
        "La sobremesa del domingo se alargó hasta bien entrada la tarde.",
        {"charla", "conversación", "tertulia"},
        "es", true
    };

    esLexicon["querencia"] = {
        "querencia", "/keˈɾen.sja/", "sustantivo",
        "Lugar predilecto donde alguien se siente seguro, cómodo y donde recobra sus fuerzas.",
        "La antigua biblioteca del pueblo era su querencia favorita.",
        {"refugio", "hogar", "santuario", "amparo"},
        "es", true
    };
    m_multiLangLexicon["es"] = esLexicon;

    // German Lexicon ("de")
    QMap<QString, DictEntry> deLexicon;
    deLexicon["fernweh"] = {
        "Fernweh", "/ˈfɛʁnveː/", "Substantiv, Neutrum",
        "Das sehnsüchtige Verlangen nach der Ferne, nach weiten Reisen und fremden Ländern.",
        "Beim Betrachten der alten Landkarten überkam ihn ein unstillbares Fernweh.",
        {"Wanderlust", "Reiselust", "Sehnsucht"},
        "de", true
    };
    deLexicon["waldeinsamkeit"] = {
        "Waldeinsamkeit", "/ˈvalt.aɪn.zaːm.kaɪt/", "Substantiv, Femininum",
        "Das erhebende Gefühl, im einsamen Wald ganz mit der Natur verbunden zu sein.",
        "Er genoss die friedliche Waldeinsamkeit des Schwarzwaldes fernab der Stadt.",
        {"Waldfrieden", "Stille", "Naturverbundenheit"},
        "de", true
    };
    m_multiLangLexicon["de"] = deLexicon;
}

DictEntry DictionaryService::lookup(const QString& word, const QString& language) {
    const QString clean = sanitizeWord(word);
    const QString lang = language.trimmed().isEmpty() ? QStringLiteral("en") : language.trimmed().toLower();
    if (clean.isEmpty()) {
        return {word, QString(), QString(), QStringLiteral("Please select or type a word to define."), QString(), {}, lang, false};
    }

    const QString cacheKey = QStringLiteral("%1:%2").arg(lang, clean);
    if (m_cache.contains(cacheKey)) {
        return m_cache.value(cacheKey);
    }

    // 1. Language-specific in-memory lexicon
    if (m_multiLangLexicon.contains(lang) && m_multiLangLexicon[lang].contains(clean)) {
        auto res = m_multiLangLexicon[lang][clean];
        m_cache.insert(cacheKey, res);
        return res;
    }

    // 2. Direct English in-memory lexicon
    if (m_lexicon.contains(clean)) {
        auto res = m_lexicon[clean];
        m_cache.insert(cacheKey, res);
        return res;
    }

    // 3. Query offline SQLite database (102,000+ words)
    if (m_dbOpen) {
        DictEntry dbRes = queryDatabase(clean, lang);
        if (dbRes.found) {
            m_cache.insert(cacheKey, dbRes);
            return dbRes;
        }

        // 4. Lemmatization: try inflected base forms (plurals, past tense, gerunds, etc.)
        const QStringList lemmas = generateLemmas(clean);
        for (const auto& stem : lemmas) {
            // Check in-memory lexicon for stem
            if (m_lexicon.contains(stem)) {
                auto res = m_lexicon[stem];
                res.word = clean;
                res.definition = QStringLiteral("[Base form: \"%1\"]\n\n%2").arg(stem, res.definition);
                m_cache.insert(cacheKey, res);
                return res;
            }
            // Check SQLite database for stem
            DictEntry stemRes = queryDatabase(stem, lang);
            if (stemRes.found) {
                stemRes.word = clean;
                stemRes.definition = QStringLiteral("[Base form: \"%1\"]\n\n%2").arg(stem, stemRes.definition);
                m_cache.insert(cacheKey, stemRes);
                return stemRes;
            }
        }

        // 5. Suggestions via prefix match
        if (clean.length() >= 3) {
            QSqlQuery sq(m_dictDb);
            sq.prepare(QStringLiteral("SELECT word FROM entries WHERE word LIKE ? AND word != ? LIMIT 4;"));
            sq.addBindValue(clean.left(std::min(4, static_cast<int>(clean.length()))) + QStringLiteral("%"));
            sq.addBindValue(clean);
            QStringList suggestions;
            if (sq.exec()) {
                while (sq.next()) {
                    suggestions.append(sq.value(0).toString());
                }
            }
            if (!suggestions.isEmpty()) {
                DictEntry sugEntry;
                sugEntry.word = clean;
                sugEntry.definition = QStringLiteral("Definition not found for \"%1\".\n\nDid you mean:\n• %2")
                    .arg(clean, suggestions.join(QStringLiteral("\n• ")));
                sugEntry.language = lang;
                sugEntry.found = false;
                m_cache.insert(cacheKey, sugEntry);
                return sugEntry;
            }
        }
    }

    // 6. Check other languages in multi-lang lexicon
    for (auto it = m_multiLangLexicon.cbegin(); it != m_multiLangLexicon.cend(); ++it) {
        if (it.key() != lang && it.value().contains(clean)) {
            auto res = it.value()[clean];
            m_cache.insert(cacheKey, res);
            return res;
        }
    }

    // 7. Not found in offline database
    DictEntry notFound;
    notFound.word = clean;
    notFound.definition = QStringLiteral("Definition not found in local offline vocabulary.");
    notFound.language = lang;
    notFound.found = false;
    m_cache.insert(cacheKey, notFound);
    return notFound;
}

QStringList DictionaryService::supportedLanguages() const {
    return {QStringLiteral("en"), QStringLiteral("fr"), QStringLiteral("es"), QStringLiteral("de")};
}

} // namespace LunaRead
