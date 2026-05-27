//
// Created by jehor on 2026/5/27.
//

#ifndef EGRET_PHYSICS_FILE_HANDLER_H
#define EGRET_PHYSICS_FILE_HANDLER_H
#include <QObject>
#include <QFile>
#include <QDir>
#include <QUrl>

namespace egret
{
    class FileHandler : public QObject
    {
        Q_OBJECT

    public:
        explicit FileHandler(QObject* parent = nullptr);

        Q_INVOKABLE bool saveTextToFile(const QString& url, const QString& textContent);

        Q_INVOKABLE bool saveBinaryToFile(const QString& url, const QByteArray& binaryContent);

    signals:
        void errorOccurred(const QString& errorMessage);
        void fileSaved(const QString& url);

    private:
    };
} // egret

#endif //EGRET_PHYSICS_FILE_HANDLER_H
