/**
 * @file        epsx_series_extractor.h
 * @brief       EPSX 序列抽取器：按实体 / 通道 / 抽帧比把文件读成列式序列。
 * @details     EpsxSeriesExtractor 是数据分析的数据入口，职责单一：
 *
 *              1. 打开 EPSX 文件并解析场景描述（复用 EpsxReader，只读、不修改文件）；
 *              2. 按用户选择的实体与通道，只解码需要的帧与槽位；
 *              3. 按抽帧比每 stride 帧取一个样本，写入列式存储；
 *              4. 补齐派生通道（模长、欧拉角、平动动能）。
 *
 *              它不做任何统计、拟合或绘图，也不持有 QML 对象，因此可以脱离 GUI
 *              在单元测试中直接调用。大文件场景下请通过 ExtractionCallbacks
 *              提供进度与取消回调，抽取本身应在后台线程执行。
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

#ifndef EGRET_PHYSICS_EPSX_SERIES_EXTRACTOR_H
#define EGRET_PHYSICS_EPSX_SERIES_EXTRACTOR_H

#include <QString>

#include <cstdint>

#include "analysis_types.h"

namespace egret::analysis
{
    /** 一次抽取的运行报告。 */
    struct ExtractionReport
    {
        /** 实际解码的帧数（含被取消前完成的部分）。 */
        std::uint64_t decodedFrames{0};

        /** 实际写入时间轴的样本数。 */
        std::uint64_t sampledCount{0};

        /** 抽取耗时（秒）。 */
        double elapsedSeconds{0.0};

        /** 是否因调用方取消而提前结束（此时结果仍可用，只是样本更少）。 */
        bool cancelled{false};
    };

    /**
     * @brief       按选择集抽取 EPSX 序列。
     * @details     所有接口均为静态，调用方无需持有实例。
     *
     * @invariant   返回 true 时 out 中任意序列的长度都等于 out->time.size()
     */
    class EpsxSeriesExtractor final
    {
    public:
        /**
         * @brief  执行一次抽取。
         * @param  request 抽取请求（文件、实体、通道、抽帧比）。
         * @param  callbacks 进度与取消回调，可为空。
         * @param  out 抽取结果，不可为 nullptr。
         * @param  report 运行报告，可为 nullptr。
         * @param  errorMessage 失败原因，可为 nullptr。
         * @return 成功（含被取消的提前结束）返回 true；文件打不开或参数非法返回 false。
         */
        [[nodiscard]] static bool extract(const ExtractionRequest& request,
                                          const ExtractionCallbacks& callbacks,
                                          ExtractionResult* out,
                                          ExtractionReport* report,
                                          QString* errorMessage);
    };
}

#endif // EGRET_PHYSICS_EPSX_SERIES_EXTRACTOR_H
