#pragma once

#include <QString>
#include <QVector>

namespace LunaRead {

class NaturalSort {
public:
    /// Compares two strings in natural alphanumeric order.
    static bool naturalCompare(const QString& a, const QString& b, bool caseSensitive = false);

    /// Sorts a vector of QStrings in-place in natural order.
    static void sort(QVector<QString>& list, bool caseSensitive = false);
};

} // namespace LunaRead
