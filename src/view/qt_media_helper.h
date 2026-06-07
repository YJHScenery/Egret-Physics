//
// Created by jehor on 2026/6/7.
//

#ifndef EGRET_PHYSICS_QT_MEDIA_HELPER_H
#define EGRET_PHYSICS_QT_MEDIA_HELPER_H
#include <QObject>
#include <QtMultimedia/QSoundEffect>


namespace egret
{
    class QtMediaHelper: public QSoundEffect
    {
    Q_OBJECT
    public:
        static QtMediaHelper& instance();

        Q_INVOKABLE static void playSound(std::uint32_t type);

        Q_INVOKABLE static void playSound(const QString& soundType);
    };
} // egret

#endif //EGRET_PHYSICS_QT_MEDIA_HELPER_H
