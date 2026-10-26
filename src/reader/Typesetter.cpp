#include "Typesetter.h"
#include <QTextLayout>
#include <QTextOption>
#include <QFontMetricsF>

namespace LunaRead {

Typesetter::Typesetter() {
    m_font = QFont("Roboto", 14);
}

void Typesetter::setFont(const QFont& font) {
    m_font = font;
}

void Typesetter::setLineHeight(qreal multiplier) {
    m_lineHeight = multiplier;
}

void Typesetter::setParagraphSpacing(qreal spacing) {
    m_paragraphSpacing = spacing;
}

void Typesetter::setMargins(const QMarginsF& margins) {
    m_margins = margins;
}

QVector<ParsedPage> Typesetter::paginate(const ChapterContent& chapter, const QSizeF& viewportSize, int startGlobalPageIndex) {
    QVector<ParsedPage> pages;

    if (chapter.isImagePage) {
        ParsedPage p;
        p.chapterIndex = chapter.index;
        p.pageIndexInChapter = 0;
        p.globalPageIndex = startGlobalPageIndex;
        p.isImagePage = true;
        p.imageBytes = chapter.imageBytes;
        pages.append(p);
        return pages;
    }

    qreal printableWidth = viewportSize.width() - (m_margins.left() + m_margins.right());
    qreal printableHeight = viewportSize.height() - (m_margins.top() + m_margins.bottom());

    if (printableWidth <= 50.0 || printableHeight <= 50.0) {
        printableWidth = 600.0;
        printableHeight = 800.0;
    }

    const QString& text = chapter.textContent;
    if (text.isEmpty()) {
        ParsedPage p;
        p.chapterIndex = chapter.index;
        p.pageIndexInChapter = 0;
        p.globalPageIndex = startGlobalPageIndex;
        p.text = "";
        pages.append(p);
        return pages;
    }

    QFontMetricsF fm(m_font);
    qreal lineSpacing = fm.lineSpacing() * m_lineHeight;

    // Split paragraphs
    QStringList paragraphs = text.split("\n", Qt::KeepEmptyParts);
    QString currentPageText;
    qreal currentHeight = 0.0;
    int pageIndex = 0;
    int charStart = 0;

    for (const QString& para : paragraphs) {
        if (para.trimmed().isEmpty()) {
            currentHeight += lineSpacing;
            currentPageText += "\n";
            continue;
        }

        QTextLayout layout(para, m_font);
        QTextOption opt(Qt::AlignJustify);
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(opt);
        layout.beginLayout();

        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(printableWidth);

            if (currentHeight + lineSpacing > printableHeight && !currentPageText.trimmed().isEmpty()) {
                // Page overflow: emit page
                ParsedPage p;
                p.chapterIndex = chapter.index;
                p.pageIndexInChapter = pageIndex;
                p.globalPageIndex = startGlobalPageIndex + pageIndex;
                p.text = currentPageText.trimmed();
                p.characterStartIndex = charStart;
                p.characterEndIndex = charStart + static_cast<int>(p.text.length());
                pages.append(p);

                charStart += static_cast<int>(p.text.length());
                pageIndex++;
                currentPageText.clear();
                currentHeight = 0.0;
            }

            QString lineStr = para.mid(line.textStart(), line.textLength());
            currentPageText += lineStr + "\n";
            currentHeight += lineSpacing;
        }
        layout.endLayout();
        currentHeight += m_paragraphSpacing;
        currentPageText += "\n";
    }

    if (!currentPageText.trimmed().isEmpty() || pages.isEmpty()) {
        ParsedPage p;
        p.chapterIndex = chapter.index;
        p.pageIndexInChapter = pageIndex;
        p.globalPageIndex = startGlobalPageIndex + pageIndex;
        p.text = currentPageText.trimmed();
        p.characterStartIndex = charStart;
        p.characterEndIndex = charStart + static_cast<int>(p.text.length());
        pages.append(p);
    }

    return pages;
}

} // namespace LunaRead
