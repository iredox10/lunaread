#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSqlDatabase>

namespace LunaRead {

struct DictEntry {
    QString word;
    QString phonetic;
    QString partOfSpeech;
    QString definition;
    QString example;
    QStringList synonyms = {};
    QString language = QStringLiteral("en");
    bool found = false;
};

class DictionaryService : public QObject {
    Q_OBJECT
public:
    explicit DictionaryService(QObject* parent = nullptr);
    ~DictionaryService() override;

    DictEntry lookup(const QString& word, const QString& language = QStringLiteral("en"));
    [[nodiscard]] bool isOfflineLexicon() const noexcept { return true; }
    [[nodiscard]] QStringList supportedLanguages() const;
    [[nodiscard]] bool isDatabaseLoaded() const noexcept { return m_dbOpen; }
    [[nodiscard]] int totalWordsCount() const;

private:
    void initBuiltInLexicon();
    void openDatabase();
    [[nodiscard]] QString sanitizeWord(const QString& rawWord) const;
    [[nodiscard]] QStringList generateLemmas(const QString& word) const;
    DictEntry queryDatabase(const QString& cleanWord, const QString& lang);

    QSqlDatabase m_dictDb;
    bool m_dbOpen = false;
    QString m_connectionName;

    QMap<QString, DictEntry> m_lexicon;
    QMap<QString, QMap<QString, DictEntry>> m_multiLangLexicon;
    QMap<QString, DictEntry> m_cache;
};

} // namespace LunaRead
