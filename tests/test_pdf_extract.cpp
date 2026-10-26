#include <QProcess>
#include <QFileInfo>
#include <QDebug>
#include <iostream>

int main() {
    QString path = "/home/iredox/design_books/Designing Interfaces Patterns for Effective Interaction Design (Tidwell, Jenifer, Brewer, Charles, Valencia etc.) (z-library.sk, 1lib.sk, z-lib.sk).pdf";
    QProcess proc;
    QStringList args;
    args << "-layout" << path << "-";
    proc.start("pdftotext", args);
    if (!proc.waitForFinished(10000)) {
        std::cout << "Timed out waiting for pdftotext!" << std::endl;
        return 1;
    }
    
    QByteArray out = proc.readAllStandardOutput();
    QList<QByteArray> pages = out.split('\x0c');
    std::cout << "Successfully extracted " << pages.size() << " pages! Output size: " << out.size() << " bytes." << std::endl;
    for (int i = 0; i < std::min(5, (int)pages.size()); ++i) {
        std::cout << "Page " << i + 1 << " non-empty: " << !pages[i].trimmed().isEmpty() 
                  << " length: " << pages[i].size() << std::endl;
        if (!pages[i].trimmed().isEmpty()) {
            std::cout << "Sample: " << pages[i].trimmed().left(80).toStdString() << std::endl;
        }
    }
    return 0;
}
