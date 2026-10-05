/**
 * @file        analysis_exporter.h
 * @brief       把抽取结果导出为 xlsx / CSV 表格。
 * @details     列结构与实时仿真器的追踪导出保持一致（`时间` + 各通道各一列，
 *              列名形如 `位置-x/m`），因此两份数据可以直接比对。多实体时：
 *
 *              - xlsx：每个实体一个工作表，另加一个「元数据」工作表；
 *              - CSV：每个实体一个文件，文件名追加实体名。
 *
 *              导出在后台线程执行，带进度与取消回调。QXlsx 是「整本工作簿驻留内存」
 *              的实现，因此设置了单元格上限：超限时 xlsx 直接拒绝并提示改用 CSV，
 *              CSV 是逐行流式写出，不受该限制。
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

#ifndef EGRET_PHYSICS_ANALYSIS_EXPORTER_H
#define EGRET_PHYSICS_ANALYSIS_EXPORTER_H

#include <QString>

#include <cstdint>

#include "analysis_types.h"

namespace egret::analysis
{
    /** 导出格式。 */
    enum class ExportFormat : std::uint8_t
    {
        /** Excel 工作簿（每实体一个工作表）。 */
        Xlsx = 0,

        /** 逗号分隔文本（逐行流式写出，适合超大数据量）。 */
        Csv = 1,
    };

    /** 导出参数。 */
    struct ExportOptions
    {
        /** 目标格式。 */
        ExportFormat format{ExportFormat::Xlsx};

        /** 目标文件路径；CSV 多实体时作为文件名模板。 */
        QString targetPath{};

        /** xlsx 允许的最大单元格数（行 × 列），超出即拒绝。 */
        std::uint64_t maxCells{2'000'000};

        /** 是否写出元数据工作表（仅 xlsx）。 */
        bool writeMetadataSheet{true};
    };

    /** 导出报告。 */
    struct ExportReport
    {
        /** 写出的数据行数（不含表头）。 */
        std::uint64_t rows{0};

        /** 写出的单元格总数（估算，用于与上限比较）。 */
        std::uint64_t cells{0};

        /** 写出的文件个数。 */
        std::uint64_t fileCount{0};

        /** 实际写出的文件路径列表。 */
        QStringList writtenFiles{};

        /** 是否因取消而只写出了部分数据。 */
        bool cancelled{false};

        /** 耗时（秒）。 */
        double elapsedSeconds{0.0};
    };

    /**
     * @brief       抽取结果导出器。
     * @details     所有接口均为静态，调用方无需持有实例。
     *
     * @invariant   返回 true 且未取消时，report->rows 等于 result.sampledCount
     */
    class AnalysisExporter final
    {
    public:
        /**
         * @brief  执行一次导出。
         * @param  result 抽取结果。
         * @param  options 导出参数。
         * @param  callbacks 进度与取消回调，可为空。
         * @param  report 导出报告，可为 nullptr。
         * @param  errorMessage 失败原因，可为 nullptr。
         * @return 成功（含被取消的部分导出）返回 true。
         */
        [[nodiscard]] static bool exportResult(const ExtractionResult& result,
                                               const ExportOptions& options,
                                               const ExtractionCallbacks& callbacks,
                                               ExportReport* report,
                                               QString* errorMessage);

        /**
         * @brief  导出格式的稳定英文键。
         * @param  format 格式。
         * @return "xlsx" 或 "csv"。
         */
        [[nodiscard]] static const char* formatKey(ExportFormat format) noexcept;
    };
}

#endif // EGRET_PHYSICS_ANALYSIS_EXPORTER_H
