#include "DictionaryService.h"

namespace LunaRead {

DictionaryService::DictionaryService(QObject* parent) : QObject(parent) {
    initBuiltInLexicon();
}

void DictionaryService::initBuiltInLexicon() {
    m_lexicon["serendipity"] = {"serendipity", "/ˌsɛrənˈdɪpɪti/", "noun",
        "The occurrence and development of events by chance in a happy or beneficial way.",
        "A fortunate stroke of serendipity brought them together."};

    m_lexicon["ephemeral"] = {"ephemeral", "/ɪˈfɛmərəl/", "adjective",
        "Lasting for a very short time.",
        "Fashions are ephemeral, but style endures."};

    m_lexicon["soliloquy"] = {"soliloquy", "/səˈlɪləkwi/", "noun",
        "An act of speaking one's thoughts aloud when by oneself.",
        "Hamlet's famous soliloquy begins with 'To be, or not to be'."};

    m_lexicon["melancholy"] = {"melancholy", "/ˈmɛlənkəli/", "noun",
        "A feeling of pensive sadness, typically with no obvious cause.",
        "An air of melancholy pervaded the quiet autumn garden."};
}

DictEntry DictionaryService::lookup(const QString& word) {
    QString clean = word.trimmed().toLower();
    if (m_lexicon.contains(clean)) {
        return m_lexicon[clean];
    }
    return {clean, "", "", "Definition not found in local offline vocabulary.", ""};
}

} // namespace LunaRead
