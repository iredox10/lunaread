#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFontDatabase>

namespace LunaRead {

/**
 * @brief Manages custom user fonts (.ttf, .otf, .woff) loaded at runtime.
 *
 * Scans ~/.local/share/lunaread/fonts/ and allows importing custom font files
 * into Qt's active font engine for reading typography customization.
 */
class CustomFontManager : public QObject {
    Q_OBJECT

public:
    static CustomFontManager& instance();

    void scanFontsDirectory();
    bool loadFontFile(const QString& fontFilePath);
    [[nodiscard]] QStringList customFontFamilies() const;
    [[nodiscard]] QStringList allAvailableFamilies() const;
    [[nodiscard]] QString fontsDirectoryPath() const;

signals:
    void fontsUpdated();

private:
    explicit CustomFontManager(QObject* parent = nullptr);
    ~CustomFontManager() override = default;

    QString m_fontsDirPath;
    QStringList m_customFamilies;
};

} // namespace LunaRead
