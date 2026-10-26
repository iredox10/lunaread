#include "NaturalSort.h"
#include <algorithm>
#include <cctype>

namespace LunaRead {

bool NaturalSort::naturalCompare(const QString& a, const QString& b, bool caseSensitive) {
    int i = 0, j = 0;
    const int lenA = a.length();
    const int lenB = b.length();

    while (i < lenA && j < lenB) {
        QChar ca = a[i];
        QChar cb = b[j];

        if (ca.isDigit() && cb.isDigit()) {
            // Extract whole numbers
            int startA = i;
            while (i < lenA && a[i].isDigit()) i++;
            int startB = j;
            while (j < lenB && b[j].isDigit()) j++;

            QString numStrA = a.mid(startA, i - startA);
            QString numStrB = b.mid(startB, j - startB);

            // Compare by numeric value
            // Trim leading zeroes for length check
            int nzA = 0;
            while (nzA < numStrA.length() && numStrA[nzA] == '0') nzA++;
            int nzB = 0;
            while (nzB < numStrB.length() && numStrB[nzB] == '0') nzB++;

            QString valA = numStrA.mid(nzA);
            QString valB = numStrB.mid(nzB);

            if (valA.length() != valB.length()) {
                return valA.length() < valB.length();
            }
            if (valA != valB) {
                return valA < valB;
            }
            // Tie breaker: more leading zeroes come first
            if (numStrA.length() != numStrB.length()) {
                return numStrA.length() > numStrB.length();
            }
        } else {
            if (!caseSensitive) {
                ca = ca.toLower();
                cb = cb.toLower();
            }
            if (ca != cb) {
                return ca < cb;
            }
            i++;
            j++;
        }
    }

    return lenA < lenB;
}

void NaturalSort::sort(QVector<QString>& list, bool caseSensitive) {
    std::sort(list.begin(), list.end(), [caseSensitive](const QString& a, const QString& b) {
        return naturalCompare(a, b, caseSensitive);
    });
}

} // namespace LunaRead
