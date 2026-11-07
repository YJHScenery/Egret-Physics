/**
 * @file        code_editor_settings.h
 * @brief       代码编辑器的字体族与字号设置，负责枚举等宽字体并持久化到 QSettings。
 * @details     用户选择的字体族与字号保存在用户配置目录（QSettings::IniFormat /
 *              UserScope / 组织名 "EgretPhysics"），键为 `editor/codeEditor/fontFamily`
 *              与 `editor/codeEditor/fontSize`。默认字体为 JetBrains Mono，默认字号 14，
 *              字号限制在 10–28 之间，超出范围时自动夹紧。
 *
 *              字体族列表只包含等宽字体（QFontDatabase::isFixedPitch），并把
 *              JetBrains Mono 等常见代码字体排在最前，其余按字母序排列。
 *              若默认字体未注册（例如 qrc 字体加载失败），会退回列表中的第一个等宽字体，
 *              同时在构造时输出一条 qWarning 便于排查。
 *
 * @author      jehor
 * @date        2026-11-07
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 */

#ifndef EGRET_PHYSICS_CODE_EDITOR_SETTINGS_H
#define EGRET_PHYSICS_CODE_EDITOR_SETTINGS_H

#include <QObject>
#include <QString>
#include <QStringList>

namespace egret
{
    /**
     * @brief 代码编辑器字体设置的单例对象。
     * @details 通过 QML 单例 `CodeEditorSettings` 使用；所有属性变化都会立即写入 QSettings。
     */
    class CodeEditorSettings final : public QObject
    {
        Q_OBJECT

        /** @brief 当前字体族名称。 */
        Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)

        /** @brief 当前字号（像素），范围 10–28。 */
        Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)

        /** @brief 允许的最小字号。 */
        Q_PROPERTY(int minimumFontSize READ minimumFontSize CONSTANT)

        /** @brief 允许的最大字号。 */
        Q_PROPERTY(int maximumFontSize READ maximumFontSize CONSTANT)

        /** @brief 默认字体族（JetBrains Mono）。 */
        Q_PROPERTY(QString defaultFontFamily READ defaultFontFamily CONSTANT)

        /** @brief 可选的等宽字体族列表。 */
        Q_PROPERTY(QStringList fontFamilies READ fontFamilies NOTIFY fontFamiliesChanged)

        /** @brief 默认字体是否已经注册到 QFontDatabase。 */
        Q_PROPERTY(bool defaultFontAvailable READ defaultFontAvailable NOTIFY fontFamiliesChanged)

    public:
        /** @brief 获取进程级单例。 */
        static CodeEditorSettings* instance();

        /** @brief 析构函数。 */
        ~CodeEditorSettings() override;

        /** @brief 返回当前字体族。 */
        [[nodiscard]] QString fontFamily() const;

        /**
         * @brief 设置字体族并持久化。
         * @param family 字体族名称；空字符串会被忽略。
         * @note 名称会与已枚举的等宽字体做大小写不敏感匹配，取列表中的标准写法。
         */
        void setFontFamily(const QString& family);

        /** @brief 返回当前字号。 */
        [[nodiscard]] int fontSize() const;

        /**
         * @brief 设置字号并持久化。
         * @param size 目标字号，超出 [10, 28] 时自动夹紧。
         */
        void setFontSize(int size);

        /** @brief 返回允许的最小字号（10）。 */
        [[nodiscard]] int minimumFontSize() const;

        /** @brief 返回允许的最大字号（28）。 */
        [[nodiscard]] int maximumFontSize() const;

        /** @brief 返回默认字体族名称（JetBrains Mono）。 */
        [[nodiscard]] QString defaultFontFamily() const;

        /** @brief 返回等宽字体族列表。 */
        [[nodiscard]] QStringList fontFamilies() const;

        /** @brief 返回默认字体是否可用。 */
        [[nodiscard]] bool defaultFontAvailable() const;

        /**
         * @brief 重新枚举系统与已注册的应用字体。
         * @details QML 侧在组件完成时调用一次，用于兜底「字体晚于单例注册」的情况；
         *          若当前字体已不存在，会自动回退到默认字体。
         */
        Q_INVOKABLE void refreshFontFamilies();

        /** @brief 恢复默认字体族与字号（JetBrains Mono / 14）。 */
        Q_INVOKABLE void resetToDefaults();

    signals:
        /** @brief 字体族变化。 */
        void fontFamilyChanged();
        /** @brief 字号变化。 */
        void fontSizeChanged();
        /** @brief 可选字体列表变化。 */
        void fontFamiliesChanged();

    private:
        /** @brief 私有构造，保证只能通过 instance() 创建。 */
        explicit CodeEditorSettings(QObject* parent = nullptr);

        /** @brief 从 QSettings 读取持久化设置。 */
        void load();

        /** @brief 写入单个键并立即同步到磁盘。 */
        void persist(const QString& key, const QVariant& value) const;

        /** @brief 在列表中查找字体族的标准写法；找不到返回空串。 */
        [[nodiscard]] QString canonicalFamily(const QString& family) const;

        /** @brief 选择一个可用的字体族：优先请求值，其次默认字体，最后列表首项。 */
        [[nodiscard]] QString resolveFamily(const QString& requested) const;

        QStringList m_fontFamilies; ///< 等宽字体族列表
        QString m_fontFamily;       ///< 当前字体族
        int m_fontSize = 0;         ///< 当前字号
    };
} // namespace egret

#endif // EGRET_PHYSICS_CODE_EDITOR_SETTINGS_H
