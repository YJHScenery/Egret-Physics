/**
 * @file        series_lod.cpp
 * @brief       多分辨率 min/max 抽稀实现。
 * @details     建塔是一次性的 O(N) 扫描，之后每次区间查询只访问 O(像素宽度) 个桶。
 *              抽稀输出点全部来自原始样本，不做插值，保证图上不出现原始数据里没有的数值。
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

#include "series_lod.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace egret::analysis
{
    void MinMaxLod::clear() noexcept
    {
        m_x = nullptr;
        m_y = nullptr;
        m_count = 0;
        m_levels.clear();
        m_xMin = 0.0;
        m_xMax = 0.0;
        m_yMin = 0.0;
        m_yMax = 0.0;
        m_xMonotonic = true;
    }

    void MinMaxLod::build(const double* x,
                          const double* y,
                          const std::size_t count,
                          const std::size_t bucketSize)
    {
        clear();
        if (x == nullptr || y == nullptr || count == 0)
        {
            return;
        }

        m_x = x;
        m_y = y;
        m_count = count;
        m_bucketSize = std::max<std::size_t>(2, bucketSize);

        // ---- 范围与单调性 ----
        m_xMonotonic = true;
        m_xMin = x[0];
        m_xMax = x[0];
        m_yMin = y[0];
        m_yMax = y[0];
        for (std::size_t i = 1; i < count; ++i)
        {
            if (x[i] < x[i - 1])
            {
                m_xMonotonic = false;
            }
            m_xMin = std::min(m_xMin, x[i]);
            m_xMax = std::max(m_xMax, x[i]);
            m_yMin = std::min(m_yMin, y[i]);
            m_yMax = std::max(m_yMax, y[i]);
        }

        // ---- 第 1 级：直接在原始样本上归并 ----
        std::vector<Bucket> first{};
        first.reserve((count + m_bucketSize - 1) / m_bucketSize);
        for (std::size_t begin = 0; begin < count; begin += m_bucketSize)
        {
            const std::size_t end = std::min(count, begin + m_bucketSize);
            Bucket bucket{};
            bucket.yMin = y[begin];
            bucket.yMax = y[begin];
            bucket.indexMin = begin;
            bucket.indexMax = begin;
            for (std::size_t i = begin + 1; i < end; ++i)
            {
                if (y[i] < bucket.yMin)
                {
                    bucket.yMin = y[i];
                    bucket.indexMin = i;
                }
                if (y[i] > bucket.yMax)
                {
                    bucket.yMax = y[i];
                    bucket.indexMax = i;
                }
            }
            first.push_back(bucket);
        }
        m_levels.push_back(std::move(first));

        // ---- 逐级归并直到只剩一个桶 ----
        while (m_levels.back().size() > 1)
        {
            const std::vector<Bucket> previous = m_levels.back();
            std::vector<Bucket> next{};
            next.reserve((previous.size() + m_bucketSize - 1) / m_bucketSize);
            for (std::size_t begin = 0; begin < previous.size(); begin += m_bucketSize)
            {
                const std::size_t end = std::min(previous.size(), begin + m_bucketSize);
                Bucket bucket = previous[begin];
                for (std::size_t i = begin + 1; i < end; ++i)
                {
                    if (previous[i].yMin < bucket.yMin)
                    {
                        bucket.yMin = previous[i].yMin;
                        bucket.indexMin = previous[i].indexMin;
                    }
                    if (previous[i].yMax > bucket.yMax)
                    {
                        bucket.yMax = previous[i].yMax;
                        bucket.indexMax = previous[i].indexMax;
                    }
                }
                next.push_back(bucket);
            }
            m_levels.push_back(std::move(next));
        }
    }

    std::size_t MinMaxLod::lowerBound(const double value) const noexcept
    {
        std::size_t low = 0;
        std::size_t high = m_count;
        while (low < high)
        {
            const std::size_t mid = low + (high - low) / 2;
            if (m_x[mid] < value)
            {
                low = mid + 1;
            }
            else
            {
                high = mid;
            }
        }
        return low;
    }

    std::uint64_t MinMaxLod::samplesPerBucket(const std::size_t level) const noexcept
    {
        std::uint64_t samples = 1;
        for (std::size_t i = 0; i < level; ++i)
        {
            if (samples > (std::numeric_limits<std::uint64_t>::max)() / m_bucketSize)
            {
                return (std::numeric_limits<std::uint64_t>::max)();
            }
            samples *= static_cast<std::uint64_t>(m_bucketSize);
        }
        return samples;
    }

    void MinMaxLod::decimate(const double xMin,
                             double xMax,
                             std::size_t maxPoints,
                             std::vector<LodPoint>* out) const
    {
        if (out == nullptr)
        {
            return;
        }
        out->clear();
        if (m_count == 0)
        {
            return;
        }
        maxPoints = std::max<std::size_t>(4, maxPoints);

        // ---- 定位区间 ----
        std::size_t lo = 0;
        std::size_t hi = m_count - 1;
        if (m_xMonotonic)
        {
            double left = xMin;
            double right = xMax;
            if (right < left)
            {
                std::swap(left, right);
            }
            lo = lowerBound(left);
            const std::size_t afterRight = lowerBound(right);
            hi = (afterRight == 0) ? 0 : afterRight - 1;
            if (lo >= m_count || lo > hi)
            {
                return;
            }
        }

        const auto emit = [out, this](const std::size_t index)
        {
            out->push_back(LodPoint{m_x[index], m_y[index]});
        };

        const std::size_t range = hi - lo + 1;
        if (range <= maxPoints)
        {
            out->reserve(range);
            for (std::size_t i = lo; i <= hi; ++i)
            {
                emit(i);
            }
            return;
        }

        // ---- 选择层级：最细且仍足够粗的层级 ----
        const std::size_t target = std::max<std::size_t>(1, maxPoints / 2);
        const std::uint64_t step = std::max<std::uint64_t>(1, range / target);

        std::size_t chosenLevel = 0;
        for (std::size_t level = m_levels.size(); level >= 1; --level)
        {
            if (samplesPerBucket(level) <= step)
            {
                chosenLevel = level;
                break;
            }
        }

        if (chosenLevel == 0)
        {
            // 区间本身很短但密度很高：直接按原始样本步进，避免过度归并
            const std::uint64_t stride = std::max<std::uint64_t>(1, range / maxPoints);
            for (std::size_t i = lo; i <= hi; i += static_cast<std::size_t>(stride))
            {
                emit(i);
            }
            if (!out->empty() && out->back().x < m_x[hi])
            {
                emit(hi);
            }
            return;
        }

        const std::vector<Bucket>& buckets = m_levels[chosenLevel - 1];
        const std::uint64_t perBucket = samplesPerBucket(chosenLevel);
        const std::uint64_t bucketStride = std::max<std::uint64_t>(1, step / perBucket);
        const std::size_t firstBucket = lo / static_cast<std::size_t>(perBucket);
        const std::size_t lastBucket = std::min(buckets.size() - 1,
                                                hi / static_cast<std::size_t>(perBucket));
        out->reserve(2 * ((lastBucket - firstBucket) / static_cast<std::size_t>(bucketStride) + 1));

        for (std::size_t index = firstBucket; index <= lastBucket;
             index += static_cast<std::size_t>(bucketStride))
        {
            const Bucket& bucket = buckets[index];
            const std::size_t indexMin = static_cast<std::size_t>(bucket.indexMin);
            const std::size_t indexMax = static_cast<std::size_t>(bucket.indexMax);
            if (indexMin <= indexMax)
            {
                out->push_back(LodPoint{m_x[indexMin], bucket.yMin});
                out->push_back(LodPoint{m_x[indexMax], bucket.yMax});
            }
            else
            {
                out->push_back(LodPoint{m_x[indexMax], bucket.yMax});
                out->push_back(LodPoint{m_x[indexMin], bucket.yMin});
            }
        }

        if (!out->empty() && out->back().x < m_x[hi])
        {
            emit(hi);
        }
    }

    void MinMaxLod::decimateUniform(std::size_t maxPoints, std::vector<LodPoint>* out) const
    {
        if (out == nullptr)
        {
            return;
        }
        out->clear();
        if (m_count == 0)
        {
            return;
        }
        maxPoints = std::max<std::size_t>(2, maxPoints);

        const std::size_t stride = std::max<std::size_t>(1, m_count / maxPoints);
        out->reserve(m_count / stride + 1);
        for (std::size_t i = 0; i < m_count; i += stride)
        {
            out->push_back(LodPoint{m_x[i], m_y[i]});
        }
        if (out->back().x < m_x[m_count - 1])
        {
            out->push_back(LodPoint{m_x[m_count - 1], m_y[m_count - 1]});
        }
    }
}
