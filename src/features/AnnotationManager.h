#pragma once

#include <QObject>
#include <QVector>
#include "../core/Types.h"
#include "../db/DatabaseManager.h"

namespace LunaRead {

class AnnotationManager : public QObject {
    Q_OBJECT
public:
    explicit AnnotationManager(QObject* parent = nullptr);

    bool createHighlight(const QString& bookId, int chapter, int page, int offset, const QString& text, const QColor& color = QColor(255, 245, 157));
    bool createBookmark(const QString& bookId, int chapter, int page, const QString& title);
    bool addNote(const QString& annotationId, const QString& note);
    bool removeAnnotation(const QString& id);
    QList<AnnotationRecord> getAnnotations(const QString& bookId);
};

} // namespace LunaRead
