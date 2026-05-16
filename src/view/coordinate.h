#pragma once

#include <QtQuick3D/QQuick3DGeometry>
#include <QVector2D>
#include <QVector3D>

class CoordinateGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    Q_PROPERTY(Shape shape READ shape WRITE setShape NOTIFY shapeChanged)
    Q_PROPERTY(float size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(float radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(int rings READ rings WRITE setRings NOTIFY ringsChanged)
    Q_PROPERTY(int slices READ slices WRITE setSlices NOTIFY slicesChanged)

public:
    enum Shape
    {
        Cube,
        Sphere
    };

    Q_ENUM(Shape)

    struct Vertex
    {
        QVector3D position;
        QVector3D normal;
        QVector2D uv;
    };

    explicit CoordinateGeometry(QQuick3DObject* parent = nullptr);

    Shape shape() const;
    void setShape(Shape shape);

    float size() const;
    void setSize(float size);

    float radius() const;
    void setRadius(float radius);

    int rings() const;
    void setRings(int rings);

    int slices() const;
    void setSlices(int slices);

signals:
    void shapeChanged();
    void sizeChanged();
    void radiusChanged();
    void ringsChanged();
    void slicesChanged();

private:
    void updateGeometry();
    void buildCube(QVector<Vertex>& vertices, float size);
    void buildSphere(QVector<Vertex>& vertices, float radius, int rings, int slices);

    Shape m_shape = Cube;
    float m_size = 1.0f;
    float m_radius = 0.5f;
    int m_rings = 16;
    int m_slices = 32;
};
