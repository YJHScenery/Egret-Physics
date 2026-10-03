/**
 * @file        file_association.h
 * @brief       Windows 文件关联接口：把 .epsx 扩展名注册到当前用户。
 * @details     关联只写 `HKCU\Software\Classes`（当前用户），不需要管理员权限，
 *              也不覆盖系统级（HKLM）关联。注册后资源管理器双击 .epsx 文件会以
 *              `"<可执行文件>" --play "%1"` 启动播放器；文件图标使用随程序分发的
 *              `simulation-file-blue.ico`。
 *
 *              与格式规范（docs/EPSX 模拟数据文件格式规范（草案）.md §2.2）保持一致：
 *              - ProgID 固定为 `EgretPhysics.Simulation`；
 *              - 只在用户区写入，注销时整体删除这两个键。
 *
 * @author      jehor
 * @date        2026-10-03
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    FileAssociation (文件关联)
 */

#ifndef EGRET_PHYSICS_FILE_ASSOCIATION_H
#define EGRET_PHYSICS_FILE_ASSOCIATION_H

#include <QString>

namespace egret
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
     * @brief  把 .epsx 关联到当前可执行文件。
     * @details 会先确保图标文件已落到用户数据目录，再写入扩展名、ProgID、
     *          DefaultIcon 与 shell\open\command，最后通知资源管理器刷新图标缓存。
     * @return 操作结果；失败时 message 给出具体原因。
     */
    [[nodiscard]] FileAssociationResult registerEpsxFileAssociation();

    /**
     * @brief  注销 .epsx 关联。
     * @details 删除 `HKCU\Software\Classes\EgretPhysics.Simulation` 与
     *          `HKCU\Software\Classes\.epsx` 两个键；键本身不存在不算失败。
     * @return 操作结果；失败时 message 给出具体原因。
     */
    [[nodiscard]] FileAssociationResult unregisterEpsxFileAssociation();
}

#endif // EGRET_PHYSICS_FILE_ASSOCIATION_H
