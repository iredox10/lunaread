#pragma once

#include <QObject>
#include <QString>
#include <QMap>

namespace LunaRead {

struct DictEntry {
    QString word;
    QString phonetic;
    QString partOfSpeech;
    QString definition;
    QString example;
};

class DictionaryService : public QObject {
    Q_OBJECT
public:
    explicit DictionaryService(QObject* parent = nullptr);

    DictEntry lookup(const QString& word);

private:
    void initBuiltInLexicon();
    QMap<QString, DictEntry> m_lexicon;
};

} // namespace LunaRead
