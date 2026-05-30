/**
 * @file        database_controller.h
 * @brief       数据库控制器头文件，提供数据库连接和数据操作功能。
 * @details     定义 DatabaseController 类，用于处理数据库的连接、查询和数据操作。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_DATABASE_CONTROLLER_H
#define EGRET_PHYSICS_DATABASE_CONTROLLER_H

namespace egret
{
    /**
     * @brief       数据库控制器类，提供数据库连接和数据操作功能。
     * @details     DatabaseController 是数据库控制器类，
     *              用于处理数据库的连接、查询和数据操作。
     *              支持数据库连接管理、SQL 查询执行、数据插入更新删除等操作。
     *              采用单例模式，全局唯一实例。
     *              提供事务管理和错误处理功能。
     *              用于物理引擎的数据持久化。
     *
     * @invariant   instance() 返回全局唯一实例
     * @remark      DatabaseController 是单例，全局唯一实例
     * @see         JsonController, QSqlDatabase
     */
    class DatabaseController
    {
    };
}

#endif // EGRET_PHYSICS_DATABASE_CONTROLLER_H
