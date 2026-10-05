/**
 * @file        qcustomplot_item.h
 * @brief       QCustomPlot 的 QML 桥接项。
 * @details     QCustomPlot 是成熟的 GPLv3 开源科学绘图库（单头单源），自带
 *              拖拽平移、滚轮缩放（以光标为锚点）、框选放大、自适应刻度、自适应采样、
 *              图例与十字光标等能力，这正是「数学函数绘制器」式的交互所需要的。
 *
 *              它是 QWidget 实现，无法直接放进 QML 场景，因此这里用 QQuickPaintedItem
 *              承载：在 GUI 线程把图渲染成 QImage 缓存，绘制时只做一次贴图；
 *              鼠标与滚轮事件换算到设备像素后转发给内部的 QCustomPlot。
 *
 * @author      jehor
 * @date        2026-10-05
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    Analysis (数据分析)
 */

#ifndef EGRET_PHYSICS_QCUSTOMPLOT_ITEM_H
#define EGRET_PHYSICS_QCUSTOMPLOT_ITEM_H

#include <QImage>
#include <QQuickPaintedItem>
#include <QVariantList>

class QCustomPlot;
class QCPGraph;

namespace egret::analysis
{
    /**
     * @brief       嵌在 QML 中的 QCustomPlot 图表项。
     * @details     数据由 QML 通过 setPrimarySeries / setOverlaySeries 传入（点集为
     *              QPointF 列表），交互全部由 QCustomPlot 自己处理，刻度由
     *              QCPAxisTicker 自动选取「整齐」间隔，缩放后自动重算。
     *
     * @invariant   m_plot 在对象生命周期内始终有效
     */
    class QCustomPlotItem : public QQuickPaintedItem
    {
        Q_OBJECT

    public:
        /**
         * @brief  构造并初始化绘图外观与交互。
         * @param  parent QML 父项。
         */
        explicit QCustomPlotItem(QQuickItem* parent = nullptr);

        /** 析构时释放内部 QCustomPlot。 */
        ~QCustomPlotItem() override;

        /**
         * @brief  绘制缓存的图像。
         * @param  painter 绘制器。
         */
        void paint(QPainter* painter) override;

        /** 清空两条曲线。 */
        Q_INVOKABLE void clearSeries();

        /**
         * @brief  设置主曲线（实测数据）。
         * @param  points QPointF 列表。
         * @param  label 图例名称。
         */
        Q_INVOKABLE void setPrimarySeries(const QVariantList& points, const QString& label);

        /**
         * @brief  设置叠加曲线（拟合曲线或理论曲线）。
         * @param  points QPointF 列表。
         * @param  label 图例名称。
         */
        Q_INVOKABLE void setOverlaySeries(const QVariantList& points, const QString& label);

        /** 恢复为全部数据的完整范围。 */
        Q_INVOKABLE void resetView();

        /**
         * @brief  以视图中心为基准缩放。
         * @param  factor < 1 放大，> 1 缩小。
         */
        Q_INVOKABLE void zoomBy(double factor);

        /**
         * @brief  开关框选放大模式。
         * @param  enabled true 时左键拖拽为框选放大，false 时为平移。
         */
        Q_INVOKABLE void setAreaZoomEnabled(bool enabled);

        /** 当前可见横轴下限。 */
        [[nodiscard]] Q_INVOKABLE double visibleXMin() const;

        /** 当前可见横轴上限。 */
        [[nodiscard]] Q_INVOKABLE double visibleXMax() const;

    protected:
        /** 转发鼠标按下。 */
        void mousePressEvent(QMouseEvent* event) override;

        /** 转发鼠标移动（含悬停，用于读数）。 */
        void mouseMoveEvent(QMouseEvent* event) override;

        /** 转发鼠标释放。 */
        void mouseReleaseEvent(QMouseEvent* event) override;

        /** 转发滚轮。 */
        void wheelEvent(QWheelEvent* event) override;

        /** 转发悬停移动。 */
        void hoverMoveEvent(QHoverEvent* event) override;

        /** 尺寸变化时同步内部控件并重绘缓存。 */
        void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

    private:
        /** 按当前尺寸重新渲染缓存图像。 */
        void refreshCache();

        /** 逻辑像素到设备像素的比例。 */
        [[nodiscard]] double plotScale() const;

        /** 内部绘图控件。 */
        QCustomPlot* m_plot{nullptr};

        /** 主曲线。 */
        QCPGraph* m_primary{nullptr};

        /** 叠加曲线。 */
        QCPGraph* m_overlay{nullptr};

        /** 缓存图像（GUI 线程写、渲染线程读）。 */
        QImage m_cache{};
    };
}

#endif // EGRET_PHYSICS_QCUSTOMPLOT_ITEM_H
