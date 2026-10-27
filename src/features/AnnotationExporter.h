#pragma once

#include <QString>
#include <QList>
#include "../core/Types.h"

namespace LunaRead {

class AnnotationExporter {
public:
    static QString exportToMarkdown(const QString& bookTitle, const QList<AnnotationRecord>& annotations);
    static QString exportToHtml(const QString& bookTitle, const QList<AnnotationRecord>& annotations);
    static QString exportToPlainText(const QString& bookTitle, const QList<AnnotationRecord>& annotations);
    static QString exportToJson(const QString& bookTitle, const QList<AnnotationRecord>& annotations);
};

} // namespace LunaRead
