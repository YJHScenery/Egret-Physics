/**
 * @file        code_editor_highlighter.h
 * @brief       C 语言语法高亮器，基于 QSyntaxHighlighter 实现并暴露给 QML 使用。
 * @details     本类把 Qt Widgets 时代的 QSyntaxHighlighter 与 QML 的 TextEdit/TextArea
 *              连接起来：QML 侧把 `TextArea.textDocument`（QQuickTextDocument*）赋给
 *              `textDocument` 属性，本类内部再取出底层的 QTextDocument 交给 QSyntaxHighlighter。
 *              颜色全部通过 Q_PROPERTY 由 QML 传入，因此可以跟随 DeepBlueTheme 主题色变化，
 *              切换主题时只需要重新赋值颜色即可触发 rehighlight()。
 *
 *              高亮采用手写词法扫描而不是逐条正则，原因是：
 *              - 正则无法可靠处理跨行块注释与字符串/注释的优先级关系；
 *              - 手写扫描可以精确控制「函数名 = 标识符 + 左括号」这类上下文相关规则；
 *              - 对几十到几百行规模的 C 控制程序，扫描开销可以忽略。
 *
 *              支持的语法元素：预处理指令、注释（// 与 跨行 /* *\/）、字符串、字符常量、
 *              数值（十六进制/浮点/指数/后缀）、控制关键字、内建类型、函数名。
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

#ifndef EGRET_PHYSICS_CODE_EDITOR_HIGHLIGHTER_H
#define EGRET_PHYSICS_CODE_EDITOR_HIGHLIGHTER_H

#include <QColor>
#include <QObject>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QQuickTextDocument>

#include <memory>

QT_BEGIN_NAMESPACE
class QQuickTextDocument;
QT_END_NAMESPACE

namespace egret
{
    /**
     * @brief C 语言语法高亮器。
     * @details 由 QML 创建并与 `TextArea.textDocument` 绑定；所有颜色属性都可在 QML 中
     *          直接绑定主题色，属性变化后会自动重新高亮整篇文档。
     */
    class CodeEditorHighlighter : public QSyntaxHighlighter
    {
        Q_OBJECT

        /** @brief 需要高亮的富文本文档，通常绑定 `TextArea.textDocument`。 */
        Q_PROPERTY(QQuickTextDocument* textDocument READ textDocument WRITE setTextDocument
                       NOTIFY textDocumentChanged)

        /** @brief 控制关键字（if、for、return 等）颜色。 */
        Q_PROPERTY(QColor keywordColor READ keywordColor WRITE setKeywordColor NOTIFY keywordColorChanged)

        /** @brief 内建类型（int、double、char 等）颜色。 */
        Q_PROPERTY(QColor typeColor READ typeColor WRITE setTypeColor NOTIFY typeColorChanged)

        /** @brief 预处理指令（#include、#define 等）颜色。 */
        Q_PROPERTY(QColor preprocessorColor READ preprocessorColor WRITE setPreprocessorColor
                       NOTIFY preprocessorColorChanged)

        /** @brief 字符串字面量颜色。 */
        Q_PROPERTY(QColor stringColor READ stringColor WRITE setStringColor NOTIFY stringColorChanged)

        /** @brief 字符字面量颜色。 */
        Q_PROPERTY(QColor charColor READ charColor WRITE setCharColor NOTIFY charColorChanged)

        /** @brief 数值字面量颜色。 */
        Q_PROPERTY(QColor numberColor READ numberColor WRITE setNumberColor NOTIFY numberColorChanged)

        /** @brief 注释颜色（斜体显示）。 */
        Q_PROPERTY(QColor commentColor READ commentColor WRITE setCommentColor NOTIFY commentColorChanged)

        /** @brief 函数名颜色（标识符后紧跟左括号时判定）。 */
        Q_PROPERTY(QColor functionColor READ functionColor WRITE setFunctionColor NOTIFY functionColorChanged)

    public:
        /**
         * @brief 构造语法高亮器。
         * @param parent 父对象，由 QML 引擎托管生命周期。
         */
        explicit CodeEditorHighlighter(QObject* parent = nullptr);

        /** @brief 析构函数，断开与 QML 文档的连接。 */
        ~CodeEditorHighlighter() override;

        /** @brief 返回当前关联的 QML 富文本文件对象。 */
        [[nodiscard]] QQuickTextDocument* textDocument() const;

        /**
         * @brief 绑定 QML 的富文本文档。
         * @param document 由 `TextArea.textDocument` 提供的对象，可为 nullptr（解绑）。
         */
        void setTextDocument(QQuickTextDocument* document);

        /** @brief 返回控制关键字颜色。 */
        [[nodiscard]] QColor keywordColor() const;
        /** @brief 设置控制关键字颜色，并触发重新高亮。 */
        void setKeywordColor(const QColor& color);

        /** @brief 返回内建类型颜色。 */
        [[nodiscard]] QColor typeColor() const;
        /** @brief 设置内建类型颜色，并触发重新高亮。 */
        void setTypeColor(const QColor& color);

        /** @brief 返回预处理指令颜色。 */
        [[nodiscard]] QColor preprocessorColor() const;
        /** @brief 设置预处理指令颜色，并触发重新高亮。 */
        void setPreprocessorColor(const QColor& color);

        /** @brief 返回字符串颜色。 */
        [[nodiscard]] QColor stringColor() const;
        /** @brief 设置字符串颜色，并触发重新高亮。 */
        void setStringColor(const QColor& color);

        /** @brief 返回字符常量颜色。 */
        [[nodiscard]] QColor charColor() const;
        /** @brief 设置字符常量颜色，并触发重新高亮。 */
        void setCharColor(const QColor& color);

        /** @brief 返回数值常量颜色。 */
        [[nodiscard]] QColor numberColor() const;
        /** @brief 设置数值常量颜色，并触发重新高亮。 */
        void setNumberColor(const QColor& color);

        /** @brief 返回注释颜色。 */
        [[nodiscard]] QColor commentColor() const;
        /** @brief 设置注释颜色，并触发重新高亮。 */
        void setCommentColor(const QColor& color);

        /** @brief 返回函数名颜色。 */
        [[nodiscard]] QColor functionColor() const;
        /** @brief 设置函数名颜色，并触发重新高亮。 */
        void setFunctionColor(const QColor& color);

        /**
         * @brief 逐块高亮入口，由 QSyntaxHighlighter 在文档内容变化时调用。
         * @param text 当前文本块（一行）的内容。
         */
        void highlightBlock(const QString& text) override;

    signals:
        /** @brief 关联的 QML 文档发生变化。 */
        void textDocumentChanged();
        /** @brief 控制关键字颜色变化。 */
        void keywordColorChanged();
        /** @brief 内建类型颜色变化。 */
        void typeColorChanged();
        /** @brief 预处理指令颜色变化。 */
        void preprocessorColorChanged();
        /** @brief 字符串颜色变化。 */
        void stringColorChanged();
        /** @brief 字符常量颜色变化。 */
        void charColorChanged();
        /** @brief 数值常量颜色变化。 */
        void numberColorChanged();
        /** @brief 注释颜色变化。 */
        void commentColorChanged();
        /** @brief 函数名颜色变化。 */
        void functionColorChanged();

    private:
        /** @brief 文本块状态：普通代码与跨行块注释。 */
        enum BlockState : int
        {
            StateNormal = 0,      ///< 普通代码
            StateBlockComment = 1 ///< 位于 /* ... *\/ 块注释中
        };

        /** @brief 根据当前颜色重建全部 QTextCharFormat 缓存。 */
        void applyFormats();

        /** @brief 颜色变化后统一刷新显示。 */
        void handleColorChanged();

        QQuickTextDocument* m_textDocument = nullptr; ///< QML 侧的文档包装对象（弱引用）
        QMetaObject::Connection m_documentDestroyedConnection; ///< 文档销毁通知连接

        QColor m_keywordColor;      ///< 关键字颜色
        QColor m_typeColor;         ///< 类型颜色
        QColor m_preprocessorColor; ///< 预处理指令颜色
        QColor m_stringColor;       ///< 字符串颜色
        QColor m_charColor;         ///< 字符常量颜色
        QColor m_numberColor;       ///< 数值常量颜色
        QColor m_commentColor;      ///< 注释颜色
        QColor m_functionColor;     ///< 函数名颜色

        QTextCharFormat m_keywordFormat;      ///< 关键字格式缓存
        QTextCharFormat m_typeFormat;         ///< 类型格式缓存
        QTextCharFormat m_preprocessorFormat; ///< 预处理格式缓存
        QTextCharFormat m_stringFormat;       ///< 字符串格式缓存
        QTextCharFormat m_charFormat;         ///< 字符格式缓存
        QTextCharFormat m_numberFormat;       ///< 数值格式缓存
        QTextCharFormat m_commentFormat;      ///< 注释格式缓存
        QTextCharFormat m_functionFormat;     ///< 函数名格式缓存
    };
} // namespace egret

#endif // EGRET_PHYSICS_CODE_EDITOR_HIGHLIGHTER_H
