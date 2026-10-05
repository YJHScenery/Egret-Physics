/**
 * @file        series_lod.h
 * @brief       时间序列的多分辨率 min/max 抽稀（LOD）。
 * @details     绘图只需要「屏幕上看得见的点」，而不是全部样本。MinMaxLod 在原始列上
 *              建立分级摘要：第 1 级把每 bucketSize 个样本归并成一个 (min, max) 对，
 *              第 2 级再对第 1 级归并，以此类推。查询某个横轴区间时按区间长度选择合适层级，
 *              把每个像素列压缩成两个点（极大值与极小值），于是：
 *
 *              - 绘图点数始终是 O(像素宽度)，与原始样本数无关；
 *              - 尖峰与瞬态不会被均匀抽样抹掉（保留包络），适合物理量的冲击分析；
 *              - 摘要本身很小：5×10⁵ 个样本、bucketSize = 512 时约 60 KB。
 *
 *              本类不做范围过滤以外的任何统计，也不持有数据所有权：build() 只记录
 *              原始数组指针，调用方必须保证在 LOD 生命周期内数组有效且不被移动。
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

#ifndef EGRET_PHYSICS_SERIES_LOD_H
#define EGRET_PHYSICS_SERIES_LOD_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace egret::analysis
{
    /** 抽稀后的一个绘图点。 */
    struct LodPoint
    {
        /** 横轴数值。 */
        double x{0.0};

        /** 纵轴数值。 */
        double y{0.0};
    };

    /**
     * @brief       多分辨率 min/max 抽稀器。
     * @details     典型用法：build() 一次，随后每次缩放 / 平移调用 decimate()，
     *              只有区间长度变化时才需要重新查询。
     *
     * @invariant   sampleCount() > 0 时 m_x 与 m_y 指向同一长度的有效数组
     */
    class MinMaxLod final
    {
    public:
        /**
         * @brief  建立索引与分级摘要。
         * @param  x 横轴数组，长度至少 count；不可为 nullptr（count > 0 时）。
         * @param  y 纵轴数组，长度至少 count；不可为 nullptr（count > 0 时）。
         * @param  count 样本个数。
         * @param  bucketSize 第 1 级每个桶覆盖的样本数，必须 ≥ 2。
         */
        void build(const double* x,
                   const double* y,
                   std::size_t count,
                   std::size_t bucketSize = 512);

        /** 清空并释放摘要。 */
        void clear() noexcept;

        /** 是否没有数据。 */
        [[nodiscard]] bool empty() const noexcept
        {
            return m_count == 0;
        }

        /** 原始样本个数。 */
        [[nodiscard]] std::size_t sampleCount() const noexcept
        {
            return m_count;
        }

        /** 横轴最小值（要求横轴单调不减）。 */
        [[nodiscard]] double xMin() const noexcept
        {
            return m_xMin;
        }

        /** 横轴最大值。 */
        [[nodiscard]] double xMax() const noexcept
        {
            return m_xMax;
        }

        /** 纵轴最小值。 */
        [[nodiscard]] double yMin() const noexcept
        {
            return m_yMin;
        }

        /** 纵轴最大值。 */
        [[nodiscard]] double yMax() const noexcept
        {
            return m_yMax;
        }

        /** 横轴是否单调不减（不单调时区间查询退化为全区间抽稀）。 */
        [[nodiscard]] bool isXMonotonic() const noexcept
        {
            return m_xMonotonic;
        }

        /**
         * @brief       按横轴区间抽稀。
         * @details     结果点按横轴顺序排列，每个输出点来自原始样本（不是插值），
         *              因此不会产生原始数据中不存在的数值。
         * @param  xMin 区间左端。
         * @param  xMax 区间右端，必须 ≥ xMin（否则自动交换）。
         * @param  maxPoints 期望的最大输出点数，至少为 4。
         * @param  out 输出点集，不可为 nullptr；函数会先清空它。
         */
        void decimate(double xMin, double xMax, std::size_t maxPoints, std::vector<LodPoint>* out) const;

        /**
         * @brief  对全区间做均匀抽样（用于「显示全量散点」这类快速预览）。
         * @param  maxPoints 期望的最大输出点数。
         * @param  out 输出点集，不可为 nullptr。
         */
        void decimateUniform(std::size_t maxPoints, std::vector<LodPoint>* out) const;

    private:
        /** 分级摘要中的一个桶。 */
        struct Bucket
        {
            /** 桶内纵轴最小值。 */
            double yMin{0.0};

            /** 桶内纵轴最大值。 */
            double yMax{0.0};

            /** 最小值所在样本下标。 */
            std::uint64_t indexMin{0};

            /** 最大值所在样本下标。 */
            std::uint64_t indexMax{0};
        };

        /**
         * @brief  二分查找第一个横轴 ≥ value 的样本下标。
         * @param  value 目标横轴值。
         * @return 样本下标，范围 [0, m_count]。
         */
        [[nodiscard]] std::size_t lowerBound(double value) const noexcept;

        /**
         * @brief  某个层级每个桶覆盖的样本数。
         * @param  level 层级（从 1 开始）。
         * @return 样本数。
         */
        [[nodiscard]] std::uint64_t samplesPerBucket(std::size_t level) const noexcept;

        /** 原始横轴数组（不持有所有权）。 */
        const double* m_x{nullptr};

        /** 原始纵轴数组（不持有所有权）。 */
        const double* m_y{nullptr};

        /** 样本个数。 */
        std::size_t m_count{0};

        /** 第 1 级的桶大小。 */
        std::size_t m_bucketSize{512};

        /** 分级摘要，下标 0 对应第 1 级。 */
        std::vector<std::vector<Bucket>> m_levels{};

        /** 横轴最小值。 */
        double m_xMin{0.0};

        /** 横轴最大值。 */
        double m_xMax{0.0};

        /** 纵轴最小值。 */
        double m_yMin{0.0};

        /** 纵轴最大值。 */
        double m_yMax{0.0};

        /** 横轴是否单调不减。 */
        bool m_xMonotonic{true};
    };
}

#endif // EGRET_PHYSICS_SERIES_LOD_H
