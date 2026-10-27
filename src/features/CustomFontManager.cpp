#include "CustomFontManager.h"
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDebug>

namespace LunaRead {

CustomFontManager& CustomFontManager::instance() {
    static CustomFontManager s_instance;
    return s_instance;
}

CustomFontManager::CustomFontManager(QObject* parent) : QObject(parent) {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_fontsDirPath = appData + QStringLiteral("/fonts");
    QDir().mkpath(m_fontsDirPath);
    scanFontsDirectory();
}

QString CustomFontManager::fontsDirectoryPath() const {
    return m_fontsDirPath;
}

void CustomFontManager::scanFontsDirectory() {
    m_customFamilies.clear();
    QDir dir(m_fontsDirPath);
    const auto entries = dir.entryInfoList({QStringLiteral("*.ttf"), QStringLiteral("*.otf"), QStringLiteral("*.woff"), QStringLiteral("*.woff2")}, QDir::Files);

    for (const auto& fi : entries) {
        int id = QFontDatabase::addApplicationFont(fi.absoluteFilePath());
        if (id >= 0) {
            const auto families = QFontDatabase::applicationFontFamilies(id);
            for (const auto& fam : families) {
                if (!m_customFamilies.contains(fam)) {
                    m_customFamilies.append(fam);
                }
            }
        }
    }
}

bool CustomFontManager::loadFontFile(const QString& fontFilePath) {
    if (!QFileInfo::exists(fontFilePath)) return false;

    // Copy to custom fonts directory
    QFileInfo fi(fontFilePath);
    QString targetPath = m_fontsDirPath + QLatin1Char('/') + fi.fileName();
    if (fontFilePath != targetPath) {
        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }
        QFile::copy(fontFilePath, targetPath);
    }

    int id = QFontDatabase::addApplicationFont(targetPath);
    if (id >= 0) {
        const auto families = QFontDatabase::applicationFontFamilies(id);
        for (const auto& fam : families) {
            if (!m_customFamilies.contains(fam)) {
                m_customFamilies.append(fam);
            }
        }
        emit fontsUpdated();
        return true;
    }
    return false;
}

QStringList CustomFontManager::customFontFamilies() const {
    return m_customFamilies;
}

QStringList CustomFontManager::allAvailableFamilies() const {
    QStringList all = m_customFamilies;
    const auto systemFamilies = QFontDatabase::families();
    for (const auto& f : systemFamilies) {
        if (!all.contains(f)) {
            all.append(f);
        }
    }
    return all;
}

} // namespace LunaRead
