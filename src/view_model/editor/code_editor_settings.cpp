/**
 * @file        code_editor_settings.cpp
 * @brief       代码编辑器字体设置的实现：枚举等宽字体并读写 QSettings。
 * @details     枚举时跳过以 '@' 开头的竖排字体别名，只保留 isFixedPitch 的字体族；
 *              优先排列常见代码字体，其余按字母序。持久化使用 IniFormat 的
 *              用户级配置，避免与场景文件耦合。
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

#include "editor/code_editor_settings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QSettings>
#include <QVariant>

#include <algorithm>
#include <memory>

namespace egret
{
    namespace
    {
        constexpr int kMinimumFontSize = 10; ///< 允许的最小字号
        constexpr int kMaximumFontSize = 28; ///< 允许的最大字号
        constexpr int kDefaultFontSize = 14; ///< 默认字号

        const QString kOrganization = QStringLiteral("EgretPhysics"); ///< QSettings 组织名
        const QString kFontFamilyKey = QStringLiteral("editor/codeEditor/fontFamily"); ///< 字体族键
        const QString kFontSizeKey = QStringLiteral("editor/codeEditor/fontSize");     ///< 字号键
        const QString kDefaultFontFamily = QStringLiteral("JetBrains Mono");           ///< 默认字体

        /**
         * @brief 取应用名，用于 QSettings 的应用级路径。
         * @return main.cpp 中设置的应用名；为空时回退到常量。
         */
        QString applicationName()
        {
            const QString name = QCoreApplication::applicationName();
            return name.isEmpty() ? QStringLiteral("EgretPhysicsStudio") : name;
        }

        /**
         * @brief 创建用户级 Ini 配置对象。
         * @return 新建的 QSettings（QSettings 不可拷贝，因此用 unique_ptr 管理）。
         */
        std::unique_ptr<QSettings> createSettings()
        {
            return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, kOrganization,
                                               applicationName());
        }

        /**
         * @brief 枚举系统中可用的等宽字体族。
         * @return 已排序的字体族列表，JetBrains Mono 等常用代码字体优先。
         */
        QStringList collectMonospaceFamilies()
        {
            QStringList discovered;
            const QStringList allFamilies = QFontDatabase::families();
            for (const QString& family : allFamilies)
            {
                const QString trimmed = family.trimmed();
                // 以 '@' 开头的是 Windows 竖排字体别名，不提供给编辑器选择。
                if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('@'))
                    || discovered.contains(trimmed, Qt::CaseInsensitive))
                {
                    continue;
                }
                if (!QFontDatabase::isFixedPitch(trimmed))
                {
                    continue;
                }
                discovered.append(trimmed);
            }

            static const QStringList preferred{
                QStringLiteral("JetBrains Mono"),   QStringLiteral("JetBrains Mono NL"),
                QStringLiteral("Cascadia Code"),    QStringLiteral("Cascadia Mono"),
                QStringLiteral("Fira Code"),        QStringLiteral("Source Code Pro"),
                QStringLiteral("Consolas"),         QStringLiteral("DejaVu Sans Mono"),
                QStringLiteral("Liberation Mono"),  QStringLiteral("Ubuntu Mono"),
                QStringLiteral("Menlo"),            QStringLiteral("Monaco"),
                QStringLiteral("Courier New"),      QStringLiteral("Lucida Console")};

            QStringList ordered;
            for (const QString& wanted : preferred)
            {
                for (const QString& family : discovered)
                {
                    if (family.compare(wanted, Qt::CaseInsensitive) == 0 && !ordered.contains(family))
                    {
                        ordered.append(family);
                    }
                }
            }

            QStringList rest;
            rest.reserve(discovered.size());
            for (const QString& family : discovered)
            {
                if (!ordered.contains(family))
                {
                    rest.append(family);
                }
            }
            std::sort(rest.begin(), rest.end(), [](const QString& left, const QString& right)
                      { return left.compare(right, Qt::CaseInsensitive) < 0; });

            ordered.append(rest);

            // 个别字体驱动的 isFixedPitch 判断不可靠：默认字体只要真实存在就补到首位，
            // 保证下拉框里一定能选到默认字体。
            bool hasDefault = false;
            for (const QString& family : ordered)
            {
                if (family.compare(kDefaultFontFamily, Qt::CaseInsensitive) == 0)
                {
                    hasDefault = true;
                    break;
                }
            }
            if (!hasDefault && QFontDatabase::hasFamily(kDefaultFontFamily))
            {
                ordered.prepend(kDefaultFontFamily);
            }
            return ordered;
        }
    } // namespace

    CodeEditorSettings* CodeEditorSettings::instance()
    {
        // 函数内静态对象：第一次调用时创建，进程退出时析构，避免泄漏。
        static CodeEditorSettings settingsInstance;
        return &settingsInstance;
    }

    CodeEditorSettings::CodeEditorSettings(QObject* parent)
        : QObject(parent)
        , m_fontFamily(kDefaultFontFamily)
        , m_fontSize(kDefaultFontSize)
    {
        m_fontFamilies = collectMonospaceFamilies();
        load();
        if (!defaultFontAvailable())
        {
            qWarning() << "CodeEditorSettings: 默认字体" << kDefaultFontFamily << "未注册，代码编辑器回退到"
                       << m_fontFamily;
        }
    }

    CodeEditorSettings::~CodeEditorSettings() = default;

    void CodeEditorSettings::load()
    {
        const auto settings = createSettings();
        const QString storedFamily = settings->value(kFontFamilyKey).toString();
        const int storedSize = settings->value(kFontSizeKey, kDefaultFontSize).toInt();

        m_fontSize = std::clamp(storedSize, kMinimumFontSize, kMaximumFontSize);
        m_fontFamily = resolveFamily(storedFamily.isEmpty() ? kDefaultFontFamily : storedFamily);
    }

    void CodeEditorSettings::persist(const QString& key, const QVariant& value) const
    {
        const auto settings = createSettings();
        settings->setValue(key, value);
        settings->sync();
    }

    QString CodeEditorSettings::canonicalFamily(const QString& family) const
    {
        for (const QString& candidate : m_fontFamilies)
        {
            if (candidate.compare(family, Qt::CaseInsensitive) == 0)
            {
                return candidate;
            }
        }
        return {};
    }

    QString CodeEditorSettings::resolveFamily(const QString& requested) const
    {
        const QString canonical = canonicalFamily(requested);
        if (!canonical.isEmpty())
        {
            return canonical;
        }
        const QString fallback = canonicalFamily(kDefaultFontFamily);
        if (!fallback.isEmpty())
        {
            return fallback;
        }
        if (!m_fontFamilies.isEmpty())
        {
            return m_fontFamilies.first();
        }
        // 一个等宽字体都没枚举到：保留请求值，交给 Qt 的字体回退机制。
        return requested.isEmpty() ? kDefaultFontFamily : requested;
    }

    QString CodeEditorSettings::fontFamily() const
    {
        return m_fontFamily;
    }

    void CodeEditorSettings::setFontFamily(const QString& family)
    {
        const QString trimmed = family.trimmed();
        if (trimmed.isEmpty())
        {
            return;
        }
        const QString canonical = resolveFamily(trimmed);
        if (canonical == m_fontFamily)
        {
            return;
        }
        m_fontFamily = canonical;
        persist(kFontFamilyKey, m_fontFamily);
        emit fontFamilyChanged();
    }

    int CodeEditorSettings::fontSize() const
    {
        return m_fontSize;
    }

    void CodeEditorSettings::setFontSize(const int size)
    {
        const int clamped = std::clamp(size, kMinimumFontSize, kMaximumFontSize);
        if (clamped == m_fontSize)
        {
            return;
        }
        m_fontSize = clamped;
        persist(kFontSizeKey, m_fontSize);
        emit fontSizeChanged();
    }

    int CodeEditorSettings::minimumFontSize() const
    {
        return kMinimumFontSize;
    }

    int CodeEditorSettings::maximumFontSize() const
    {
        return kMaximumFontSize;
    }

    QString CodeEditorSettings::defaultFontFamily() const
    {
        return kDefaultFontFamily;
    }

    QStringList CodeEditorSettings::fontFamilies() const
    {
        return m_fontFamilies;
    }

    bool CodeEditorSettings::defaultFontAvailable() const
    {
        return !canonicalFamily(kDefaultFontFamily).isEmpty();
    }

    void CodeEditorSettings::refreshFontFamilies()
    {
        const QStringList families = collectMonospaceFamilies();
        if (families != m_fontFamilies)
        {
            m_fontFamilies = families;
            emit fontFamiliesChanged();
        }

        // 当前字体可能因为字体未注册或已被卸载而失效，这里做一次兜底修正。
        if (canonicalFamily(m_fontFamily).isEmpty())
        {
            const QString fallback = resolveFamily(m_fontFamily);
            if (fallback != m_fontFamily)
            {
                m_fontFamily = fallback;
                persist(kFontFamilyKey, m_fontFamily);
                emit fontFamilyChanged();
            }
        }
    }

    void CodeEditorSettings::resetToDefaults()
    {
        const QString family = resolveFamily(kDefaultFontFamily);
        if (family != m_fontFamily)
        {
            m_fontFamily = family;
            persist(kFontFamilyKey, m_fontFamily);
            emit fontFamilyChanged();
        }
        if (m_fontSize != kDefaultFontSize)
        {
            m_fontSize = kDefaultFontSize;
            persist(kFontSizeKey, m_fontSize);
            emit fontSizeChanged();
        }
    }
} // namespace egret
