/**
 * @file        json_controller.h
 * @brief       JSON 数据控制器头文件，提供 JSON 数据的读取和写入功能。
 * @details     定义 JsonController 类，用于处理 JSON 格式数据的解析和序列化。
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
#ifndef EGRET_PHYSICS_JSON_CONTROLLER_H
#define EGRET_PHYSICS_JSON_CONTROLLER_H

namespace egret
{
    /**
     * @brief       JSON 数据控制器类，提供 JSON 数据的读取和写入功能。
     * @details     JsonController 是 JSON 数据控制器类，
     *              用于处理 JSON 格式数据的解析和序列化。
     *              支持 JSON 文件的读取、写入、解析、验证等操作。
     *              采用单例模式，全局唯一实例。
     *              提供错误处理和格式验证功能。
     *              用于物理引擎的配置文件和场景序列化。
     *
     * @invariant   instance() 返回全局唯一实例
     * @remark      JsonController 是单例，全局唯一实例
     * @see         DatabaseController, QJsonDocument, QJsonObject
     */
    class JsonController
    {
    };
}

#endif // EGRET_PHYSICS_JSON_CONTROLLER_H
