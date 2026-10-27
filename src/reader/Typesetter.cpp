#include "Typesetter.h"
#include "../features/TextFilterManager.h"
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

void Typesetter::setParagraphIndent(qreal indentPx) {
    m_paragraphIndent = std::max(0.0, indentPx);
}

void Typesetter::setAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
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

    const QString filteredText = TextFilterManager::instance().applyFilters(chapter.textContent);
    const QString& text = filteredText;
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
        QTextOption opt(m_alignment);
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(opt);
        layout.beginLayout();

        bool isFirstLine = true;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            qreal effectiveWidth = printableWidth - (isFirstLine ? m_paragraphIndent : 0.0);
            line.setLineWidth(effectiveWidth);
            isFirstLine = false;

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
            if (isFirstLine && m_paragraphIndent > 0.0) {
                lineStr.prepend(QStringLiteral("    "));
            }
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
