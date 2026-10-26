#include "DictionaryService.h"

namespace LunaRead {

DictionaryService::DictionaryService(QObject* parent) : QObject(parent) {
    initBuiltInLexicon();
}

void DictionaryService::initBuiltInLexicon() {
    // English Lexicon
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
    const QString clean = word.trimmed().toLower();
    const QString lang = language.trimmed().toLower();

    // Check language-specific lexicon first if specified
    if (!lang.isEmpty() && m_multiLangLexicon.contains(lang)) {
        const auto& langMap = m_multiLangLexicon[lang];
        if (langMap.contains(clean)) {
            return langMap[clean];
        }
    }

    // Check main English lexicon
    if (m_lexicon.contains(clean)) {
        return m_lexicon[clean];
    }

    // Check any other registered lexicon as fallback
    for (auto it = m_multiLangLexicon.cbegin(); it != m_multiLangLexicon.cend(); ++it) {
        if (it.key() != lang && it.value().contains(clean)) {
            return it.value()[clean];
        }
    }

    return {
        word.trimmed(),
        QString(),
        QString(),
        QStringLiteral("Definition not found in local offline vocabulary."),
        QString(),
        {},
        lang.isEmpty() ? QStringLiteral("en") : lang,
        false
    };
}

QStringList DictionaryService::supportedLanguages() const {
    return {QStringLiteral("en"), QStringLiteral("fr"), QStringLiteral("es"), QStringLiteral("de")};
}

} // namespace LunaRead

