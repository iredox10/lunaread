#pragma once

#include <QWidget>
#include <optional>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"
#include "Typesetter.h"

namespace LunaRead {

class DualPageRenderer : public QWidget {
    Q_OBJECT
public:
    explicit DualPageRenderer(QWidget* parent = nullptr);

    void setTypesetter(Typesetter* typesetter);
    void setBookInfo(const QString& bookTitle, const QString& chapterTitle);
    void setBookId(const QString& bookId);
    void setFilePath(const QString& filePath);
    void setAnnotations(const QVector<AnnotationRecord>& annotations);
    void setActiveSentence(const QString& sentence);
    void setPages(const ParsedPage& left, const std::optional<ParsedPage>& right, const ReaderThemeTokens& tokens);
    void updateTheme(const ReaderThemeTokens& tokens);
    void setRightToLeft(bool rtl);
    [[nodiscard]] bool isRightToLeft() const noexcept { return m_rightToLeft; }

signals:
    void pageTurnRequested(int delta);
    void menuToggleRequested();
    void selectionMade(const QString& selectedText, const QRectF& normalizedRect, int chapterIndex, int pageIndex, const QPoint& globalPos);
    void annotationTapped(const QString& annotationId, const QPoint& globalPos);
    void linkClicked(const QString& href, const QPointF& pos);
    void contextMenuRequested(const QPoint& globalPos, int chapterIndex, int pageIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    [[nodiscard]] QRectF imageTargetRectFor(const ParsedPage& page, const QRectF& slotRect) const;
    [[nodiscard]] const AnnotationRecord* annotationAt(const QPointF& pos, const ParsedPage& page, const QRectF& targetRect) const;

    Typesetter* m_typesetter = nullptr;
    QString m_bookTitle;
    QString m_chapterTitle;
    QString m_bookId;
    QString m_filePath;
    QVector<AnnotationRecord> m_annotations;
    ParsedPage m_leftPage;
    std::optional<ParsedPage> m_rightPage;
    ReaderThemeTokens m_tokens;
    QString m_activeSentence;
    bool m_rightToLeft = false;

    // Live Moon+ Reader selection feedback
    bool m_isSelecting = false;
    BookTextSelection m_currentSelection;
    enum class HandleDrag { None, StartHandle, EndHandle };
    HandleDrag m_activeHandleDrag = HandleDrag::None;
    QPointF m_fixedSelectionAnchor;
    QPointF m_pressPos;
    QPointF m_dragEnd;
    QRectF m_selectionRect;
    int m_selectedSlot = 0; // 0 for left slot, 1 for right slot
};

} // namespace LunaRead
