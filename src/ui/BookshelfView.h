#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
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
    void populateGrid(const QList<BookItem>& books);

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_formatFilter = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_gridContainer = nullptr;
    QGridLayout* m_gridLayout = nullptr;
};

} // namespace LunaRead
