#pragma once

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGL/QOpenGLTexture>
#include <QMatrix4x4>
#include <QPointF>
#include <QImage>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <memory>
#include "../core/Types.h"
#include "../core/ThemeTokens.h"

namespace LunaRead {

/**
 * @brief High-performance 3D OpenGL page curl simulation widget for desktop book reading.
 *
 * Implements a dual-page cylinder mesh curl deformation driven by drag progress (0.0 to 1.0)
 * and curl angle, featuring:
 * - Real-time 3D cylinder roll vertex deformation with continuous surface normals.
 * - Hardware texturing with mipmapping for front page, back page, next page, and dual-page left side.
 * - Dynamic shading:
 *     1. Spine gutter shadow with crease line at book binding.
 *     2. Ambient specular highlight along the cylinder ridge (Blinn-Phong).
 *     3. Dynamic soft drop shadow cast onto the page beneath.
 * - Interactive mouse peeling and fluid spring-damper/cubic curl animations.
 */
class PageCurlOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
    Q_PROPERTY(qreal curlProgress READ curlProgress WRITE setCurlProgress NOTIFY curlProgressChanged)
    Q_PROPERTY(qreal curlAngle READ curlAngle WRITE setCurlAngle NOTIFY curlAngleChanged)
    Q_PROPERTY(bool dualPage READ isDualPage WRITE setDualPage NOTIFY dualPageChanged)

public:
    explicit PageCurlOpenGLWidget(QWidget* parent = nullptr);
    ~PageCurlOpenGLWidget() override;

    // --- Core API ---

    /**
     * @brief Set page images for 3D curling.
     * @param front The current page surface being curled (front face).
     * @param back The reverse side of the turned sheet (back face).
     * @param next The page revealed underneath on the right.
     */
    void setPages(const QImage& front, const QImage& back, const QImage& next);

    /**
     * @brief Update curl deformation progress and mouse drag position.
     * @param progress Turn progress from 0.0 (completely flat) to 1.0 (turned).
     * @param dragPos Current mouse drag position in widget coordinates (used for diagonal curl angle).
     */
    void setCurlProgress(qreal progress, const QPointF& dragPos = QPointF());

    // --- Dual-Page Mode ---

    /**
     * @brief Enable or disable dual-page book rendering mode.
     */
    void setDualPage(bool dual);

    [[nodiscard]] bool isDualPage() const noexcept { return m_isDualPage; }

    /**
     * @brief Set the left page image for dual-page mode.
     */
    void setLeftPage(const QImage& left);

    /**
     * @brief Convenience method to set all four pages for dual-page book rendering.
     */
    void setDualPages(const QImage& left, const QImage& rightFront, const QImage& rightBack, const QImage& nextRight);

    // --- Curl & Deformation Properties ---

    [[nodiscard]] qreal curlProgress() const noexcept { return m_curlProgress; }
    [[nodiscard]] qreal curlAngle() const noexcept { return m_curlAngle; }
    void setCurlAngle(qreal angleRadians);

    [[nodiscard]] qreal cylinderRadius() const noexcept { return m_customRadius; }
    void setCylinderRadius(qreal radius);

    // --- Dynamic Shading Parameters ---

    [[nodiscard]] qreal shadowStrength() const noexcept { return m_shadowStrength; }
    void setShadowStrength(qreal strength);

    [[nodiscard]] qreal specularStrength() const noexcept { return m_specularStrength; }
    void setSpecularStrength(qreal strength);

    [[nodiscard]] qreal spineShadowWidth() const noexcept { return m_spineShadowWidth; }
    void setSpineShadowWidth(qreal widthPixels);

    [[nodiscard]] qreal spineShadowDepth() const noexcept { return m_spineShadowDepth; }
    void setSpineShadowDepth(qreal depth);

    // --- Animation & Interactivity ---

    void startCurlAnimation(qreal targetProgress, int durationMs = 320, QEasingCurve curve = QEasingCurve::OutCubic);
    void cancelAnimation();
    [[nodiscard]] bool isAnimating() const noexcept;

    [[nodiscard]] bool isInteractiveDragEnabled() const noexcept { return m_interactiveDrag; }
    void setInteractiveDragEnabled(bool enabled) { m_interactiveDrag = enabled; }

    // --- Utility: Render ParsedPage to QImage ---
    static QImage renderPageToImage(
        const ParsedPage& page,
        const QSize& size,
        const ReaderThemeTokens& tokens,
        int pageIndex = 0
    );

signals:
    void curlProgressChanged(qreal progress);
    void curlAngleChanged(qreal angle);
    void dualPageChanged(bool dual);
    void pageTurnCompleted(bool forward);
    void dragStarted(const QPointF& pos);
    void dragMoved(const QPointF& pos, qreal progress);
    void dragEnded(bool turnCommitted);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void initShaders();
    void initMeshes();
    void updateTextures();
    void cleanupGL();
    [[nodiscard]] QImage createFallbackImage(const QString& label) const;

    // Shader programs
    std::unique_ptr<QOpenGLShaderProgram> m_curlShader;
    std::unique_ptr<QOpenGLShaderProgram> m_baseShader;

    // OpenGL Buffers & VAOs
    QOpenGLVertexArrayObject m_curlVao;
    QOpenGLBuffer m_curlVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_curlEbo{QOpenGLBuffer::IndexBuffer};
    int m_curlIndexCount = 0;

    QOpenGLVertexArrayObject m_baseVao;
    QOpenGLBuffer m_baseVbo{QOpenGLBuffer::VertexBuffer};

    // Textures
    std::unique_ptr<QOpenGLTexture> m_frontTexture;
    std::unique_ptr<QOpenGLTexture> m_backTexture;
    std::unique_ptr<QOpenGLTexture> m_nextTexture;
    std::unique_ptr<QOpenGLTexture> m_leftTexture;

    // Backing images
    QImage m_frontImage;
    QImage m_backImage;
    QImage m_nextImage;
    QImage m_leftImage;
    bool m_texturesDirty = false;
    bool m_glInitialized = false;

    // Deformation & Rendering State
    qreal m_curlProgress = 0.0;
    qreal m_curlAngle = 0.0;
    QPointF m_dragPos;
    QPointF m_dragStartPos;
    bool m_isDualPage = false;
    bool m_isDragging = false;
    bool m_interactiveDrag = true;

    // Shading & Geometry parameters
    qreal m_customRadius = 0.0; // 0.0 = auto
    qreal m_shadowStrength = 0.65;
    qreal m_specularStrength = 0.40;
    qreal m_shininess = 32.0;
    qreal m_spineShadowWidth = 35.0;
    qreal m_spineShadowDepth = 0.50;

    // Animation
    QPropertyAnimation* m_animation = nullptr;

    // Model-View-Projection matrix
    QMatrix4x4 m_projection;
};

} // namespace LunaRead
