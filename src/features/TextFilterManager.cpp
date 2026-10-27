#include "TextFilterManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QUuid>
#include <QDebug>
#include "../db/DatabaseManager.h"

namespace LunaRead {

TextFilterManager& TextFilterManager::instance() {
    static TextFilterManager s_instance;
    return s_instance;
}

TextFilterManager::TextFilterManager(QObject* parent) : QObject(parent) {
    ensureTable();
    loadDefaultsIfEmpty();
}

void TextFilterManager::ensureTable() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS text_filter_rules (
            id TEXT PRIMARY KEY,
            pattern TEXT NOT NULL,
            replacement TEXT NOT NULL,
            is_regex INTEGER NOT NULL DEFAULT 0,
            case_sensitive INTEGER NOT NULL DEFAULT 0,
            scope TEXT NOT NULL DEFAULT 'global',
            enabled INTEGER NOT NULL DEFAULT 1
        );
    )");
}

void TextFilterManager::loadDefaultsIfEmpty() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT COUNT(*) FROM text_filter_rules"));
    if (q.next() && q.value(0).toInt() == 0) {
        FilterRule r1;
        r1.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        r1.pattern = QStringLiteral(R"(\n{3,})");
        r1.replacement = QStringLiteral("\n\n");
        r1.isRegex = true;
        r1.caseSensitive = false;
        r1.scope = QStringLiteral("global");
        r1.enabled = true;
        addRule(r1);

        FilterRule r2;
        r2.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        r2.pattern = QStringLiteral(R"([ \t]+$)");
        r2.replacement = QStringLiteral("");
        r2.isRegex = true;
        r2.caseSensitive = false;
        r2.scope = QStringLiteral("global");
        r2.enabled = true;
        addRule(r2);
    }
}

QList<FilterRule> TextFilterManager::getRules(const QString& bookId) const {
    QList<FilterRule> list;
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);

    if (bookId.isEmpty()) {
        q.prepare(QStringLiteral("SELECT id, pattern, replacement, is_regex, case_sensitive, scope, enabled FROM text_filter_rules ORDER BY id ASC"));
    } else {
        q.prepare(QStringLiteral("SELECT id, pattern, replacement, is_regex, case_sensitive, scope, enabled FROM text_filter_rules WHERE scope = 'global' OR scope = ? ORDER BY id ASC"));
        q.addBindValue(bookId);
    }

    if (q.exec()) {
        while (q.next()) {
            FilterRule r;
            r.id = q.value(0).toString();
            r.pattern = q.value(1).toString();
            r.replacement = q.value(2).toString();
            r.isRegex = (q.value(3).toInt() != 0);
            r.caseSensitive = (q.value(4).toInt() != 0);
            r.scope = q.value(5).toString();
            r.enabled = (q.value(6).toInt() != 0);
            list.append(r);
        }
    }
    return list;
}

void TextFilterManager::addRule(const FilterRule& rule) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO text_filter_rules (id, pattern, replacement, is_regex, case_sensitive, scope, enabled)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(rule.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : rule.id);
    q.addBindValue(rule.pattern);
    q.addBindValue(rule.replacement);
    q.addBindValue(rule.isRegex ? 1 : 0);
    q.addBindValue(rule.caseSensitive ? 1 : 0);
    q.addBindValue(rule.scope);
    q.addBindValue(rule.enabled ? 1 : 0);
    if (q.exec()) {
        emit rulesChanged();
    }
}

void TextFilterManager::updateRule(const FilterRule& rule) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(R"(
        UPDATE text_filter_rules
        SET pattern = ?, replacement = ?, is_regex = ?, case_sensitive = ?, scope = ?, enabled = ?
        WHERE id = ?
    )");
    q.addBindValue(rule.pattern);
    q.addBindValue(rule.replacement);
    q.addBindValue(rule.isRegex ? 1 : 0);
    q.addBindValue(rule.caseSensitive ? 1 : 0);
    q.addBindValue(rule.scope);
    q.addBindValue(rule.enabled ? 1 : 0);
    q.addBindValue(rule.id);
    if (q.exec()) {
        emit rulesChanged();
    }
}

void TextFilterManager::deleteRule(const QString& ruleId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM text_filter_rules WHERE id = ?"));
    q.addBindValue(ruleId);
    if (q.exec()) {
        emit rulesChanged();
    }
}

void TextFilterManager::setRuleEnabled(const QString& ruleId, bool enabled) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE text_filter_rules SET enabled = ? WHERE id = ?"));
    q.addBindValue(enabled ? 1 : 0);
    q.addBindValue(ruleId);
    if (q.exec()) {
        emit rulesChanged();
    }
}

QString TextFilterManager::applyFilters(const QString& text, const QString& bookId) const {
    if (text.isEmpty()) return text;

    QString processed = text;
    const auto rules = getRules(bookId);

    for (const auto& r : rules) {
        if (!r.enabled || r.pattern.isEmpty()) continue;

        if (r.isRegex) {
            QRegularExpression::PatternOptions options = QRegularExpression::MultilineOption;
            if (!r.caseSensitive) {
                options |= QRegularExpression::CaseInsensitiveOption;
            }
            QRegularExpression re(r.pattern, options);
            if (re.isValid()) {
                processed.replace(re, r.replacement);
            }
        } else {
            const auto cs = r.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
            processed.replace(r.pattern, r.replacement, cs);
        }
    }

    return processed;
}

} // namespace LunaRead
