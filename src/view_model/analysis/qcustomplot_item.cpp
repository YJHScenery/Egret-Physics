/**
 * @file        qcustomplot_item.cpp
 * @brief       QCustomPlot 的 QML 桥接实现。
 * @details     渲染路径：GUI 线程调用 refreshCache() → QCustomPlot::toPixmap() →
 *              QImage 缓存；paint() 在渲染线程只做贴图，避免跨线程使用 QPixmap。
 *              事件路径：QML 事件坐标乘以设备像素比后转发给 QCustomPlot 的事件处理函数，
 *              为此用一个暴露 protected 处理函数的小派生类，避免依赖隐藏控件的 sendEvent。
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

#include "qcustomplot_item.h"

#include "qcustomplot.h"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickWindow>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace egret::analysis
{
    namespace
    {
        /** 把 QCustomPlot 的 protected 事件处理函数暴露出来供桥接调用。 */
        class PlotWidget final : public QCustomPlot
        {
        public:
            using QCustomPlot::QCustomPlot;
            using QCustomPlot::mouseMoveEvent;
            using QCustomPlot::mousePressEvent;
            using QCustomPlot::mouseReleaseEvent;
            using QCustomPlot::wheelEvent;
        };
    }

    QCustomPlotItem::QCustomPlotItem(QQuickItem* parent)
        : QQuickPaintedItem(parent)
    {
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::AllButtons);
        setFlag(QQuickItem::ItemHasContents, true);

        auto* plot = new PlotWidget();
        m_plot = plot;

        // 交互：左键拖拽平移、滚轮以光标为锚点缩放；框选放大在需要时再打开
        plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        plot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
        plot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
        plot->setSelectionRectMode(QCP::srmNone);
        plot->setBackground(QBrush(QColor(10, 26, 58)));

        const QColor axisColor(141, 179, 217);
        const QColor textColor(220, 234, 255);
        for (QCPAxis* axis : {plot->xAxis, plot->yAxis})
        {
            axis->setBasePen(QPen(axisColor));
            axis->setTickPen(QPen(axisColor));
            axis->setSubTickPen(QPen(axisColor));
            axis->setTickLabelColor(textColor);
            axis->setLabelColor(textColor);
            axis->grid()->setPen(QPen(QColor(30, 58, 92), 1, Qt::DotLine));
            axis->grid()->setSubGridVisible(false);
            axis->grid()->setZeroLinePen(QPen(QColor(63, 121, 176), 1, Qt::DashLine));
        }

        plot->legend->setVisible(true);
        plot->legend->setBrush(QBrush(QColor(16, 41, 76, 200)));
        plot->legend->setBorderPen(QPen(QColor(63, 121, 176)));
        plot->legend->setTextColor(textColor);

        m_primary = plot->addGraph();
        m_primary->setPen(QPen(QColor(86, 182, 255), 2));
        m_primary->setAdaptiveSampling(true);

        m_overlay = plot->addGraph();
        m_overlay->setPen(QPen(QColor(255, 184, 77), 2));
        m_overlay->setAdaptiveSampling(true);

        plot->resize(1, 1);
    }

    QCustomPlotItem::~QCustomPlotItem()
    {
        delete m_plot;
        m_plot = nullptr;
    }

    double QCustomPlotItem::plotScale() const
    {
        const QQuickWindow* quickWindow = window();
        return quickWindow != nullptr ? quickWindow->effectiveDevicePixelRatio() : 1.0;
    }

    void QCustomPlotItem::refreshCache()
    {
        if (m_plot == nullptr)
        {
            return;
        }
        const double scale = plotScale();
        const int pixelWidth = std::max(1, static_cast<int>(std::lround(width() * scale)));
        const int pixelHeight = std::max(1, static_cast<int>(std::lround(height() * scale)));
        m_plot->resize(pixelWidth, pixelHeight);
        m_cache = m_plot->toPixmap(pixelWidth, pixelHeight, 1.0).toImage();
    }

    void QCustomPlotItem::paint(QPainter* painter)
    {
        if (painter == nullptr || m_cache.isNull())
        {
            return;
        }
        painter->drawImage(QRectF(0.0, 0.0, width(), height()), m_cache);
    }

    void QCustomPlotItem::clearSeries()
    {
        if (m_plot == nullptr)
        {
            return;
        }
        m_primary->data()->clear();
        m_overlay->data()->clear();
        refreshCache();
        update();
    }

    void QCustomPlotItem::setPrimarySeries(const QVariantList& points, const QString& label)
    {
        if (m_plot == nullptr)
        {
            return;
        }
        QVector<double> keys{};
        QVector<double> values{};
        keys.reserve(points.size());
        values.reserve(points.size());
        for (const QVariant& value : points)
        {
            const QPointF point = value.toPointF();
            keys.append(point.x());
            values.append(point.y());
        }
        m_primary->setData(keys, values);
        m_primary->setName(label);
        refreshCache();
        update();
    }

    void QCustomPlotItem::setOverlaySeries(const QVariantList& points, const QString& label)
    {
        if (m_plot == nullptr)
        {
            return;
        }
        QVector<double> keys{};
        QVector<double> values{};
        keys.reserve(points.size());
        values.reserve(points.size());
        for (const QVariant& value : points)
        {
            const QPointF point = value.toPointF();
            keys.append(point.x());
            values.append(point.y());
        }
        m_overlay->setData(keys, values);
        m_overlay->setName(label);
        m_overlay->setVisible(!keys.isEmpty());
        refreshCache();
        update();
    }

    void QCustomPlotItem::resetView()
    {
        if (m_plot == nullptr)
        {
            return;
        }
        m_plot->rescaleAxes(true);
        refreshCache();
        update();
    }

    void QCustomPlotItem::zoomBy(const double factor)
    {
        if (m_plot == nullptr)
        {
            return;
        }
        m_plot->xAxis->scaleRange(factor, m_plot->xAxis->range().center());
        m_plot->yAxis->scaleRange(factor, m_plot->yAxis->range().center());
        refreshCache();
        update();
    }

    void QCustomPlotItem::setAreaZoomEnabled(const bool enabled)
    {
        if (m_plot == nullptr)
        {
            return;
        }
        m_plot->setSelectionRectMode(enabled ? QCP::srmZoom : QCP::srmNone);
    }

    double QCustomPlotItem::visibleXMin() const
    {
        return m_plot != nullptr ? m_plot->xAxis->range().lower : 0.0;
    }

    double QCustomPlotItem::visibleXMax() const
    {
        return m_plot != nullptr ? m_plot->xAxis->range().upper : 0.0;
    }

    void QCustomPlotItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
    {
        QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
        refreshCache();
        update();
    }

    void QCustomPlotItem::mousePressEvent(QMouseEvent* event)
    {
        const double scale = plotScale();
        QMouseEvent mapped{event->type(),
                           event->position() * scale,
                           event->scenePosition() * scale,
                           event->globalPosition() * scale,
                           event->button(),
                           event->buttons(),
                           event->modifiers(),
                           event->pointingDevice()};
        static_cast<PlotWidget*>(m_plot)->mousePressEvent(&mapped);
        refreshCache();
        update();
        event->accept();
    }

    void QCustomPlotItem::mouseMoveEvent(QMouseEvent* event)
    {
        const double scale = plotScale();
        QMouseEvent mapped{event->type(),
                           event->position() * scale,
                           event->scenePosition() * scale,
                           event->globalPosition() * scale,
                           event->button(),
                           event->buttons(),
                           event->modifiers(),
                           event->pointingDevice()};
        static_cast<PlotWidget*>(m_plot)->mouseMoveEvent(&mapped);
        refreshCache();
        update();
        event->accept();
    }

    void QCustomPlotItem::hoverMoveEvent(QHoverEvent* event)
    {
        // 把悬停当作无按键的移动交给 QCustomPlot，用于十字光标读数
        const double scale = plotScale();
        QMouseEvent mapped{QEvent::MouseMove,
                           event->position() * scale,
                           event->position() * scale,
                           mapToGlobal(event->position()) * scale,
                           Qt::NoButton,
                           Qt::NoButton,
                           Qt::NoModifier};
        static_cast<PlotWidget*>(m_plot)->mouseMoveEvent(&mapped);
        refreshCache();
        update();
        event->accept();
    }

    void QCustomPlotItem::mouseReleaseEvent(QMouseEvent* event)
    {
        const double scale = plotScale();
        QMouseEvent mapped{event->type(),
                           event->position() * scale,
                           event->scenePosition() * scale,
                           event->globalPosition() * scale,
                           event->button(),
                           event->buttons(),
                           event->modifiers(),
                           event->pointingDevice()};
        static_cast<PlotWidget*>(m_plot)->mouseReleaseEvent(&mapped);
        refreshCache();
        update();
        event->accept();
    }

    void QCustomPlotItem::wheelEvent(QWheelEvent* event)
    {
        const double scale = plotScale();
        QWheelEvent mapped{event->position() * scale,
                           event->globalPosition() * scale,
                           event->pixelDelta(),
                           event->angleDelta(),
                           event->buttons(),
                           event->modifiers(),
                           event->phase(),
                           event->inverted(),
                           event->source(),
                           event->pointingDevice()};
        static_cast<PlotWidget*>(m_plot)->wheelEvent(&mapped);
        refreshCache();
        update();
        event->accept();
    }
}
