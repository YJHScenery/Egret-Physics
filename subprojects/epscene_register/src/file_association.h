/**
 * @file        file_association.h
 * @brief       Windows 文件关联接口：把 .epscene 扩展名注册到当前用户。
 * @details     关联只写 `HKCU\Software\Classes`（当前用户），不需要管理员权限，
 *              也不覆盖系统级（HKLM）关联。注册后资源管理器双击 .epscene 文件会以
 *              `"<编辑器可执行文件>" --edit "%1"` 启动主程序，并在场景编辑器工作区
 *              载入该场景；文件图标使用本工具内嵌的 `serialization-scene-json-file.ico`。
 *
 *              与 .epsx 关联（子项目 `subprojects/epsx_register`）保持同一套写法：
 *              - ProgID 固定为 `EgretPhysics.Scene`；
 *              - 只在用户区写入，注销时整体删除这两个键。
 *
 * @author      jehor
 * @date        2026-10-06
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     EpsceneRegister
 * @defgroup    FileAssociation (场景文件关联)
 */

#ifndef EGRET_PHYSICS_EPSCENE_REGISTER_FILE_ASSOCIATION_H
#define EGRET_PHYSICS_EPSCENE_REGISTER_FILE_ASSOCIATION_H

#include <QString>

namespace egret::epscene_register
{
    /**
     * @brief 文件关联操作的结果。
     * @details 同时携带「是否成功」与一段面向用户的中文描述，
     *          便于命令行入口直接打印，无需再做二次判断。
     */
    struct FileAssociationResult
    {
        /** 操作是否成功。 */
        bool succeeded{false};

        /** 成功时是结果说明，失败时是失败原因。 */
        QString message{};
    };

    /**
     * @brief  把 .epscene 关联到指定的编辑器可执行文件。
     * @details 会先确保图标文件已落到用户数据目录，再写入扩展名、ProgID、
     *          DefaultIcon 与 shell\open\command，最后通知资源管理器刷新图标缓存。
     * @param  executablePath 编辑器可执行文件的路径；传空串时取本工具同目录下的
     *                        `Egret_Physics.exe`。路径必须真实存在。
     * @return 操作结果；失败时 message 给出具体原因。
     */
    [[nodiscard]] FileAssociationResult registerEpsceneFileAssociation(const QString& executablePath = {});

    /**
     * @brief  注销 .epscene 关联。
     * @details 删除 `HKCU\Software\Classes\EgretPhysics.Scene` 与
     *          `HKCU\Software\Classes\.epscene` 两个键；键本身不存在不算失败。
     * @return 操作结果；失败时 message 给出具体原因。
     */
    [[nodiscard]] FileAssociationResult unregisterEpsceneFileAssociation();
}

#endif // EGRET_PHYSICS_EPSCENE_REGISTER_FILE_ASSOCIATION_H
