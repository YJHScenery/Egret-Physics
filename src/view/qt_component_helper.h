//
// Created by jehor on 2026/5/16.
//

#ifndef EGRET_PHYSICS_Q_WIDGETS_COMPONENT_HELPER_H
#define EGRET_PHYSICS_Q_WIDGETS_COMPONENT_HELPER_H
#include <QApplication>
#include <QWidget>
#include <QObject>

namespace egret {

class QtComponentHelper: public QObject {

    Q_OBJECT
public:
    explicit QtComponentHelper(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE static void showAboutQt();

};

} // egret

#endif //EGRET_PHYSICS_Q_WIDGETS_COMPONENT_HELPER_H
