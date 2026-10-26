#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

namespace LunaRead {

struct DictEntry {
    QString word;
    QString phonetic;
    QString partOfSpeech;
    QString definition;
    QString example;
    QStringList synonyms = {};
    QString language = QStringLiteral("en");
    bool found = true;
};

class DictionaryService : public QObject {
    Q_OBJECT
public:
    explicit DictionaryService(QObject* parent = nullptr);

    DictEntry lookup(const QString& word, const QString& language = QStringLiteral("en"));
    [[nodiscard]] bool isOfflineLexicon() const noexcept { return true; }
    [[nodiscard]] QStringList supportedLanguages() const;

private:
    void initBuiltInLexicon();
    QMap<QString, DictEntry> m_lexicon;
    QMap<QString, QMap<QString, DictEntry>> m_multiLangLexicon;
};

} // namespace LunaRead

