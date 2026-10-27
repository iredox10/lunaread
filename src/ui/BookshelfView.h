#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include "../core/Types.h"
#include "BookCardWidget.h"

namespace LunaRead {

class BookshelfView : public QWidget {
    Q_OBJECT
public:
    explicit BookshelfView(QWidget* parent = nullptr);

    void refreshBooks();

signals:
    void bookSelected(const QString& bookId);

private:
    void setupContinueBanner();
    void updateContinueBanner(const QList<BookItem>& allBooks);
    void populateGrid(const QList<BookItem>& books);

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_formatFilter = nullptr;

    // Continue Reading Hero Banner
    QFrame* m_continueBanner = nullptr;
    QLabel* m_bannerTitleLabel = nullptr;
    QLabel* m_bannerAuthorLabel = nullptr;
    QLabel* m_bannerTimeLabel = nullptr;
    QProgressBar* m_bannerProgress = nullptr;
    QPushButton* m_resumeBtn = nullptr;
    QString m_resumeBookId;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_gridContainer = nullptr;
    QGridLayout* m_gridLayout = nullptr;
};

} // namespace LunaRead
