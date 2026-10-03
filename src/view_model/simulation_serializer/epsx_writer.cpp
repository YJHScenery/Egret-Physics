/**
 * @file        epsx_writer.cpp
 * @brief       EPSX 文件流式写入器实现。
 * @details     实现 epsx_writer.h 中声明的写入流程：文件头、场景描述块、
 *              逐帧数据块、索引块与尾块，以及文件头的回填。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Serialize
 * @defgroup    EPSX (Egret Physics Simulation 文件格式)
 */

#include "epsx_writer.h"

#include <QFileInfo>
#include <QDir>

#include <limits>

namespace egret::epsx
{
    namespace
    {
        /** 把 QString 写入调用方提供的错误指针（为空则忽略）。 */
        void setError(QString* errorMessage, const QString& message)
        {
            if (errorMessage != nullptr) {
                *errorMessage = message;
            }
        }

        /**
         * @brief  把刚体记录序列化为连续字节。
         * @param  bodies 刚体记录列表。
         * @return 序列化结果，长度等于 bodies.size() * kBodyStrideFloat64。
         */
        std::vector<std::byte> serializeBodies(const std::vector<BodyState>& bodies)
        {
            std::vector<std::byte> payload;
            payload.reserve(bodies.size() * kBodyStrideFloat64);

            ByteWriter writer(payload);
            for (const BodyState& body : bodies) {
                writer.writeU64(body.id);
                writer.writeU32(body.slot);
                writer.writeU32(body.stateFlags);
                writer.writeF64(body.position.x());
                writer.writeF64(body.position.y());
                writer.writeF64(body.position.z());
                writer.writeF64(body.rotation.x());
                writer.writeF64(body.rotation.y());
                writer.writeF64(body.rotation.z());
                writer.writeF64(body.rotation.w());
                writer.writeF64(body.linearVelocity.x());
                writer.writeF64(body.linearVelocity.y());
                writer.writeF64(body.linearVelocity.z());
                writer.writeF64(body.angularVelocity.x());
                writer.writeF64(body.angularVelocity.y());
                writer.writeF64(body.angularVelocity.z());
                writer.writeF64(body.mass);
                writer.writeF64(body.restitution);
                writer.writeU64(0ull);
            }
            return payload;
        }

        /**
         * @brief  把约束记录序列化为连续字节。
         * @param  constraints 约束记录列表。
         * @return 序列化结果，长度等于 constraints.size() * kConstraintStrideFloat64。
         */
        std::vector<std::byte> serializeConstraints(const std::vector<ConstraintState>& constraints)
        {
            std::vector<std::byte> payload;
            payload.reserve(constraints.size() * kConstraintStrideFloat64);

            ByteWriter writer(payload);
            for (const ConstraintState& constraint : constraints) {
                writer.writeU64(constraint.id);
                writer.writeU32(constraint.slot);
                writer.writeU32(constraint.type);
                writer.writeU32(constraint.flags);
                writer.writeU32(0u);
                writer.writeU64(constraint.entityAId);
                writer.writeU64(constraint.entityBId);
                writer.writeF64(constraint.anchorAWorld.x());
                writer.writeF64(constraint.anchorAWorld.y());
                writer.writeF64(constraint.anchorAWorld.z());
                writer.writeF64(constraint.anchorBWorld.x());
                writer.writeF64(constraint.anchorBWorld.y());
                writer.writeF64(constraint.anchorBWorld.z());
                writer.writeF64(constraint.restLength);
                writer.writeF64(constraint.currentLength);
                writer.writeF64(constraint.error);
                writer.writeF64(constraint.forceMagnitude);
                writer.writeU64(0ull);
            }
            return payload;
        }

        /**
         * @brief  把力场记录序列化为连续字节。
         * @param  fields 力场记录列表。
         * @return 序列化结果，长度等于 fields.size() * kFieldStrideFloat64。
         */
        std::vector<std::byte> serializeFields(const std::vector<FieldState>& fields)
        {
            std::vector<std::byte> payload;
            payload.reserve(fields.size() * kFieldStrideFloat64);

            ByteWriter writer(payload);
            for (const FieldState& field : fields) {
                writer.writeU64(field.id);
                writer.writeU32(field.slot);
                writer.writeU32(field.type);
                writer.writeU32(field.flags);
                writer.writeU32(0u);
                writer.writeF64(field.vector.x());
                writer.writeF64(field.vector.y());
                writer.writeF64(field.vector.z());
                writer.writeF64(field.center.x());
                writer.writeF64(field.center.y());
                writer.writeF64(field.center.z());
                writer.writeF64(field.strength);
                writer.writeU64(0ull);
            }
            return payload;
        }

        /**
         * @brief  向帧载荷追加一个帧段（32 字节段头 + 元素数组）。
         * @param  framePayload 目标帧载荷缓冲区。
         * @param  tag 段类型标记。
         * @param  strideBytes 单元素字节数。
         * @param  sectionPayload 已序列化的元素数组。
         */
        void appendSection(std::vector<std::byte>& framePayload,
                           const FourCC& tag,
                           const std::uint32_t strideBytes,
                           const std::vector<std::byte>& sectionPayload)
        {
            const std::uint32_t elementCount =
                strideBytes == 0 ? 0u : static_cast<std::uint32_t>(sectionPayload.size() / strideBytes);

            ByteWriter writer(framePayload);
            writer.writeFourCC(tag);
            writer.writeU32(elementCount);
            writer.writeU32(strideBytes);
            writer.writeU32(0u);
            writer.writeU64(static_cast<std::uint64_t>(sectionPayload.size()));
            writer.writeU32(crc32c(sectionPayload.data(), sectionPayload.size()));
            writer.writeU32(1u);
            writer.writeBytes(sectionPayload.data(), sectionPayload.size());
        }
    }

    EpsxWriter::EpsxWriter(QString filePath) :
        m_filePath(std::move(filePath))
    {
    }

    EpsxWriter::~EpsxWriter()
    {
        if (m_open) {
            m_file.close();
            m_open = false;
        }
    }

    bool EpsxWriter::open(const SceneInfo& info, QString* errorMessage)
    {
        if (m_open) {
            setError(errorMessage, QStringLiteral("EPSX 写入器已经处于打开状态"));
            return false;
        }

        if (info.solverStepNs == 0 || info.sampleIntervalNs == 0) {
            setError(errorMessage, QStringLiteral("求解步长与采样间隔都必须大于 0"));
            return false;
        }

        if (!decimationFor(info.solverStepNs, info.sampleIntervalNs).has_value()) {
            setError(errorMessage, QStringLiteral("采样间隔必须是求解步长的整数倍"));
            return false;
        }

        if (info.sampleIntervalNs > std::numeric_limits<std::uint32_t>::max()) {
            setError(errorMessage, QStringLiteral("采样间隔超出文件头字段容量"));
            return false;
        }

        // 确保输出目录存在，否则 QFile::open 只会给出一个含糊的失败。
        const QFileInfo fileInfo(m_filePath);
        const QDir parentDir = fileInfo.absoluteDir();
        if (!parentDir.exists() && !QDir().mkpath(parentDir.absolutePath())) {
            setError(errorMessage, QStringLiteral("无法创建输出目录：%1").arg(parentDir.absolutePath()));
            return false;
        }

        m_file.setFileName(m_filePath);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setError(errorMessage, QStringLiteral("无法打开输出文件：%1（%2）")
                                      .arg(m_filePath, m_file.errorString()));
            return false;
        }

        m_info = info;
        m_index.clear();
        m_frameCount = 0;
        m_frameStrideBytes = 0;
        m_sceneChunkOffset = 0;
        m_indexChunkOffset = 0;

        const std::vector<std::byte> header = buildFileHeader();
        if (!writeRaw(header, errorMessage)) {
            m_file.close();
            return false;
        }

        const std::vector<std::byte> scenePayload = buildSceneChunkPayload();
        if (!writeChunk(kTagScene, scenePayload, &m_sceneChunkOffset, nullptr, errorMessage)) {
            m_file.close();
            return false;
        }

        m_open = true;
        return true;
    }

    bool EpsxWriter::writeFrame(const FrameState& frame, QString* errorMessage)
    {
        if (!m_open) {
            setError(errorMessage, QStringLiteral("EPSX 写入器尚未打开"));
            return false;
        }

        if (frame.bodies.size() != m_info.bodyCount ||
            frame.constraints.size() != m_info.constraintCount ||
            frame.fields.size() != m_info.fieldCount) {
            setError(errorMessage,
                     QStringLiteral("第 %1 帧的元素数量与场景声明不一致（刚体 %2/%3，约束 %4/%5，场 %6/%7）")
                         .arg(frame.frameIndex)
                         .arg(frame.bodies.size())
                         .arg(m_info.bodyCount)
                         .arg(frame.constraints.size())
                         .arg(m_info.constraintCount)
                         .arg(frame.fields.size())
                         .arg(m_info.fieldCount));
            return false;
        }

        const std::vector<std::byte> payload = buildFramePayload(frame);

        std::uint64_t chunkOffset{0};
        std::uint32_t chunkLength{0};
        if (!writeChunk(kTagFrame, payload, &chunkOffset, &chunkLength, errorMessage)) {
            return false;
        }

        // 首帧确定定长长度；后续帧长度若不同则退化为变长（依赖索引块定位）。
        if (m_index.empty()) {
            m_frameStrideBytes = chunkLength;
        }
        else if (m_frameStrideBytes != chunkLength) {
            m_frameStrideBytes = 0;
        }

        m_index.push_back(IndexEntry{chunkOffset, chunkLength, static_cast<std::uint32_t>(payload.size())});
        m_frameCount = static_cast<std::uint64_t>(m_index.size());
        return true;
    }

    bool EpsxWriter::close(QString* errorMessage)
    {
        if (!m_open) {
            setError(errorMessage, QStringLiteral("EPSX 写入器尚未打开或已经收尾"));
            return false;
        }

        // 1. 索引块
        std::vector<std::byte> indexPayload;
        indexPayload.reserve(m_index.size() * kIndexEntryBytes);
        {
            ByteWriter writer(indexPayload);
            for (const IndexEntry& entry : m_index) {
                writer.writeU64(entry.frameOffset);
                writer.writeU32(entry.chunkLength);
                writer.writeU32(entry.payloadLength);
            }
        }

        if (!writeChunk(kTagIndex, indexPayload, &m_indexChunkOffset, nullptr, errorMessage)) {
            m_file.close();
            m_open = false;
            return false;
        }

        // 2. 尾块：总帧数、索引位置、文件长度
        std::vector<std::byte> endPayload;
        {
            ByteWriter writer(endPayload);
            writer.writeU64(m_frameCount);
            writer.writeU64(m_indexChunkOffset);
            writer.writeU32(static_cast<std::uint32_t>(m_file.pos() & 0xFFFFFFFFu));
            writer.writeU32(0u);
        }

        if (!writeChunk(kTagEnd, endPayload, nullptr, nullptr, errorMessage)) {
            m_file.close();
            m_open = false;
            return false;
        }

        // 3. 回填文件头
        const bool patched = patchFileHeader(errorMessage);
        if (patched) {
            m_file.flush();
        }
        m_file.close();
        m_open = false;
        return patched;
    }

    std::vector<std::byte> EpsxWriter::buildFileHeader() const
    {
        std::vector<std::byte> header;
        header.reserve(kFileHeaderBytes);

        ByteWriter writer(header);
        writer.writeBytes(kFileMagic.data(), kFileMagic.size());
        writer.writeU16(kVersionMajor);
        writer.writeU16(kVersionMinor);
        writer.writeU32(static_cast<std::uint32_t>(FileFlags::LittleEndian));
        writer.writeU64(m_frameCount);

        const std::uint64_t durationNs =
            m_info.durationNs != 0
                ? m_info.durationNs
                : m_frameCount * m_info.sampleIntervalNs;
        writer.writeU64(durationNs);
        writer.writeU32(static_cast<std::uint32_t>(m_info.sampleIntervalNs));
        writer.writeU32(m_info.bodyCount);
        writer.writeU64(m_sceneChunkOffset);
        writer.writeU64(m_indexChunkOffset);
        writer.writeU32(static_cast<std::uint32_t>(m_frameStrideBytes));

        // 校验和覆盖前 60 字节，字段本身不参与。
        const std::uint32_t headerCrc = crc32c(header.data(), header.size());
        writer.writeU32(headerCrc);
        return header;
    }

    std::vector<std::byte> EpsxWriter::buildSceneChunkPayload() const
    {
        std::vector<std::byte> payload;
        payload.reserve(m_info.sceneJson.size());
        payload.insert(payload.end(),
                       reinterpret_cast<const std::byte*>(m_info.sceneJson.data()),
                       reinterpret_cast<const std::byte*>(m_info.sceneJson.data()) + m_info.sceneJson.size());
        return payload;
    }

    std::vector<std::byte> EpsxWriter::buildFramePayload(const FrameState& frame) const
    {
        std::vector<std::byte> payload;
        payload.reserve(static_cast<std::size_t>(framePayloadBytes(frame.bodies.size(),
                                                                  frame.constraints.size(),
                                                                  frame.fields.size())));

        ByteWriter writer(payload);

        // ---- 帧头（24 字节）----
        writer.writeU32(frame.frameIndex);
        writer.writeU32(0u); // frameFlags 预留
        writer.writeU64(frame.simTimeNs);
        writer.writeU64(frame.stepIndex);

        // ---- 各帧段：先序列化为独立缓冲区，再据此计算段校验和 ----
        if (!frame.bodies.empty()) {
            appendSection(payload, kSectionBody, kBodyStrideFloat64, serializeBodies(frame.bodies));
        }
        if (!frame.constraints.empty()) {
            appendSection(payload, kSectionConstraint, kConstraintStrideFloat64,
                          serializeConstraints(frame.constraints));
        }
        if (!frame.fields.empty()) {
            appendSection(payload, kSectionField, kFieldStrideFloat64, serializeFields(frame.fields));
        }

        return payload;
    }

    bool EpsxWriter::writeChunk(const FourCC& tag,
                               const std::vector<std::byte>& payload,
                               std::uint64_t* chunkOffset,
                               std::uint32_t* chunkLength,
                               QString* errorMessage)
    {
        const std::size_t padding = paddingFor(payload.size());
        const std::uint64_t offset = static_cast<std::uint64_t>(m_file.pos());

        const std::uint64_t totalLength = static_cast<std::uint64_t>(kChunkHeaderBytes) +
                                          static_cast<std::uint64_t>(payload.size()) +
                                          static_cast<std::uint64_t>(padding);
        if (totalLength > std::numeric_limits<std::uint32_t>::max()) {
            setError(errorMessage, QStringLiteral("数据块过大，超出 32 位长度字段容量"));
            return false;
        }

        std::vector<std::byte> chunk;
        chunk.reserve(static_cast<std::size_t>(totalLength));
        {
            ByteWriter writer(chunk);
            writer.writeFourCC(tag);
            writer.writeU32(0u); // chunkFlags 预留
            writer.writeU64(static_cast<std::uint64_t>(payload.size()));
            writer.writeU32(crc32c(payload.data(), payload.size()));
            writer.writeU32(0u); // reserved
            writer.writeBytes(payload.data(), payload.size());
            writer.writeZeros(padding);
        }

        if (!writeRaw(chunk, errorMessage)) {
            return false;
        }

        if (chunkOffset != nullptr) {
            *chunkOffset = offset;
        }
        if (chunkLength != nullptr) {
            *chunkLength = static_cast<std::uint32_t>(totalLength);
        }
        return true;
    }

    bool EpsxWriter::writeRaw(const std::vector<std::byte>& bytes, QString* errorMessage)
    {
        if (bytes.empty()) {
            return true;
        }

        const qint64 written = m_file.write(reinterpret_cast<const char*>(bytes.data()),
                                            static_cast<qint64>(bytes.size()));
        if (written != static_cast<qint64>(bytes.size())) {
            setError(errorMessage, QStringLiteral("写入文件失败：%1（%2）").arg(m_filePath, m_file.errorString()));
            return false;
        }
        return true;
    }

    bool EpsxWriter::patchFileHeader(QString* errorMessage)
    {
        const std::vector<std::byte> header = buildFileHeader();
        if (header.size() != kFileHeaderBytes) {
            setError(errorMessage, QStringLiteral("内部错误：文件头长度不是 64 字节"));
            return false;
        }

        if (!m_file.seek(0)) {
            setError(errorMessage, QStringLiteral("无法定位到文件头以回填：%1").arg(m_file.errorString()));
            return false;
        }

        if (!writeRaw(header, errorMessage)) {
            return false;
        }

        // 回填后把写入位置移回文件末尾，保持后续写入语义一致。
        return m_file.seek(m_file.size());
    }

    std::size_t EpsxWriter::paddingFor(const std::size_t payloadLength) noexcept
    {
        const std::size_t remainder = payloadLength % kAlignment;
        return remainder == 0 ? 0 : kAlignment - remainder;
    }
}
