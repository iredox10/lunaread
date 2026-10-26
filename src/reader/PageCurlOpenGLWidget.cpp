#include "PageCurlOpenGLWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>
#include <numbers>
#include <vector>

namespace LunaRead {

struct PageVertex {
    float x;
    float y;
    float u;
    float v;
};

PageCurlOpenGLWidget::PageCurlOpenGLWidget(QWidget* parent)
    : QOpenGLWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_animation = new QPropertyAnimation(this, "curlProgress", this);
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_curlProgress >= 0.999) {
            emit pageTurnCompleted(true);
            m_curlProgress = 0.0;
            m_curlAngle = 0.0;
            update();
        }
    });
}

PageCurlOpenGLWidget::~PageCurlOpenGLWidget() {
    makeCurrent();
    cleanupGL();
    doneCurrent();
}

void PageCurlOpenGLWidget::setPages(const QImage& front, const QImage& back, const QImage& next) {
    m_frontImage = front.isNull() ? createFallbackImage(QStringLiteral("Page (Front)")) : front;
    m_backImage = back.isNull() ? createFallbackImage(QStringLiteral("Page (Back)")) : back;
    m_nextImage = next.isNull() ? createFallbackImage(QStringLiteral("Next Page")) : next;
    m_texturesDirty = true;
    update();
}

void PageCurlOpenGLWidget::setLeftPage(const QImage& left) {
    m_leftImage = left.isNull() ? createFallbackImage(QStringLiteral("Left Page")) : left;
    m_texturesDirty = true;
    update();
}

void PageCurlOpenGLWidget::setDualPages(
    const QImage& left,
    const QImage& rightFront,
    const QImage& rightBack,
    const QImage& nextRight
) {
    m_isDualPage = true;
    m_leftImage = left.isNull() ? createFallbackImage(QStringLiteral("Left Page")) : left;
    m_frontImage = rightFront.isNull() ? createFallbackImage(QStringLiteral("Right Page")) : rightFront;
    m_backImage = rightBack.isNull() ? createFallbackImage(QStringLiteral("Back Page")) : rightBack;
    m_nextImage = nextRight.isNull() ? createFallbackImage(QStringLiteral("Next Right Page")) : nextRight;
    m_texturesDirty = true;
    emit dualPageChanged(true);
    update();
}

void PageCurlOpenGLWidget::setDualPage(bool dual) {
    if (m_isDualPage != dual) {
        m_isDualPage = dual;
        emit dualPageChanged(dual);
        update();
    }
}

void PageCurlOpenGLWidget::setCurlProgress(qreal progress, const QPointF& dragPos) {
    qreal clamped = std::clamp(progress, 0.0, 1.0);
    if (!qFuzzyCompare(m_curlProgress, clamped)) {
        m_curlProgress = clamped;
        emit curlProgressChanged(m_curlProgress);
    }

    if (!dragPos.isNull()) {
        m_dragPos = dragPos;
        // Calculate curl angle from vertical drag deviation
        qreal halfH = height() > 0 ? static_cast<qreal>(height()) * 0.5 : 1.0;
        qreal deltaY = dragPos.y() - halfH;
        qreal targetAngle = std::clamp((deltaY / halfH) * 0.38, -0.48, 0.48);
        setCurlAngle(targetAngle);
    }

    update();
}

void PageCurlOpenGLWidget::setCurlAngle(qreal angleRadians) {
    qreal clamped = std::clamp(angleRadians, -0.55, 0.55);
    if (!qFuzzyCompare(m_curlAngle, clamped)) {
        m_curlAngle = clamped;
        emit curlAngleChanged(m_curlAngle);
        update();
    }
}

void PageCurlOpenGLWidget::setCylinderRadius(qreal radius) {
    m_customRadius = std::max(0.0, radius);
    update();
}

void PageCurlOpenGLWidget::setShadowStrength(qreal strength) {
    m_shadowStrength = std::clamp(strength, 0.0, 1.0);
    update();
}

void PageCurlOpenGLWidget::setSpecularStrength(qreal strength) {
    m_specularStrength = std::clamp(strength, 0.0, 1.0);
    update();
}

void PageCurlOpenGLWidget::setSpineShadowWidth(qreal widthPixels) {
    m_spineShadowWidth = std::max(0.0, widthPixels);
    update();
}

void PageCurlOpenGLWidget::setSpineShadowDepth(qreal depth) {
    m_spineShadowDepth = std::clamp(depth, 0.0, 1.0);
    update();
}

void PageCurlOpenGLWidget::startCurlAnimation(qreal targetProgress, int durationMs, QEasingCurve curve) {
    cancelAnimation();
    m_animation->setDuration(durationMs);
    m_animation->setStartValue(m_curlProgress);
    m_animation->setEndValue(std::clamp(targetProgress, 0.0, 1.0));
    m_animation->setEasingCurve(curve);
    m_animation->start();
}

void PageCurlOpenGLWidget::cancelAnimation() {
    if (m_animation->state() == QAbstractAnimation::Running) {
        m_animation->stop();
    }
}

bool PageCurlOpenGLWidget::isAnimating() const noexcept {
    return m_animation->state() == QAbstractAnimation::Running;
}

QImage PageCurlOpenGLWidget::createFallbackImage(const QString& label) const {
    const int w = 1000;
    const int h = 1400;
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(QColor(248, 244, 235)); // Warm parchment paper

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Subtle paper border
    p.setPen(QPen(QColor(210, 200, 185), 2.0));
    p.drawRect(24, 24, w - 48, h - 48);

    // Watermark/label
    p.setPen(QColor(120, 110, 95));
    QFont font("Sans Serif", 24, QFont::DemiBold);
    p.setFont(font);
    p.drawText(QRectF(40, 100, w - 80, 80), Qt::AlignCenter, label);

    // Placeholder lines
    p.setPen(QColor(180, 170, 155, 120));
    for (int y = 240; y < h - 160; y += 42) {
        p.drawLine(80, y, w - 80, y);
    }

    return img;
}

QImage PageCurlOpenGLWidget::renderPageToImage(
    const ParsedPage& page,
    const QSize& size,
    const ReaderThemeTokens& tokens,
    int pageIndex
) {
    QImage img(size, QImage::Format_RGBA8888);
    img.fill(tokens.backgroundColor);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal w = size.width();
    const qreal h = size.height();

    if (page.isImagePage && !page.imageBytes.isEmpty()) {
        QPixmap pix;
        pix.loadFromData(page.imageBytes);
        if (!pix.isNull()) {
            p.drawPixmap(0, 0, pix.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        return img;
    }

    // Body text
    p.setPen(tokens.textColor);
    QFont bodyFont("Roboto", 14);
    p.setFont(bodyFont);

    QRectF textRect(40, 48, w - 80, h - 96);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, page.text);

    // Footer
    p.setPen(tokens.headerFooterColor);
    QFont footerFont("Roboto", 10);
    p.setFont(footerFont);
    QRectF footerRect(40, h - 40, w - 80, 24);
    QString footerStr = QStringLiteral("Page %1").arg(pageIndex > 0 ? pageIndex : (page.pageIndexInChapter + 1));
    p.drawText(footerRect, Qt::AlignRight | Qt::AlignVCenter, footerStr);

    return img;
}

void PageCurlOpenGLWidget::initializeGL() {
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); // We render dual-sided via fragment shader with culling off for curl
    glDisable(GL_CULL_FACE); // Allow front & back of curl sheet

    initShaders();
    initMeshes();

    m_glInitialized = true;
    updateTextures();
}

void PageCurlOpenGLWidget::initShaders() {
    // 1. Curl Deformation Shader
    m_curlShader = std::make_unique<QOpenGLShaderProgram>();

    const char* curlVertexShader = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_texCoord;

uniform mat4 u_projection;
uniform vec4 u_pageRect; // x, y, width, height
uniform vec2 u_curlOrigin;
uniform vec2 u_curlNormal;
uniform float u_curlRadius;
uniform float u_flapFlattenFactor;

out vec2 v_texCoord;
out vec3 v_worldPos;
out vec3 v_normal;

const float PI = 3.14159265358979323846;

void main() {
    v_texCoord = a_texCoord;
    vec2 p = u_pageRect.xy + a_pos * u_pageRect.zw;

    float d = dot(p - u_curlOrigin, u_curlNormal);
    vec3 worldPos;
    vec3 normal;

    if (d <= 0.0) {
        // Flat page before curl axis
        worldPos = vec3(p, 0.0);
        normal = vec3(0.0, 0.0, 1.0);
    } else if (d < PI * u_curlRadius) {
        // Wrapping around the 3D cylinder
        float phi = d / u_curlRadius;
        vec2 alongAxis = p - u_curlNormal * d;
        vec2 rolled2D = alongAxis + u_curlNormal * (u_curlRadius * sin(phi));
        float z = u_curlRadius * (1.0 - cos(phi));
        worldPos = vec3(rolled2D, z);
        normal = vec3(-sin(phi) * u_curlNormal.x, -sin(phi) * u_curlNormal.y, cos(phi));
    } else {
        // Flat turned flap resting over opposite page
        float dFlat = d - PI * u_curlRadius;
        vec2 alongAxis = p - u_curlNormal * d;
        vec2 rolled2D = alongAxis - u_curlNormal * dFlat;
        float z = max(0.2, 2.0 * u_curlRadius - u_flapFlattenFactor * dFlat);
        worldPos = vec3(rolled2D, z);
        normal = vec3(0.0, 0.0, -1.0);
    }

    v_worldPos = worldPos;
    v_normal = normal;
    gl_Position = u_projection * vec4(worldPos, 1.0);
}
)";

    const char* curlFragmentShader = R"(#version 330 core
in vec2 v_texCoord;
in vec3 v_worldPos;
in vec3 v_normal;

uniform sampler2D u_frontTexture;
uniform sampler2D u_backTexture;
uniform vec3 u_lightDir;
uniform float u_specularStrength;
uniform float u_shininess;
uniform float u_spineX;
uniform float u_spineWidth;
uniform float u_spineDepth;
uniform int u_isDualPage;

out vec4 fragColor;

void main() {
    vec4 texColor;
    vec3 normal;

    if (v_normal.z >= 0.0) {
        // Front page face
        texColor = texture(u_frontTexture, v_texCoord);
        normal = normalize(v_normal);
    } else {
        // Back page face (mirrored texture coordinate)
        vec2 backUV = vec2(1.0 - v_texCoord.x, v_texCoord.y);
        texColor = texture(u_backTexture, backUV);
        // Subtle paper translucency bleed-through from front
        vec4 bleed = texture(u_frontTexture, v_texCoord);
        texColor = mix(texColor, bleed, 0.04);
        normal = normalize(-v_normal);
    }

    // Half-Lambert diffuse shading
    vec3 lightDir = normalize(u_lightDir);
    float NdotL = dot(normal, lightDir);
    float diff = max(NdotL * 0.4 + 0.6, 0.0);

    // Blinn-Phong specular highlight along cylinder ridge
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float specAngle = max(dot(normal, halfDir), 0.0);
    float spec = pow(specAngle, u_shininess) * u_specularStrength;

    // Spine gutter shadow
    float spineFactor = 1.0;
    if (u_spineWidth > 0.0) {
        float distSpine = abs(v_worldPos.x - u_spineX);
        if (distSpine < u_spineWidth) {
            float g = 1.0 - smoothstep(0.0, u_spineWidth, distSpine);
            spineFactor = 1.0 - g * u_spineDepth;
            if (u_isDualPage == 1 && distSpine < 1.2) {
                spineFactor *= 0.65;
            }
        }
    }

    vec3 finalRgb = (texColor.rgb * diff + vec3(spec)) * spineFactor;
    fragColor = vec4(finalRgb, texColor.a);
}
)";

    m_curlShader->addShaderFromSourceCode(QOpenGLShader::Vertex, curlVertexShader);
    m_curlShader->addShaderFromSourceCode(QOpenGLShader::Fragment, curlFragmentShader);
    m_curlShader->link();

    // 2. Base / Background Underneath Page Shader
    m_baseShader = std::make_unique<QOpenGLShaderProgram>();

    const char* baseVertexShader = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_texCoord;

uniform mat4 u_projection;
uniform vec4 u_rect;

out vec2 v_texCoord;
out vec2 v_worldPos;

void main() {
    v_texCoord = a_texCoord;
    v_worldPos = u_rect.xy + a_pos * u_rect.zw;
    gl_Position = u_projection * vec4(v_worldPos, 0.0, 1.0);
}
)";

    const char* baseFragmentShader = R"(#version 330 core
in vec2 v_texCoord;
in vec2 v_worldPos;

uniform sampler2D u_baseTexture;
uniform vec2 u_curlOrigin;
uniform vec2 u_curlNormal;
uniform float u_curlRadius;
uniform float u_shadowStrength;
uniform float u_spineX;
uniform float u_spineWidth;
uniform float u_spineDepth;
uniform int u_isDualPage;
uniform float u_progress;

out vec4 fragColor;

const float PI = 3.14159265358979323846;

void main() {
    vec4 texColor = texture(u_baseTexture, v_texCoord);

    // Dynamic drop shadow cast by overhead cylinder
    float shadowFactor = 0.0;
    if (u_progress > 0.005 && u_progress < 0.995 && u_curlRadius > 0.0) {
        float d = dot(v_worldPos - u_curlOrigin, u_curlNormal);
        float R = max(u_curlRadius, 10.0);
        if (d > -2.0 * R && d < 2.5 * PI * R) {
            float contact = smoothstep(-1.5 * R, 0.0, d) * smoothstep(1.2 * R, 0.0, d);
            float overhead = smoothstep(-0.5 * R, 0.5 * R, d) * smoothstep(2.5 * R, 0.5 * R, d);
            float flapDist = abs(d - PI * R);
            float flapShadow = smoothstep(2.0 * R, 0.0, flapDist) * 0.5;
            shadowFactor = max(contact * 0.8, max(overhead * 0.6, flapShadow)) * u_shadowStrength;
        }
    }

    // Spine gutter shadow
    float spineFactor = 1.0;
    if (u_spineWidth > 0.0) {
        float distSpine = abs(v_worldPos.x - u_spineX);
        if (distSpine < u_spineWidth) {
            float g = 1.0 - smoothstep(0.0, u_spineWidth, distSpine);
            spineFactor = 1.0 - g * u_spineDepth;
            if (u_isDualPage == 1 && distSpine < 1.2) {
                spineFactor *= 0.65;
            }
        }
    }

    vec3 shadedRgb = mix(texColor.rgb, texColor.rgb * 0.25, shadowFactor) * spineFactor;
    fragColor = vec4(shadedRgb, texColor.a);
}
)";

    m_baseShader->addShaderFromSourceCode(QOpenGLShader::Vertex, baseVertexShader);
    m_baseShader->addShaderFromSourceCode(QOpenGLShader::Fragment, baseFragmentShader);
    m_baseShader->link();
}

void PageCurlOpenGLWidget::initMeshes() {
    // 1. Fine-grained Deformable Grid Mesh (160 x 100 quads)
    constexpr int COLS = 160;
    constexpr int ROWS = 100;

    std::vector<PageVertex> vertices;
    vertices.reserve((COLS + 1) * (ROWS + 1));

    for (int r = 0; r <= ROWS; ++r) {
        float v = static_cast<float>(r) / static_cast<float>(ROWS);
        for (int c = 0; c <= COLS; ++c) {
            float u = static_cast<float>(c) / static_cast<float>(COLS);
            vertices.push_back({u, v, u, v});
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(COLS * ROWS * 6);

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            unsigned int topLeft = static_cast<unsigned int>(r * (COLS + 1) + c);
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = static_cast<unsigned int>((r + 1) * (COLS + 1) + c);
            unsigned int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    m_curlIndexCount = static_cast<int>(indices.size());

    m_curlVao.create();
    m_curlVao.bind();

    m_curlVbo.create();
    m_curlVbo.bind();
    m_curlVbo.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(PageVertex)));

    m_curlEbo.create();
    m_curlEbo.bind();
    m_curlEbo.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(unsigned int)));

    // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PageVertex), reinterpret_cast<void*>(offsetof(PageVertex, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PageVertex), reinterpret_cast<void*>(offsetof(PageVertex, u)));

    m_curlVao.release();

    // 2. Base Flat Quad Mesh (2 triangles)
    const PageVertex baseQuadVertices[] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}
    };

    m_baseVao.create();
    m_baseVao.bind();

    m_baseVbo.create();
    m_baseVbo.bind();
    m_baseVbo.allocate(baseQuadVertices, sizeof(baseQuadVertices));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PageVertex), reinterpret_cast<void*>(offsetof(PageVertex, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PageVertex), reinterpret_cast<void*>(offsetof(PageVertex, u)));

    m_baseVao.release();
}

void PageCurlOpenGLWidget::updateTextures() {
    if (!m_glInitialized) {
        return;
    }

    auto uploadTexture = [](const QImage& sourceImage, std::unique_ptr<QOpenGLTexture>& tex) {
        if (sourceImage.isNull()) {
            return;
        }
        tex = std::make_unique<QOpenGLTexture>(sourceImage.convertToFormat(QImage::Format_RGBA8888));
        tex->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
        tex->setWrapMode(QOpenGLTexture::ClampToEdge);
    };

    if (m_frontImage.isNull()) {
        m_frontImage = createFallbackImage(QStringLiteral("Page (Front)"));
    }
    if (m_backImage.isNull()) {
        m_backImage = createFallbackImage(QStringLiteral("Page (Back)"));
    }
    if (m_nextImage.isNull()) {
        m_nextImage = createFallbackImage(QStringLiteral("Next Page"));
    }
    if (m_leftImage.isNull()) {
        m_leftImage = createFallbackImage(QStringLiteral("Left Page"));
    }

    uploadTexture(m_frontImage, m_frontTexture);
    uploadTexture(m_backImage, m_backTexture);
    uploadTexture(m_nextImage, m_nextTexture);
    uploadTexture(m_leftImage, m_leftTexture);

    m_texturesDirty = false;
}

void PageCurlOpenGLWidget::cleanupGL() {
    m_frontTexture.reset();
    m_backTexture.reset();
    m_nextTexture.reset();
    m_leftTexture.reset();

    m_curlVbo.destroy();
    m_curlEbo.destroy();
    m_curlVao.destroy();

    m_baseVbo.destroy();
    m_baseVao.destroy();

    m_curlShader.reset();
    m_baseShader.reset();

    m_glInitialized = false;
}

void PageCurlOpenGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, static_cast<GLsizei>(w * devicePixelRatio()), static_cast<GLsizei>(h * devicePixelRatio()));
    m_projection.setToIdentity();
    m_projection.ortho(0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, -600.0f, 600.0f);
}

void PageCurlOpenGLWidget::paintGL() {
    glClearColor(0.14f, 0.14f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_texturesDirty) {
        updateTextures();
    }

    if (!m_frontTexture || !m_backTexture || !m_nextTexture) {
        return;
    }

    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());

    // Spine and page geometry
    const float spineX = m_isDualPage ? w * 0.5f : 0.0f;
    const QVector4D rightPageRect = m_isDualPage
        ? QVector4D(spineX, 0.0f, w - spineX, h)
        : QVector4D(0.0f, 0.0f, w, h);
    const QVector4D leftPageRect = QVector4D(0.0f, 0.0f, spineX, h);

    // Calculate curl origin and normal
    const float progress = static_cast<float>(m_curlProgress);
    const float angle = static_cast<float>(m_curlAngle);
    const QVector2D curlNormal(std::cos(angle), std::sin(angle));

    // Sweep origin from far right (progress=0) to far left (progress=1)
    const float xOrigin = m_isDualPage ? (w - progress * w) : (w * (1.0f - progress));
    const QVector2D curlOrigin(xOrigin, h * 0.5f);

    // Cylinder radius
    float radius;
    if (m_customRadius > 0.0) {
        radius = static_cast<float>(m_customRadius);
    } else {
        float baseR = std::max(22.0f, (m_isDualPage ? w * 0.5f : w) * 0.09f);
        float factor = std::sin(static_cast<float>(std::numbers::pi) * std::clamp(progress, 0.0f, 1.0f));
        radius = std::clamp(baseR * factor, 15.0f, baseR * 1.3f);
    }

    // --- STEP 1: Render Base / Underlying Pages ---
    m_baseShader->bind();
    m_baseShader->setUniformValue("u_projection", m_projection);
    m_baseShader->setUniformValue("u_curlOrigin", curlOrigin);
    m_baseShader->setUniformValue("u_curlNormal", curlNormal);
    m_baseShader->setUniformValue("u_curlRadius", radius);
    m_baseShader->setUniformValue("u_shadowStrength", static_cast<float>(m_shadowStrength));
    m_baseShader->setUniformValue("u_spineX", spineX);
    m_baseShader->setUniformValue("u_spineWidth", static_cast<float>(m_spineShadowWidth));
    m_baseShader->setUniformValue("u_spineDepth", static_cast<float>(m_spineShadowDepth));
    m_baseShader->setUniformValue("u_isDualPage", m_isDualPage ? 1 : 0);
    m_baseShader->setUniformValue("u_progress", progress);

    m_baseVao.bind();

    if (m_isDualPage) {
        // Left Page Base
        if (m_leftTexture) {
            m_leftTexture->bind(0);
            m_baseShader->setUniformValue("u_baseTexture", 0);
            m_baseShader->setUniformValue("u_rect", leftPageRect);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // Right Page Base (Next Page revealed underneath)
        m_nextTexture->bind(0);
        m_baseShader->setUniformValue("u_baseTexture", 0);
        m_baseShader->setUniformValue("u_rect", rightPageRect);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        // Single Page Base (Next Page)
        m_nextTexture->bind(0);
        m_baseShader->setUniformValue("u_baseTexture", 0);
        m_baseShader->setUniformValue("u_rect", rightPageRect);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    m_baseVao.release();
    m_baseShader->release();

    // --- STEP 2: Render 3D Curled Page Mesh ---
    if (progress < 0.999f) {
        m_curlShader->bind();
        m_curlShader->setUniformValue("u_projection", m_projection);
        m_curlShader->setUniformValue("u_pageRect", rightPageRect);
        m_curlShader->setUniformValue("u_curlOrigin", curlOrigin);
        m_curlShader->setUniformValue("u_curlNormal", curlNormal);
        m_curlShader->setUniformValue("u_curlRadius", radius);
        m_curlShader->setUniformValue("u_flapFlattenFactor", 0.038f);
        m_curlShader->setUniformValue("u_lightDir", QVector3D(-0.35f, 0.45f, 0.82f));
        m_curlShader->setUniformValue("u_specularStrength", static_cast<float>(m_specularStrength));
        m_curlShader->setUniformValue("u_shininess", static_cast<float>(m_shininess));
        m_curlShader->setUniformValue("u_spineX", spineX);
        m_curlShader->setUniformValue("u_spineWidth", static_cast<float>(m_spineShadowWidth));
        m_curlShader->setUniformValue("u_spineDepth", static_cast<float>(m_spineShadowDepth));
        m_curlShader->setUniformValue("u_isDualPage", m_isDualPage ? 1 : 0);

        m_frontTexture->bind(0);
        m_curlShader->setUniformValue("u_frontTexture", 0);

        m_backTexture->bind(1);
        m_curlShader->setUniformValue("u_backTexture", 1);

        m_curlVao.bind();
        glDrawElements(GL_TRIANGLES, m_curlIndexCount, GL_UNSIGNED_INT, nullptr);
        m_curlVao.release();

        m_curlShader->release();
    }
}

void PageCurlOpenGLWidget::mousePressEvent(QMouseEvent* event) {
    if (m_interactiveDrag && event->button() == Qt::LeftButton) {
        const qreal xThreshold = m_isDualPage ? width() * 0.6 : width() * 0.5;
        if (event->position().x() >= xThreshold) {
            m_isDragging = true;
            m_dragStartPos = event->position();
            m_dragPos = m_dragStartPos;
            cancelAnimation();
            emit dragStarted(m_dragStartPos);
        }
    }
    QOpenGLWidget::mousePressEvent(event);
}

void PageCurlOpenGLWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        m_dragPos = event->position();
        qreal span = m_isDualPage ? width() : width();
        qreal dx = m_dragStartPos.x() - m_dragPos.x();
        qreal progress = std::clamp(dx / span, 0.0, 1.0);
        setCurlProgress(progress, m_dragPos);
        emit dragMoved(m_dragPos, progress);
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void PageCurlOpenGLWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isDragging && event->button() == Qt::LeftButton) {
        m_isDragging = false;
        if (m_curlProgress >= 0.28) {
            startCurlAnimation(1.0, 280, QEasingCurve::OutQuad);
            emit dragEnded(true);
        } else {
            startCurlAnimation(0.0, 200, QEasingCurve::OutQuad);
            emit dragEnded(false);
        }
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

} // namespace LunaRead
