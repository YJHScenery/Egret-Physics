/**
 * @file        eps_writer.h
 * @brief       EPS 文件流式写入器。
 * @details     EpsWriter 按 docs/EPS 模拟数据文件格式规范（草案）.md 定义的布局，
 *              把离线求解结果逐帧写入磁盘：
 *
 *              1. open()  写 64 字节文件头与 SCNE 场景描述块；
 *              2. writeFrame()  追加一个 FRAM 帧块（帧头 + BODY / CONS / FLDS 段）；
 *              3. close()  追加 INDX 索引块与 END! 尾块，再回填文件头字段并重算文件头校验和。
 *
 *              流式写入意味着内存中不必保留整段历史：中途失败时已写入的完整帧仍可被读取，
 *              只是缺少尾块，读取端会据此给出「文件可能不完整」的提示。
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

#ifndef EGRET_PHYSICS_EPS_WRITER_H
#define EGRET_PHYSICS_EPS_WRITER_H

#include <QFile>
#include <QString>

#include <cstdint>
#include <string>
#include <vector>

#include "eps_format.h"

namespace egret::eps
{
    /**
     * @brief       流式写出的 EPS 文件写入器。
     * @details     使用方式固定为 open → writeFrame×N → close。
     *              未调用 close() 就析构时不会补写索引与尾块，文件处于「不完整」状态。
     *
     * @invariant   m_open 为 true 时 m_file 处于已打开状态
     * @invariant   m_index.size() 等于已写入的帧数
     */
    class EpsWriter final
    {
    public:
        /**
         * @brief 描述一次模拟会话的静态信息。
         * @details 这些信息在文件头或场景描述块中只写一次，帧循环期间不再变化。
         */
        struct SceneInfo
        {
            /** 编辑器导出的场景 JSON（UTF-8），供播放器重建场景。 */
            std::string sceneJson{};

            /** 生成者描述，例如应用名 + 版本 + 提交号。 */
            std::string generator{};

            /** 求解推进步长（纳秒）。 */
            std::uint64_t solverStepNs{kDefaultSolverStepNs};

            /** 采样间隔（纳秒）。 */
            std::uint64_t sampleIntervalNs{kDefaultSampleIntervalNs};

            /** 声明的模拟总时长（纳秒）；未知时填 0。 */
            std::uint64_t durationNs{0};

            /** 刚体数量（等于每帧 BODY 段元素数）。 */
            std::uint32_t bodyCount{0};

            /** 约束数量（等于每帧 CONS 段元素数）。 */
            std::uint32_t constraintCount{0};

            /** 力场数量（等于每帧 FLDS 段元素数）。 */
            std::uint32_t fieldCount{0};
        };

        /**
         * @brief 绑定输出文件路径。
         * @param filePath 目标文件路径（含扩展名）。
         */
        explicit EpsWriter(QString filePath);

        /** 析构时若文件仍处于打开状态，直接关闭而不补写索引。 */
        ~EpsWriter();

        EpsWriter(const EpsWriter&) = delete;

        EpsWriter& operator=(const EpsWriter&) = delete;

        EpsWriter(EpsWriter&&) = delete;

        EpsWriter& operator=(EpsWriter&&) = delete;

        /**
         * @brief  开始写入：创建文件、写文件头与场景描述块。
         * @param  info 本次模拟的静态信息。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool open(const SceneInfo& info, QString* errorMessage);

        /**
         * @brief  追加一帧。
         * @param  frame 帧数据；各段元素数必须与 open() 时声明的数量一致。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool writeFrame(const FrameState& frame, QString* errorMessage);

        /**
         * @brief  收尾：写索引块与尾块，回填文件头并关闭文件。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool close(QString* errorMessage);

        /** 是否已成功打开且尚未收尾。 */
        [[nodiscard]] bool isOpen() const noexcept
        {
            return m_open;
        }

        /** 已写入的帧数。 */
        [[nodiscard]] std::uint64_t frameCount() const noexcept
        {
            return static_cast<std::uint64_t>(m_index.size());
        }

        /** 目标文件路径。 */
        [[nodiscard]] const QString& filePath() const noexcept
        {
            return m_filePath;
        }

    private:
        /** 索引块中的单条记录。 */
        struct IndexEntry
        {
            /** 帧块起始偏移。 */
            std::uint64_t frameOffset{0};

            /** 帧块总长度（含块头、载荷与填充）。 */
            std::uint32_t chunkLength{0};

            /** 帧块载荷字节数。 */
            std::uint32_t payloadLength{0};
        };

        /**
         * @brief  构造 64 字节文件头。
         * @return 文件头字节序列。
         */
        [[nodiscard]] std::vector<std::byte> buildFileHeader() const;

        /**
         * @brief  构造帧载荷（帧头 + 各段）。
         * @param  frame 帧数据。
         * @return 载荷字节序列。
         */
        [[nodiscard]] std::vector<std::byte> buildFramePayload(const FrameState& frame) const;

        /**
         * @brief  构造场景描述块载荷（UTF-8 JSON）。
         * @return 载荷字节序列。
         */
        [[nodiscard]] std::vector<std::byte> buildSceneChunkPayload() const;

        /**
         * @brief  写出一个完整数据块（块头 + 载荷 + 填充）。
         * @param  tag 块类型。
         * @param  payload 载荷。
         * @param  chunkOffset 输出该块的起始偏移，可为 nullptr。
         * @param  chunkLength 输出该块总长度，可为 nullptr。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool writeChunk(const FourCC& tag,
                                      const std::vector<std::byte>& payload,
                                      std::uint64_t* chunkOffset,
                                      std::uint32_t* chunkLength,
                                      QString* errorMessage);

        /**
         * @brief  把字节序列写到文件当前位置。
         * @param  bytes 字节序列。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool writeRaw(const std::vector<std::byte>& bytes, QString* errorMessage);

        /**
         * @brief  回填文件头字段并重算文件头校验和。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool patchFileHeader(QString* errorMessage);

        /** 计算 payload 之后的对齐填充字节数。 */
        [[nodiscard]] static std::size_t paddingFor(const std::size_t payloadLength) noexcept;

        /** 目标文件路径。 */
        QString m_filePath{};

        /** 输出文件句柄。 */
        QFile m_file{};

        /** 本次模拟的静态信息。 */
        SceneInfo m_info{};

        /** 帧索引表。 */
        std::vector<IndexEntry> m_index{};

        /** 文件头中的帧块定长长度；为 0 表示长度可变。 */
        std::uint64_t m_frameStrideBytes{0};

        /** 场景描述块起始偏移。 */
        std::uint64_t m_sceneChunkOffset{0};

        /** 索引块起始偏移。 */
        std::uint64_t m_indexChunkOffset{0};

        /** 已写入的帧数。 */
        std::uint64_t m_frameCount{0};

        /** 文件是否处于已打开状态。 */
        bool m_open{false};
    };
}

#endif // EGRET_PHYSICS_EPS_WRITER_H
