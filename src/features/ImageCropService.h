#pragma once

#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QSettings>

namespace LunaRead {

class ImageCropService {
public:
    static ImageCropService& instance();

    [[nodiscard]] bool isAutoCropEnabled() const noexcept { return m_enabled; }
    void setAutoCropEnabled(bool enabled);

    [[nodiscard]] int whiteThreshold() const noexcept { return m_whiteThreshold; }
    void setWhiteThreshold(int threshold);

    [[nodiscard]] int marginPadding() const noexcept { return m_marginPadding; }
    void setMarginPadding(int padding);

    /// Analyzes an image and crops out surrounding empty borders
    QImage cropMargins(const QImage& sourceImage) const;
    QPixmap cropMargins(const QPixmap& sourcePixmap) const;

private:
    ImageCropService();
    ~ImageCropService() = default;

    bool m_enabled = false;
    int m_whiteThreshold = 238; // Luminance 0-255 where > threshold is considered white margin
    int m_marginPadding = 8;    // Safety padding inside border
};

} // namespace LunaRead
