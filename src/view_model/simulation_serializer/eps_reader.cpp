/**
 * @file        eps_reader.cpp
 * @brief       EPS 文件随机访问读取器实现。
 * @details     实现文件头校验、场景描述块解析、帧索引读取（含缺失索引时的顺序扫描回退），
 *              以及帧载荷到 FrameState 的解码。
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

#include "eps_reader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <tuple>

namespace egret::eps
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

        /** 把 FourCC 渲染成便于报错的文本。 */
        QString describeTag(const FourCC& tag)
        {
            return QString::fromLatin1(tag.data(), static_cast<int>(tag.size()));
        }

        /**
         * @brief  解析 BODY 段的元素数组。
         * @param  data 段载荷起始地址。
         * @param  bytes 段载荷字节数。
         * @param  count 元素个数。
         * @param  stride 单元素字节数。
         * @param  out 输出刚体记录列表。
         * @param  errorMessage 失败时写入错误描述。
         * @return 成功返回 true。
         */
        bool parseBodySection(const std::uint8_t* data,
                              const std::size_t bytes,
                              const std::uint32_t count,
                              const std::uint32_t stride,
                              std::vector<BodyState>* out,
                              QString* errorMessage)
        {
            if (stride != kBodyStrideFloat64) {
                setError(errorMessage, QStringLiteral("BODY 段步长 %1 不受支持（当前仅支持 %2）")
                                           .arg(stride)
                                           .arg(kBodyStrideFloat64));
                return false;
            }
            if (static_cast<std::uint64_t>(count) * stride > bytes) {
                setError(errorMessage, QStringLiteral("BODY 段长度与元素个数不一致"));
                return false;
            }

            out->clear();
            out->reserve(count);

            ByteReader reader(data, bytes);
            for (std::uint32_t i = 0; i < count; ++i) {
                BodyState body{};
                body.id = reader.readU64();
                body.slot = reader.readU32();
                body.stateFlags = reader.readU32();
                body.position.x() = reader.readF64();
                body.position.y() = reader.readF64();
                body.position.z() = reader.readF64();
                body.rotation.x() = reader.readF64();
                body.rotation.y() = reader.readF64();
                body.rotation.z() = reader.readF64();
                body.rotation.w() = reader.readF64();
                body.linearVelocity.x() = reader.readF64();
                body.linearVelocity.y() = reader.readF64();
                body.linearVelocity.z() = reader.readF64();
                body.angularVelocity.x() = reader.readF64();
                body.angularVelocity.y() = reader.readF64();
                body.angularVelocity.z() = reader.readF64();
                body.mass = reader.readF64();
                body.restitution = reader.readF64();
                std::ignore = reader.readU64(); // reserved

                if (!reader.ok()) {
                    setError(errorMessage, QStringLiteral("BODY 段第 %1 个元素解码越界").arg(i));
                    return false;
                }
                out->push_back(body);
            }
            return true;
        }

        /**
         * @brief  解析 CONS 段的元素数组。
         * @param  data 段载荷起始地址。
         * @param  bytes 段载荷字节数。
         * @param  count 元素个数。
         * @param  stride 单元素字节数。
         * @param  out 输出约束记录列表。
         * @param  errorMessage 失败时写入错误描述。
         * @return 成功返回 true。
         */
        bool parseConstraintSection(const std::uint8_t* data,
                                    const std::size_t bytes,
                                    const std::uint32_t count,
                                    const std::uint32_t stride,
                                    std::vector<ConstraintState>* out,
                                    QString* errorMessage)
        {
            if (stride != kConstraintStrideFloat64) {
                setError(errorMessage, QStringLiteral("CONS 段步长 %1 不受支持（当前仅支持 %2）")
                                           .arg(stride)
                                           .arg(kConstraintStrideFloat64));
                return false;
            }
            if (static_cast<std::uint64_t>(count) * stride > bytes) {
                setError(errorMessage, QStringLiteral("CONS 段长度与元素个数不一致"));
                return false;
            }

            out->clear();
            out->reserve(count);

            ByteReader reader(data, bytes);
            for (std::uint32_t i = 0; i < count; ++i) {
                ConstraintState constraint{};
                constraint.id = reader.readU64();
                constraint.slot = reader.readU32();
                constraint.type = reader.readU32();
                constraint.flags = reader.readU32();
                std::ignore = reader.readU32(); // reserved
                constraint.entityAId = reader.readU64();
                constraint.entityBId = reader.readU64();
                constraint.anchorAWorld.x() = reader.readF64();
                constraint.anchorAWorld.y() = reader.readF64();
                constraint.anchorAWorld.z() = reader.readF64();
                constraint.anchorBWorld.x() = reader.readF64();
                constraint.anchorBWorld.y() = reader.readF64();
                constraint.anchorBWorld.z() = reader.readF64();
                constraint.restLength = reader.readF64();
                constraint.currentLength = reader.readF64();
                constraint.error = reader.readF64();
                constraint.forceMagnitude = reader.readF64();
                std::ignore = reader.readU64(); // reserved

                if (!reader.ok()) {
                    setError(errorMessage, QStringLiteral("CONS 段第 %1 个元素解码越界").arg(i));
                    return false;
                }
                out->push_back(constraint);
            }
            return true;
        }

        /**
         * @brief  解析 FLDS 段的元素数组。
         * @param  data 段载荷起始地址。
         * @param  bytes 段载荷字节数。
         * @param  count 元素个数。
         * @param  stride 单元素字节数。
         * @param  out 输出力场记录列表。
         * @param  errorMessage 失败时写入错误描述。
         * @return 成功返回 true。
         */
        bool parseFieldSection(const std::uint8_t* data,
                               const std::size_t bytes,
                               const std::uint32_t count,
                               const std::uint32_t stride,
                               std::vector<FieldState>* out,
                               QString* errorMessage)
        {
            if (stride != kFieldStrideFloat64) {
                setError(errorMessage, QStringLiteral("FLDS 段步长 %1 不受支持（当前仅支持 %2）")
                                           .arg(stride)
                                           .arg(kFieldStrideFloat64));
                return false;
            }
            if (static_cast<std::uint64_t>(count) * stride > bytes) {
                setError(errorMessage, QStringLiteral("FLDS 段长度与元素个数不一致"));
                return false;
            }

            out->clear();
            out->reserve(count);

            ByteReader reader(data, bytes);
            for (std::uint32_t i = 0; i < count; ++i) {
                FieldState field{};
                field.id = reader.readU64();
                field.slot = reader.readU32();
                field.type = reader.readU32();
                field.flags = reader.readU32();
                std::ignore = reader.readU32(); // reserved
                field.vector.x() = reader.readF64();
                field.vector.y() = reader.readF64();
                field.vector.z() = reader.readF64();
                field.center.x() = reader.readF64();
                field.center.y() = reader.readF64();
                field.center.z() = reader.readF64();
                field.strength = reader.readF64();
                std::ignore = reader.readU64(); // reserved

                if (!reader.ok()) {
                    setError(errorMessage, QStringLiteral("FLDS 段第 %1 个元素解码越界").arg(i));
                    return false;
                }
                out->push_back(field);
            }
            return true;
        }
    }

    EpsReader::EpsReader(QString filePath) :
        m_filePath(std::move(filePath))
    {
    }

    EpsReader::~EpsReader()
    {
        close();
    }

    bool EpsReader::open(QString* errorMessage)
    {
        if (m_open) {
            setError(errorMessage, QStringLiteral("EPS 读取器已经打开"));
            return false;
        }

        m_file.setFileName(m_filePath);
        if (!m_file.open(QIODevice::ReadOnly)) {
            setError(errorMessage, QStringLiteral("无法打开文件：%1（%2）").arg(m_filePath, m_file.errorString()));
            return false;
        }

        if (!parseFileHeader(errorMessage) ||
            !parseSceneChunk(errorMessage) ||
            !parseFrameIndex(errorMessage)) {
            m_file.close();
            return false;
        }

        m_open = true;
        return true;
    }

    void EpsReader::close()
    {
        if (m_file.isOpen()) {
            m_file.close();
        }
        m_open = false;
    }

    bool EpsReader::parseFileHeader(QString* errorMessage)
    {
        if (m_file.size() < static_cast<qint64>(kFileHeaderBytes)) {
            setError(errorMessage, QStringLiteral("文件长度不足 %1 字节，不是有效的 EPS 文件")
                                       .arg(kFileHeaderBytes));
            return false;
        }

        if (!m_file.seek(0)) {
            setError(errorMessage, QStringLiteral("无法定位到文件头"));
            return false;
        }

        const QByteArray raw = m_file.read(static_cast<qint64>(kFileHeaderBytes));
        if (raw.size() != static_cast<qsizetype>(kFileHeaderBytes)) {
            setError(errorMessage, QStringLiteral("读取文件头失败"));
            return false;
        }

        const auto* bytes = reinterpret_cast<const std::uint8_t*>(raw.constData());

        if (std::memcmp(bytes, kFileMagic.data(), kFileMagic.size()) != 0) {
            setError(errorMessage,
                     QStringLiteral("文件魔数不匹配，这不是 Egret Physics 模拟文件（扩展名可能被其他格式占用）"));
            return false;
        }

        ByteReader reader(bytes, kFileHeaderBytes);
        reader.skip(kFileMagic.size());
        const std::uint16_t versionMajor = reader.readU16();
        const std::uint16_t versionMinor = reader.readU16();
        const std::uint32_t flags = reader.readU32();
        const std::uint64_t frameCount = reader.readU64();
        std::ignore = reader.readU64();  // durationNs
        const std::uint32_t sampleIntervalNs = reader.readU32();
        m_bodyCount = reader.readU32();
        m_sceneChunkOffset = reader.readU64();
        m_indexChunkOffset = reader.readU64();
        m_frameStrideBytes = reader.readU32();
        const std::uint32_t headerCrc = reader.readU32();

        if (!reader.ok()) {
            setError(errorMessage, QStringLiteral("文件头字段解析越界"));
            return false;
        }

        const std::uint32_t expectedCrc = crc32c(bytes, kFileHeaderBytes - sizeof(std::uint32_t));
        if (headerCrc != expectedCrc) {
            setError(errorMessage, QStringLiteral("文件头校验失败（期望 0x%1，实际 0x%2）")
                                       .arg(expectedCrc, 8, 16, QLatin1Char('0'))
                                       .arg(headerCrc, 8, 16, QLatin1Char('0')));
            return false;
        }

        if (versionMajor != kVersionMajor) {
            setError(errorMessage, QStringLiteral("不支持的 EPS 主版本号 %1（当前支持 %2）")
                                       .arg(versionMajor)
                                       .arg(kVersionMajor));
            return false;
        }

        if (hasFlag(flags, FileFlags::Float32Geometry)) {
            setError(errorMessage, QStringLiteral("该文件使用 float32 几何精度，当前版本尚未支持"));
            return false;
        }
        if (hasFlag(flags, FileFlags::ZstdFrames)) {
            setError(errorMessage, QStringLiteral("该文件使用 zstd 压缩帧，当前版本尚未支持"));
            return false;
        }

        m_sampleIntervalNs = sampleIntervalNs;
        m_metadata.sampleIntervalNs = sampleIntervalNs;
        m_metadata.declaredFrameCount = frameCount;
        m_metadata.generator = std::string{};
        m_metadata.solverStepNs = kDefaultSolverStepNs;
        m_metadata.decimation = sampleIntervalNs == 0
                                    ? 1ull
                                    : std::max(1ull, sampleIntervalNs / kDefaultSolverStepNs);

        if (versionMinor != kVersionMinor) {
            // 次版本差异允许继续读取：新版本只会在保留字段内追加信息。
        }
        return true;
    }

    bool EpsReader::parseSceneChunk(QString* errorMessage)
    {
        std::vector<std::byte> payload;
        if (!readChunkPayload(m_sceneChunkOffset, kTagScene, &payload, errorMessage)) {
            return false;
        }

        m_sceneJson.assign(reinterpret_cast<const char*>(payload.data()), payload.size());

        // 解析元信息；解析失败不视为致命错误，只是无法提供可选字段。
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(m_sceneJson));
        if (!doc.isObject()) {
            return true;
        }

        const QJsonObject root = doc.object();
        const QJsonObject sampling = root.value(QStringLiteral("sampling")).toObject();

        if (sampling.contains(QStringLiteral("solverStepNs"))) {
            m_metadata.solverStepNs = static_cast<std::uint64_t>(
                sampling.value(QStringLiteral("solverStepNs")).toDouble(kDefaultSolverStepNs));
        }
        if (sampling.contains(QStringLiteral("intervalNs"))) {
            m_metadata.sampleIntervalNs = static_cast<std::uint64_t>(
                sampling.value(QStringLiteral("intervalNs")).toDouble(m_metadata.sampleIntervalNs));
        }
        if (sampling.contains(QStringLiteral("decimation"))) {
            m_metadata.decimation = static_cast<std::uint64_t>(
                sampling.value(QStringLiteral("decimation")).toDouble(1.0));
        }
        if (sampling.contains(QStringLiteral("frameCount"))) {
            m_metadata.declaredFrameCount = static_cast<std::uint64_t>(
                sampling.value(QStringLiteral("frameCount")).toDouble(0.0));
        }
        if (root.contains(QStringLiteral("generator"))) {
            m_metadata.generator = root.value(QStringLiteral("generator")).toString().toStdString();
        }

        const QJsonArray sections = root.value(QStringLiteral("frameSections")).toArray();
        m_metadata.sections.clear();
        m_metadata.sections.reserve(static_cast<std::size_t>(sections.size()));
        for (const QJsonValueConstRef& value : sections) {
            const QJsonObject section = value.toObject();
            SectionDescriptor descriptor{};
            const QString type = section.value(QStringLiteral("type")).toString();
            if (type.size() == 4) {
                for (int i = 0; i < 4; ++i) {
                    descriptor.tag[static_cast<std::size_t>(i)] = type.at(i).toLatin1();
                }
            }
            descriptor.strideBytes = static_cast<std::uint32_t>(
                section.value(QStringLiteral("strideBytes")).toDouble(0.0));
            descriptor.elementCount = static_cast<std::uint32_t>(
                section.value(QStringLiteral("count")).toDouble(0.0));
            m_metadata.sections.push_back(descriptor);
        }

        return true;
    }

    bool EpsReader::parseFrameIndex(QString* errorMessage)
    {
        m_index.clear();
        m_hasTrailer = false;

        if (m_indexChunkOffset == 0) {
            // 未正常收尾：退化为顺序扫描。
            return scanFrameChunks(errorMessage);
        }

        std::vector<std::byte> payload;
        if (!readChunkPayload(m_indexChunkOffset, kTagIndex, &payload, errorMessage)) {
            return false;
        }

        if (payload.size() % kIndexEntryBytes != 0) {
            setError(errorMessage, QStringLiteral("索引块长度不是条目长度的整数倍"));
            return false;
        }

        const std::size_t entryCount = payload.size() / kIndexEntryBytes;
        m_index.reserve(entryCount);

        ByteReader reader(payload);
        for (std::size_t i = 0; i < entryCount; ++i) {
            IndexEntry entry{};
            entry.frameOffset = reader.readU64();
            entry.chunkLength = reader.readU32();
            entry.payloadLength = reader.readU32();
            if (!reader.ok()) {
                setError(errorMessage, QStringLiteral("索引块第 %1 条记录越界").arg(i));
                return false;
            }
            m_index.push_back(entry);
        }

        // 尾块存在与否只影响「文件是否完整」的提示，不影响读取。
        const std::size_t indexPadding = (kAlignment - (payload.size() % kAlignment)) % kAlignment;
        const qint64 endChunkOffset = static_cast<qint64>(m_indexChunkOffset) +
                                      static_cast<qint64>(kChunkHeaderBytes) +
                                      static_cast<qint64>(payload.size()) +
                                      static_cast<qint64>(indexPadding);
        if (endChunkOffset >= 0 && m_file.size() >= endChunkOffset + static_cast<qint64>(kChunkHeaderBytes)) {
            if (m_file.seek(endChunkOffset)) {
                const QByteArray tagBytes = m_file.read(4);
                if (tagBytes.size() == 4) {
                    FourCC tag{};
                    for (int i = 0; i < 4; ++i) {
                        tag[static_cast<std::size_t>(i)] = tagBytes.at(i);
                    }
                    m_hasTrailer = (tag == kTagEnd);
                }
            }
        }

        return true;
    }

    bool EpsReader::scanFrameChunks(QString* errorMessage)
    {
        // 从场景块之后开始，顺序遍历块头，收集所有 FRAM 块。
        const qint64 fileSize = m_file.size();
        qint64 offset = static_cast<qint64>(m_sceneChunkOffset);

        // 先跳过场景块本身。
        if (!m_file.seek(offset)) {
            setError(errorMessage, QStringLiteral("无法定位到场景描述块"));
            return false;
        }
        const QByteArray sceneHeader = m_file.read(static_cast<qint64>(kChunkHeaderBytes));
        if (sceneHeader.size() != static_cast<qsizetype>(kChunkHeaderBytes)) {
            setError(errorMessage, QStringLiteral("场景描述块头部不完整"));
            return false;
        }
        {
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(sceneHeader.constData());
            ByteReader reader(bytes, kChunkHeaderBytes);
            std::ignore = reader.readFourCC();
            std::ignore = reader.readU32();
            const std::uint64_t payloadLength = reader.readU64();
            const std::size_t padding = (kAlignment - (payloadLength % kAlignment)) % kAlignment;
            offset += static_cast<qint64>(kChunkHeaderBytes) +
                      static_cast<qint64>(payloadLength) +
                      static_cast<qint64>(padding);
        }

        while (offset + static_cast<qint64>(kChunkHeaderBytes) <= fileSize) {
            if (!m_file.seek(offset)) {
                break;
            }
            const QByteArray header = m_file.read(static_cast<qint64>(kChunkHeaderBytes));
            if (header.size() != static_cast<qsizetype>(kChunkHeaderBytes)) {
                break;
            }

            const auto* bytes = reinterpret_cast<const std::uint8_t*>(header.constData());
            ByteReader reader(bytes, kChunkHeaderBytes);
            const FourCC tag = reader.readFourCC();
            std::ignore = reader.readU32();
            const std::uint64_t payloadLength = reader.readU64();
            std::ignore = reader.readU32();
            std::ignore = reader.readU32();

            const std::size_t padding = (kAlignment - (payloadLength % kAlignment)) % kAlignment;
            const std::uint64_t chunkLength = kChunkHeaderBytes + payloadLength + padding;

            if (tag == kTagFrame) {
                IndexEntry entry{};
                entry.frameOffset = static_cast<std::uint64_t>(offset);
                entry.chunkLength = static_cast<std::uint32_t>(chunkLength);
                entry.payloadLength = static_cast<std::uint32_t>(payloadLength);
                m_index.push_back(entry);
            }
            else if (tag == kTagEnd) {
                m_hasTrailer = true;
                break;
            }
            else if (tag != kTagMeta) {
                // 未知块按长度跳过，保证向前兼容。
            }

            offset += static_cast<qint64>(chunkLength);
        }

        if (m_index.empty()) {
            setError(errorMessage, QStringLiteral("文件中没有找到任何帧块"));
            return false;
        }
        return true;
    }

    bool EpsReader::readChunkPayload(const std::uint64_t chunkOffset,
                                     const FourCC& expectedTag,
                                     std::vector<std::byte>* payload,
                                     QString* errorMessage)
    {
        if (payload == nullptr) {
            setError(errorMessage, QStringLiteral("内部错误：输出载荷指针为空"));
            return false;
        }

        if (!m_file.seek(static_cast<qint64>(chunkOffset))) {
            setError(errorMessage, QStringLiteral("无法定位到偏移 %1").arg(chunkOffset));
            return false;
        }

        const QByteArray rawHeader = m_file.read(static_cast<qint64>(kChunkHeaderBytes));
        if (rawHeader.size() != static_cast<qsizetype>(kChunkHeaderBytes)) {
            setError(errorMessage, QStringLiteral("偏移 %1 处的数据块头不完整").arg(chunkOffset));
            return false;
        }

        const auto* headerBytes = reinterpret_cast<const std::uint8_t*>(rawHeader.constData());
        ByteReader headerReader(headerBytes, kChunkHeaderBytes);
        const FourCC tag = headerReader.readFourCC();
        std::ignore = headerReader.readU32();
        const std::uint64_t payloadLength = headerReader.readU64();
        const std::uint32_t payloadCrc = headerReader.readU32();
        std::ignore = headerReader.readU32();

        if (tag != expectedTag) {
            setError(errorMessage, QStringLiteral("偏移 %1 处的块类型为 %2，期望 %3")
                                       .arg(chunkOffset)
                                       .arg(describeTag(tag), describeTag(expectedTag)));
            return false;
        }

        if (payloadLength > static_cast<std::uint64_t>(m_file.size())) {
            setError(errorMessage, QStringLiteral("块载荷长度 %1 超出文件大小").arg(payloadLength));
            return false;
        }

        const QByteArray rawPayload = m_file.read(static_cast<qint64>(payloadLength));
        if (static_cast<std::uint64_t>(rawPayload.size()) != payloadLength) {
            setError(errorMessage, QStringLiteral("读取块载荷失败：偏移 %1").arg(chunkOffset));
            return false;
        }

        const auto actualCrc = crc32c(rawPayload.constData(), static_cast<std::size_t>(payloadLength));
        if (actualCrc != payloadCrc) {
            setError(errorMessage, QStringLiteral("块 %1 校验失败（偏移 %2，期望 0x%3，实际 0x%4）")
                                       .arg(describeTag(tag))
                                       .arg(chunkOffset)
                                       .arg(payloadCrc, 8, 16, QLatin1Char('0'))
                                       .arg(actualCrc, 8, 16, QLatin1Char('0')));
            return false;
        }

        payload->assign(reinterpret_cast<const std::byte*>(rawPayload.constData()),
                        reinterpret_cast<const std::byte*>(rawPayload.constData()) + rawPayload.size());
        return true;
    }

    bool EpsReader::readFrame(const std::uint64_t frameIndex, FrameState* out, QString* errorMessage)
    {
        if (!m_open) {
            setError(errorMessage, QStringLiteral("EPS 读取器尚未打开"));
            return false;
        }
        if (out == nullptr) {
            setError(errorMessage, QStringLiteral("内部错误：输出帧指针为空"));
            return false;
        }
        if (frameIndex >= frameCount()) {
            setError(errorMessage, QStringLiteral("帧号 %1 越界（共 %2 帧）").arg(frameIndex).arg(frameCount()));
            return false;
        }

        std::vector<std::byte> payload;
        if (!readChunkPayload(m_index[static_cast<std::size_t>(frameIndex)].frameOffset,
                              kTagFrame,
                              &payload,
                              errorMessage)) {
            return false;
        }

        return parseFramePayload(payload, out, errorMessage);
    }

    bool EpsReader::readBody(const std::uint64_t frameIndex,
                             const std::uint32_t slot,
                             BodyState* out,
                             QString* errorMessage)
    {
        if (out == nullptr) {
            setError(errorMessage, QStringLiteral("内部错误：输出刚体指针为空"));
            return false;
        }

        FrameState frame{};
        if (!readFrame(frameIndex, &frame, errorMessage)) {
            return false;
        }

        for (const BodyState& body : frame.bodies) {
            if (body.slot == slot) {
                *out = body;
                return true;
            }
        }

        setError(errorMessage, QStringLiteral("第 %1 帧中不存在槽位 %2 的刚体").arg(frameIndex).arg(slot));
        return false;
    }

    bool EpsReader::parseFramePayload(const std::vector<std::byte>& payload,
                                      FrameState* out,
                                      QString* errorMessage) const
    {
        ByteReader reader(payload);

        out->frameIndex = reader.readU32();
        std::ignore = reader.readU32(); // frameFlags
        out->simTimeNs = reader.readU64();
        out->stepIndex = reader.readU64();
        if (!reader.ok()) {
            setError(errorMessage, QStringLiteral("帧头解析越界"));
            return false;
        }

        out->bodies.clear();
        out->constraints.clear();
        out->fields.clear();

        while (reader.remaining() >= kSectionHeaderBytes) {
            const FourCC tag = reader.readFourCC();
            const std::uint32_t elementCount = reader.readU32();
            const std::uint32_t strideBytes = reader.readU32();
            std::ignore = reader.readU32(); // sectionFlags
            const std::uint64_t sectionBytes = reader.readU64();
            const std::uint32_t sectionCrc = reader.readU32();
            std::ignore = reader.readU32(); // recordVersion

            if (!reader.ok()) {
                setError(errorMessage, QStringLiteral("帧段头解析越界"));
                return false;
            }

            if (sectionBytes > reader.remaining()) {
                setError(errorMessage, QStringLiteral("帧段 %1 载荷长度 %2 超出帧范围")
                                           .arg(describeTag(tag))
                                           .arg(sectionBytes));
                return false;
            }

            const std::uint8_t* sectionData = reader.cursor();
            const std::uint32_t actualCrc = crc32c(sectionData, static_cast<std::size_t>(sectionBytes));
            if (actualCrc != sectionCrc) {
                setError(errorMessage, QStringLiteral("帧段 %1 校验失败（期望 0x%2，实际 0x%3）")
                                           .arg(describeTag(tag))
                                           .arg(sectionCrc, 8, 16, QLatin1Char('0'))
                                           .arg(actualCrc, 8, 16, QLatin1Char('0')));
                return false;
            }

            bool parsed{true};
            if (tag == kSectionBody) {
                parsed = parseBodySection(sectionData, static_cast<std::size_t>(sectionBytes),
                                          elementCount, strideBytes, &out->bodies, errorMessage);
            }
            else if (tag == kSectionConstraint) {
                parsed = parseConstraintSection(sectionData, static_cast<std::size_t>(sectionBytes),
                                                elementCount, strideBytes, &out->constraints, errorMessage);
            }
            else if (tag == kSectionField) {
                parsed = parseFieldSection(sectionData, static_cast<std::size_t>(sectionBytes),
                                           elementCount, strideBytes, &out->fields, errorMessage);
            }
            else {
                // 未知段按长度跳过，保证向前兼容。
            }

            if (!parsed) {
                return false;
            }

            reader.skip(static_cast<std::size_t>(sectionBytes));
        }

        return true;
    }
}
