#include "coordinate.h"

#include <QtMath>

CoordinateGeometry::CoordinateGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent) {
    updateGeometry();
}

CoordinateGeometry::Shape CoordinateGeometry::shape() const {
    return m_shape;
}

void CoordinateGeometry::setShape(Shape shape) {
    if (m_shape == shape) {
        return;
    }
    m_shape = shape;
    updateGeometry();
    emit shapeChanged();
}

float CoordinateGeometry::size() const {
    return m_size;
}

void CoordinateGeometry::setSize(float size) {
    float clamped = qMax(0.001f, size);
    if (qFuzzyCompare(m_size, clamped)) {
        return;
    }
    m_size = clamped;
    updateGeometry();
    emit sizeChanged();
}

float CoordinateGeometry::radius() const {
    return m_radius;
}

void CoordinateGeometry::setRadius(float radius) {
    float clamped = qMax(0.001f, radius);
    if (qFuzzyCompare(m_radius, clamped)) {
        return;
    }
    m_radius = clamped;
    updateGeometry();
    emit radiusChanged();
}

int CoordinateGeometry::rings() const {
    return m_rings;
}

void CoordinateGeometry::setRings(int rings) {
    int clamped = qMax(3, rings);
    if (m_rings == clamped) {
        return;
    }
    m_rings = clamped;
    updateGeometry();
    emit ringsChanged();
}

int CoordinateGeometry::slices() const {
    return m_slices;
}

void CoordinateGeometry::setSlices(int slices) {
    int clamped = qMax(3, slices);
    if (m_slices == clamped) {
        return;
    }
    m_slices = clamped;
    updateGeometry();
    emit slicesChanged();
}

void CoordinateGeometry::updateGeometry() {
    QVector<Vertex> vertices;
    if (m_shape == Cube) {
        buildCube(vertices, m_size);
    } else {
        buildSphere(vertices, m_radius, m_rings, m_slices);
    }

    clear();
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    setStride(sizeof(Vertex));
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, sizeof(QVector3D),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic,
                 sizeof(QVector3D) * 2, QQuick3DGeometry::Attribute::F32Type);

    QByteArray vertexData(reinterpret_cast<const char*>(vertices.constData()),
                          vertices.size() * sizeof(Vertex));
    setVertexData(vertexData);

    if (!vertices.isEmpty()) {
        QVector3D minBound = vertices.first().position;
        QVector3D maxBound = vertices.first().position;
        for (const Vertex& vertex : vertices) {
            minBound.setX(qMin(minBound.x(), vertex.position.x()));
            minBound.setY(qMin(minBound.y(), vertex.position.y()));
            minBound.setZ(qMin(minBound.z(), vertex.position.z()));
            maxBound.setX(qMax(maxBound.x(), vertex.position.x()));
            maxBound.setY(qMax(maxBound.y(), vertex.position.y()));
            maxBound.setZ(qMax(maxBound.z(), vertex.position.z()));
        }
        setBounds(minBound, maxBound);
    }
}

static CoordinateGeometry::Vertex makeVertex(const QVector3D& position,
                                         const QVector3D& normal,
                                         const QVector2D& uv) {
    CoordinateGeometry::Vertex vertex;
    vertex.position = position;
    vertex.normal = normal;
    vertex.uv = uv;
    return vertex;
}

void CoordinateGeometry::buildCube(QVector<Vertex>& vertices, float size) {
    const float h = size * 0.5f;

    auto addQuad = [&](const QVector3D& a, const QVector3D& b,
                       const QVector3D& c, const QVector3D& d,
                       const QVector3D& normal) {
        vertices.append(makeVertex(a, normal, QVector2D(0.0f, 0.0f)));
        vertices.append(makeVertex(b, normal, QVector2D(1.0f, 0.0f)));
        vertices.append(makeVertex(c, normal, QVector2D(1.0f, 1.0f)));
        vertices.append(makeVertex(a, normal, QVector2D(0.0f, 0.0f)));
        vertices.append(makeVertex(c, normal, QVector2D(1.0f, 1.0f)));
        vertices.append(makeVertex(d, normal, QVector2D(0.0f, 1.0f)));
    };

    addQuad(QVector3D(-h, -h, h), QVector3D(h, -h, h), QVector3D(h, h, h), QVector3D(-h, h, h), QVector3D(0, 0, 1));
    addQuad(QVector3D(h, -h, -h), QVector3D(-h, -h, -h), QVector3D(-h, h, -h), QVector3D(h, h, -h), QVector3D(0, 0, -1));
    addQuad(QVector3D(-h, h, h), QVector3D(h, h, h), QVector3D(h, h, -h), QVector3D(-h, h, -h), QVector3D(0, 1, 0));
    addQuad(QVector3D(-h, -h, -h), QVector3D(h, -h, -h), QVector3D(h, -h, h), QVector3D(-h, -h, h), QVector3D(0, -1, 0));
    addQuad(QVector3D(h, -h, h), QVector3D(h, -h, -h), QVector3D(h, h, -h), QVector3D(h, h, h), QVector3D(1, 0, 0));
    addQuad(QVector3D(-h, -h, -h), QVector3D(-h, -h, h), QVector3D(-h, h, h), QVector3D(-h, h, -h), QVector3D(-1, 0, 0));
}

void CoordinateGeometry::buildSphere(QVector<Vertex>& vertices, float radius, int rings, int slices) {
    const float pi = static_cast<float>(M_PI);

    for (int ring = 0; ring < rings; ++ring) {
        float v0 = static_cast<float>(ring) / rings;
        float v1 = static_cast<float>(ring + 1) / rings;
        float theta0 = v0 * pi;
        float theta1 = v1 * pi;

        for (int slice = 0; slice < slices; ++slice) {
            float u0 = static_cast<float>(slice) / slices;
            float u1 = static_cast<float>(slice + 1) / slices;
            float phi0 = u0 * pi * 2.0f;
            float phi1 = u1 * pi * 2.0f;

            QVector3D p00(qSin(theta0) * qCos(phi0), qCos(theta0), qSin(theta0) * qSin(phi0));
            QVector3D p10(qSin(theta1) * qCos(phi0), qCos(theta1), qSin(theta1) * qSin(phi0));
            QVector3D p11(qSin(theta1) * qCos(phi1), qCos(theta1), qSin(theta1) * qSin(phi1));
            QVector3D p01(qSin(theta0) * qCos(phi1), qCos(theta0), qSin(theta0) * qSin(phi1));

            vertices.append(makeVertex(p00 * radius, p00.normalized(), QVector2D(u0, v0)));
            vertices.append(makeVertex(p10 * radius, p10.normalized(), QVector2D(u0, v1)));
            vertices.append(makeVertex(p11 * radius, p11.normalized(), QVector2D(u1, v1)));

            vertices.append(makeVertex(p00 * radius, p00.normalized(), QVector2D(u0, v0)));
            vertices.append(makeVertex(p11 * radius, p11.normalized(), QVector2D(u1, v1)));
            vertices.append(makeVertex(p01 * radius, p01.normalized(), QVector2D(u1, v0)));
        }
    }
}
