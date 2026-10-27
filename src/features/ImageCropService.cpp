#include "ImageCropService.h"
#include <QColor>
#include <algorithm>

namespace LunaRead {

ImageCropService& ImageCropService::instance() {
    static ImageCropService s_instance;
    return s_instance;
}

ImageCropService::ImageCropService() {
    QSettings settings;
    m_enabled = settings.value("reader/autoCropMargins", false).toBool();
    m_whiteThreshold = settings.value("reader/cropWhiteThreshold", 238).toInt();
    m_marginPadding = settings.value("reader/cropMarginPadding", 8).toInt();
}

void ImageCropService::setAutoCropEnabled(bool enabled) {
    m_enabled = enabled;
    QSettings settings;
    settings.setValue("reader/autoCropMargins", enabled);
}

void ImageCropService::setWhiteThreshold(int threshold) {
    m_whiteThreshold = std::clamp(threshold, 150, 254);
    QSettings settings;
    settings.setValue("reader/cropWhiteThreshold", m_whiteThreshold);
}

void ImageCropService::setMarginPadding(int padding) {
    m_marginPadding = std::clamp(padding, 0, 40);
    QSettings settings;
    settings.setValue("reader/cropMarginPadding", m_marginPadding);
}

QImage ImageCropService::cropMargins(const QImage& src) const {
    if (!m_enabled || src.isNull() || src.width() < 50 || src.height() < 50) {
        return src;
    }

    QImage img = src.convertToFormat(QImage::Format_RGB32);
    const int w = img.width();
    const int h = img.height();

    int top = 0;
    int bottom = h - 1;
    int left = 0;
    int right = w - 1;

    auto isNonWhite = [this](QRgb rgb) {
        int r = qRed(rgb);
        int g = qGreen(rgb);
        int b = qBlue(rgb);
        int lum = (r * 299 + g * 587 + b * 114) / 1000;
        return lum < m_whiteThreshold;
    };

    // 1. Scan Top edge
    bool found = false;
    for (int y = 0; y < h / 3; ++y) {
        const QRgb* scanLine = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < w; x += 2) {
            if (isNonWhite(scanLine[x])) {
                top = y;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    // 2. Scan Bottom edge
    found = false;
    for (int y = h - 1; y > (h * 2) / 3; --y) {
        const QRgb* scanLine = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < w; x += 2) {
            if (isNonWhite(scanLine[x])) {
                bottom = y;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    // 3. Scan Left edge
    found = false;
    for (int x = 0; x < w / 3; ++x) {
        for (int y = top; y <= bottom; y += 2) {
            if (isNonWhite(img.pixel(x, y))) {
                left = x;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    // 4. Scan Right edge
    found = false;
    for (int x = w - 1; x > (w * 2) / 3; --x) {
        for (int y = top; y <= bottom; y += 2) {
            if (isNonWhite(img.pixel(x, y))) {
                right = x;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    // Safety padding
    top = std::max(0, top - m_marginPadding);
    bottom = std::min(h - 1, bottom + m_marginPadding);
    left = std::max(0, left - m_marginPadding);
    right = std::min(w - 1, right + m_marginPadding);

    QRect cropRect(left, top, right - left + 1, bottom - top + 1);

    if (cropRect.width() > 40 && cropRect.height() > 40 &&
        (cropRect.width() < static_cast<int>(w * 0.98) || cropRect.height() < static_cast<int>(h * 0.98))) {
        return img.copy(cropRect);
    }

    return src;
}

QPixmap ImageCropService::cropMargins(const QPixmap& src) const {
    if (!m_enabled || src.isNull()) {
        return src;
    }
    QImage cropped = cropMargins(src.toImage());
    return QPixmap::fromImage(cropped);
}

} // namespace LunaRead
