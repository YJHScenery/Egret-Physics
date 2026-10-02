/**
 * @file        eps_reader.h
 * @brief       EPS 文件随机访问读取器。
 * @details     EpsReader 打开 EPS 文件后先校验文件头，再读取场景描述块与帧索引块，
 *              之后可以按帧号 O(1) 定位并解码任意一帧，供播放器拖动进度条或调试时定点取帧。
 *
 *              容错策略：
 *              - 文件头 / 数据块 / 帧段三层都做 CRC-32C 校验，任一层损坏都会给出明确定位；
 *              - 若缺少尾块与索引块（求解中途崩溃导致文件未收尾），会自动改为顺序扫描帧块，
 *                仍能读出已经完整写入的帧。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Serialize
 * @defgroup    Eps (Egret Physics Simulation 文件格式)
 */

#ifndef EGRET_PHYSICS_EPS_READER_H
#define EGRET_PHYSICS_EPS_READER_H

#include <QFile>
#include <QString>

#include <cstdint>
#include <string>
#include <vector>

#include "eps_format.h"

namespace egret::eps
{
    /**
     * @brief       场景描述块中声明的帧段布局。
     * @details     用于校验帧内出现的段是否与声明一致。
     */
    struct SectionDescriptor
    {
        /** 段类型标记。 */
        FourCC tag{};

        /** 单元素字节数。 */
        std::uint32_t strideBytes{0};

        /** 元素个数。 */
        std::uint32_t elementCount{0};
    };

    /**
     * @brief       从场景描述块解析出的元信息。
     * @details     这些字段以 JSON 形式存放在 SCNE 块中，读取后缓存供播放器与调试工具使用。
     */
    struct SceneMetadata
    {
        /** 求解推进步长（纳秒）。 */
        std::uint64_t solverStepNs{kDefaultSolverStepNs};

        /** 采样间隔（纳秒）。 */
        std::uint64_t sampleIntervalNs{kDefaultSampleIntervalNs};

        /** 抽帧比 k。 */
        std::uint64_t decimation{20};

        /** 场景声明的帧总数（可能与实际写入帧数不同，以后者为准）。 */
        std::uint64_t declaredFrameCount{0};

        /** 生成者描述。 */
        std::string generator{};

        /** 帧段布局声明。 */
        std::vector<SectionDescriptor> sections{};
    };

    /**
     * @brief       随机访问读取器。
     * @details     使用方式：open → readFrame(任意帧号) → close（或析构自动关闭）。
     *
     * @invariant   m_open 为 true 时 m_file 处于已打开状态
     * @invariant   m_index.size() 等于 frameCount()
     */
    class EpsReader final
    {
    public:
        /**
         * @brief 绑定待读取的文件路径。
         * @param filePath 文件路径。
         */
        explicit EpsReader(QString filePath);

        /** 析构时自动关闭文件。 */
        ~EpsReader();

        EpsReader(const EpsReader&) = delete;

        EpsReader& operator=(const EpsReader&) = delete;

        EpsReader(EpsReader&&) = delete;

        EpsReader& operator=(EpsReader&&) = delete;

        /**
         * @brief  打开并解析文件头、场景描述块与帧索引。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool open(QString* errorMessage);

        /** 关闭文件并释放索引。 */
        void close();

        /** 是否已成功打开。 */
        [[nodiscard]] bool isOpen() const noexcept
        {
            return m_open;
        }

        /** 文件路径。 */
        [[nodiscard]] const QString& filePath() const noexcept
        {
            return m_filePath;
        }

        /** 场景描述块中的原始 JSON 文本。 */
        [[nodiscard]] const std::string& sceneJson() const noexcept
        {
            return m_sceneJson;
        }

        /** 从场景描述块解析出的元信息。 */
        [[nodiscard]] const SceneMetadata& metadata() const noexcept
        {
            return m_metadata;
        }

        /** 实际可读取的帧数。 */
        [[nodiscard]] std::uint64_t frameCount() const noexcept
        {
            return static_cast<std::uint64_t>(m_index.size());
        }

        /** 文件头中声明的采样间隔（纳秒）。 */
        [[nodiscard]] std::uint64_t sampleIntervalNs() const noexcept
        {
            return m_sampleIntervalNs;
        }

        /** 文件头中声明的刚体数量。 */
        [[nodiscard]] std::uint32_t bodyCount() const noexcept
        {
            return m_bodyCount;
        }

        /** 是否存在尾块（存在表示写入正常收尾）。 */
        [[nodiscard]] bool hasTrailer() const noexcept
        {
            return m_hasTrailer;
        }

        /**
         * @brief  读取指定帧。
         * @param  frameIndex 帧号，从 0 开始。
         * @param  out 输出帧数据，不可为 nullptr。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool readFrame(std::uint64_t frameIndex, FrameState* out, QString* errorMessage);

        /**
         * @brief  读取并返回指定帧中指定槽位的刚体记录。
         * @param  frameIndex 帧号。
         * @param  slot 槽位。
         * @param  out 输出刚体记录，不可为 nullptr。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool readBody(std::uint64_t frameIndex,
                                    std::uint32_t slot,
                                    BodyState* out,
                                    QString* errorMessage);

    private:
        /** 帧索引中的单条记录。 */
        struct IndexEntry
        {
            /** 帧块起始偏移。 */
            std::uint64_t frameOffset{0};

            /** 帧块总长度。 */
            std::uint32_t chunkLength{0};

            /** 帧块载荷字节数。 */
            std::uint32_t payloadLength{0};
        };

        /**
         * @brief  读取并校验一个数据块的载荷。
         * @param  chunkOffset 块起始偏移。
         * @param  expectedTag 期望的块类型。
         * @param  payload 输出载荷。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool readChunkPayload(std::uint64_t chunkOffset,
                                            const FourCC& expectedTag,
                                            std::vector<std::byte>* payload,
                                            QString* errorMessage);

        /**
         * @brief  解析文件头。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool parseFileHeader(QString* errorMessage);

        /**
         * @brief  解析场景描述块（JSON 文本与元信息）。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool parseSceneChunk(QString* errorMessage);

        /**
         * @brief  读取帧索引；缺失时退化为顺序扫描帧块。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool parseFrameIndex(QString* errorMessage);

        /**
         * @brief  在没有索引块的情况下，从场景块之后顺序扫描出所有帧块。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 至少扫描到一个帧块返回 true。
         */
        [[nodiscard]] bool scanFrameChunks(QString* errorMessage);

        /**
         * @brief  解析帧载荷。
         * @param  payload 帧载荷。
         * @param  out 输出帧数据。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool parseFramePayload(const std::vector<std::byte>& payload,
                                             FrameState* out,
                                             QString* errorMessage) const;

        /** 目标文件路径。 */
        QString m_filePath{};

        /** 文件句柄。 */
        QFile m_file{};

        /** 场景描述块 JSON 原文。 */
        std::string m_sceneJson{};

        /** 场景元信息。 */
        SceneMetadata m_metadata{};

        /** 帧索引。 */
        std::vector<IndexEntry> m_index{};

        /** 文件头中的采样间隔。 */
        std::uint64_t m_sampleIntervalNs{kDefaultSampleIntervalNs};

        /** 文件头中的刚体数量。 */
        std::uint32_t m_bodyCount{0};

        /** 场景描述块起始偏移。 */
        std::uint64_t m_sceneChunkOffset{0};

        /** 索引块起始偏移。 */
        std::uint64_t m_indexChunkOffset{0};

        /** 帧块定长长度；0 表示长度可变。 */
        std::uint32_t m_frameStrideBytes{0};

        /** 是否存在尾块。 */
        bool m_hasTrailer{false};

        /** 是否已打开。 */
        bool m_open{false};
    };
}

#endif // EGRET_PHYSICS_EPS_READER_H
