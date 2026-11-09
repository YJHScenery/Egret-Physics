/** @file main.cpp @brief 三维矢量场实例化渲染独立验证程序。 */
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QtQuick3D/qquick3dinstancing.h>
#include <QtQuick3D/qquick3dgeometry.h>
#include <cmath>

/** @brief 沿正 Y 轴的方截面箭头，所有实例共享同一网格。 */
class ArrowGeometry : public QQuick3DGeometry {
    Q_OBJECT
public:
    /** @brief 构造由方柱和四棱锥组成的箭头。 */
    explicit ArrowGeometry(QQuick3DObject *parent = nullptr) : QQuick3DGeometry(parent) {
        QByteArray bytes;
        auto triangle = [&bytes](QVector3D a, QVector3D b, QVector3D c) {
            for (const auto &v : {a,b,c}) { float p[] = {v.x(),v.y(),v.z()}; bytes.append(reinterpret_cast<const char *>(p), sizeof p); }
        };
        const float w=.035f, h=.7f, head=.12f;
        QVector3D p[]={{-w,0,-w},{w,0,-w},{w,0,w},{-w,0,w}, {-w,h,-w},{w,h,-w},{w,h,w},{-w,h,w}};
        for (int i=0;i<4;++i) { int j=(i+1)%4; triangle(p[i],p[j],p[j+4]); triangle(p[i],p[j+4],p[i+4]); }
        QVector3D q[]={{-head,h,-head},{head,h,-head},{head,h,head},{-head,h,head}};
        for (int i=0;i<4;++i) triangle(q[i],q[(i+1)%4],{0,1,0});
        triangle(q[0],q[2],q[1]); triangle(q[0],q[3],q[2]);
        setVertexData(bytes); setStride(3*sizeof(float)); setPrimitiveType(PrimitiveType::Triangles);
        addAttribute(Attribute::PositionSemantic,0,Attribute::F32Type);
        setBounds({-head,0,-head},{head,1,head});
    }
};

/** @brief 以稳定的空间随机阈值将场强映射到箭头出现密度。 */
class FieldInstances : public QQuick3DInstancing {
    Q_OBJECT
    Q_PROPERTY(float time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(float extent READ extent WRITE setExtent NOTIFY extentChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    /** @brief 初始化实例表。 */
    explicit FieldInstances(QQuick3DObject *parent=nullptr) : QQuick3DInstancing(parent) {}
    /** @brief 返回模拟时间。 */ float time() const { return m_time; }
    /** @brief 返回采样范围半径。 */ float extent() const { return m_extent; }
    /** @brief 返回最近生成的实例数量。 */ int count() const { return m_count; }
    /** @brief 更新时间并请求重建实例表。 */ void setTime(float t) { if (qFuzzyCompare(t,m_time)) return; m_time=t; markDirty(); emit timeChanged(); }
    /** @brief 更新采样范围。 */ void setExtent(float v) { v=qBound(2.f,v,30.f); if (qFuzzyCompare(v,m_extent)) return; m_extent=v; markDirty(); emit extentChanged(); }
    Q_SIGNAL void timeChanged();
    Q_SIGNAL void extentChanged();
    Q_SIGNAL void countChanged();
protected:
    /** @brief 生成固定长度箭头，并以概率密度表示场强。 */
    QByteArray getInstanceBuffer(int *instanceCount) override {
        QByteArray buffer; constexpr int n=13; float spacing=2*m_extent/(n-1); int count=0;
        for (int x=0;x<n;++x) for (int y=0;y<n;++y) for (int z=0;z<n;++z) {
            QVector3D p((x-(n-1)/2.f)*spacing,(y-(n-1)/2.f)*spacing,(z-(n-1)/2.f)*spacing);
            QVector3D f(-p.z(), 2.f*std::cos(m_time+p.y()*.18f), p.x());
            float strength=qMin(1.f,f.length()/m_extent);
            unsigned hash=unsigned(x)*73856093u ^ unsigned(y)*19349663u ^ unsigned(z)*83492791u;
            hash ^= hash >> 16; hash *= 0x7feb352du; hash ^= hash >> 15;
            if ((hash & 65535u)/65535.f >= strength || f.lengthSquared()<1e-8f) continue;
            auto entry=calculateTableEntryFromQuaternion(p,{spacing*.65f,spacing*.65f,spacing*.65f},QQuaternion::rotationTo({0,1,0},f.normalized()),QColor::fromRgbF(.18,.75,1));
            buffer.append(reinterpret_cast<const char *>(&entry),sizeof entry); ++count;
        }
        *instanceCount=count; if (m_count!=count) { m_count=count; emit countChanged(); } return buffer;
    }
private:
    float m_time=0, m_extent=8; int m_count=0;
};

/** @brief 注册原型类型并启动视口。 */
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv); qmlRegisterType<ArrowGeometry>("FieldDemo",1,0,"ArrowGeometry");
    qmlRegisterType<FieldInstances>("FieldDemo",1,0,"FieldInstances");
    QQmlApplicationEngine engine; engine.load(QUrl("qrc:/Main.qml")); if (engine.rootObjects().isEmpty()) return 1; return app.exec();
}
#include "main.moc"

