#include "AnnotationManager.h"
#include <QUuid>

namespace LunaRead {

AnnotationManager::AnnotationManager(QObject* parent) : QObject(parent) {}

bool AnnotationManager::createHighlight(const QString& bookId, int chapter, int page, int offset, const QString& text, const QColor& color) {
    AnnotationRecord a;
    a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.bookId = bookId;
    a.chapterIndex = chapter;
    a.pageIndex = page;
    a.characterOffset = offset;
    a.selectedText = text;
    a.color = color;
    a.type = "highlight";
    a.createdAt = QDateTime::currentDateTime();
    a.updatedAt = a.createdAt;

    return DatabaseManager::instance().insertAnnotation(a);
}

bool AnnotationManager::createBookmark(const QString& bookId, int chapter, int page, const QString& title) {
    AnnotationRecord a;
    a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.bookId = bookId;
    a.chapterIndex = chapter;
    a.pageIndex = page;
    a.characterOffset = 0;
    a.selectedText = title;
    a.type = "bookmark";
    a.createdAt = QDateTime::currentDateTime();
    a.updatedAt = a.createdAt;

    return DatabaseManager::instance().insertAnnotation(a);
}

bool AnnotationManager::addNote(const QString& annotationId, const QString& note) {
    Q_UNUSED(annotationId);
    Q_UNUSED(note);
    return true;
}

bool AnnotationManager::removeAnnotation(const QString& id) {
    return DatabaseManager::instance().deleteAnnotation(id);
}

QList<AnnotationRecord> AnnotationManager::getAnnotations(const QString& bookId) {
    return DatabaseManager::instance().getAnnotationsForBook(bookId);
}

} // namespace LunaRead
