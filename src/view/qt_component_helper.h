/**
 * @file        qt_component_helper.h
 * @brief       Qt 组件助手头文件，提供 Qt 相关组件工具函数。
 * @details     定义 QtComponentHelper 类，提供 Qt 组件相关的工具函数，如显示关于对话框等。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_Q_WIDGETS_COMPONENT_HELPER_H
#define EGRET_PHYSICS_Q_WIDGETS_COMPONENT_HELPER_H
#include <QApplication>
#include <QWidget>
#include <QObject>

namespace egret
{

    /**
     * @brief       Qt 组件助手类，提供 Qt 相关组件工具函数。
     * @details     QtComponentHelper 继承自 QObject，是 Qt 组件助手类，
     *              提供 Qt 组件相关的工具函数，如显示关于对话框等。
     *              支持 QML 调用和静态方法调用。
     *              用于 Qt 应用程序的辅助功能。
     *
     * @invariant   showAboutQt() 显示 Qt 关于对话框
     * @remark      QtComponentHelper 是 QObject，支持 QML 调用
     * @see         QObject, QApplication, QWidget
     */
    class QtComponentHelper : public QObject
    {

        Q_OBJECT
    public:
        explicit QtComponentHelper(QObject *parent = nullptr) : QObject(parent) {}

        Q_INVOKABLE static void showAboutQt();
    };

} // egret

#endif // EGRET_PHYSICS_Q_WIDGETS_COMPONENT_HELPER_H
