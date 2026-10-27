#pragma once

#include <QString>
#include <QList>
#include <QObject>
#include <QSqlDatabase>

namespace LunaRead {

struct FilterRule {
    QString id;
    QString pattern;
    QString replacement;
    bool isRegex = false;
    bool caseSensitive = false;
    QString scope = QStringLiteral("global"); // "global" or specific bookId
    bool enabled = true;
};

/**
 * @brief Manages custom text replacement and content filtering rules (regex or plain text).
 *
 * Stored in SQLite and applied dynamically during typesetter pagination.
 * Allows users to filter out web novel translator ads, fix repetitive formatting errors,
 * clean excessive blank lines, or perform custom substitutions.
 */
class TextFilterManager : public QObject {
    Q_OBJECT

public:
    static TextFilterManager& instance();

    QList<FilterRule> getRules(const QString& bookId = QString()) const;
    void addRule(const FilterRule& rule);
    void updateRule(const FilterRule& rule);
    void deleteRule(const QString& ruleId);
    void setRuleEnabled(const QString& ruleId, bool enabled);

    QString applyFilters(const QString& text, const QString& bookId = QString()) const;

signals:
    void rulesChanged();

private:
    explicit TextFilterManager(QObject* parent = nullptr);
    ~TextFilterManager() override = default;

    void ensureTable();
    void loadDefaultsIfEmpty();

    TextFilterManager(const TextFilterManager&) = delete;
    TextFilterManager& operator=(const TextFilterManager&) = delete;
};

} // namespace LunaRead
