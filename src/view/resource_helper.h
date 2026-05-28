//
// Created by jehor on 2026/5/16.
//

#ifndef EGRET_PHYSICS_RESOURCE_HELPER_H
#define EGRET_PHYSICS_RESOURCE_HELPER_H
#include "basic_utils.h"
#include <QObject>
#include <QVariantMap>
#include <QAbstractListModel>

namespace egret
{
    class ResourceHelper : public QObject
    {
        Q_OBJECT

    public:
        explicit ResourceHelper(QObject* parent = nullptr);

        Q_INVOKABLE static QString getVersionString(const QString& prefix, const QString& suffix,
                                                    const QString& sep = " ");

        Q_INVOKABLE static int getMajorVersion();

        Q_INVOKABLE static int getMinorVersion();

        Q_INVOKABLE static int getPatchVersion();

        Q_INVOKABLE static double getG();

        Q_INVOKABLE static double getStandardGravity();

        Q_INVOKABLE static double getSpeedOfLight();

        Q_INVOKABLE static double getStandardAtmosphere();

        Q_INVOKABLE static double getEarthMass();

        Q_INVOKABLE static double getEarthMeanRadius();

        Q_INVOKABLE static double getMoonMass();

        Q_INVOKABLE static double getMoonOrbitRadius();

        Q_INVOKABLE static double getSunMass();

        Q_INVOKABLE static double getAstronomicalUnit();
        
    };



} // egret

#endif //EGRET_PHYSICS_RESOURCE_HELPER_H
