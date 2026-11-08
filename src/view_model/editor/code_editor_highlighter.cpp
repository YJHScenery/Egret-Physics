/** @file code_editor_highlighter.cpp @brief QML 文本文档的 C 语言词法高亮实现。 */
#include "code_editor_highlighter.h"
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSet>

namespace egret {
/** @brief 初始化默认语法颜色。 */
CodeEditorHighlighter::CodeEditorHighlighter(QObject* parent) : QSyntaxHighlighter(parent),
    m_keywordColor("#FF6D8A"), m_typeColor("#34D5C9"), m_preprocessorColor("#B99AFF"),
    m_stringColor("#FFB84D"), m_charColor("#FFB84D"), m_numberColor("#B99AFF"),
    m_commentColor("#8DB3D9"), m_functionColor("#27B5FF") { applyFormats(); }
/** @brief 断开文档销毁通知。 */
CodeEditorHighlighter::~CodeEditorHighlighter() { disconnect(m_documentDestroyedConnection); }
/** @brief 返回关联文档。 */
QQuickTextDocument* CodeEditorHighlighter::textDocument() const { return m_textDocument; }
/** @brief 绑定文档并监听其生命周期。 */
void CodeEditorHighlighter::setTextDocument(QQuickTextDocument* document) {
    if (document == m_textDocument) return;
    disconnect(m_documentDestroyedConnection);
    m_textDocument = document;
    setDocument(document ? document->textDocument() : nullptr);
    if (document) m_documentDestroyedConnection = connect(document, &QObject::destroyed, this, [this] {
        m_textDocument = nullptr; setDocument(nullptr); emit textDocumentChanged();
    });
    emit textDocumentChanged();
}

/** @brief 定义颜色访问与更新函数。 */
#define EGRET_COLOR(Name, name) \
    QColor CodeEditorHighlighter::name##Color() const { return m_##name##Color; } \
    void CodeEditorHighlighter::set##Name##Color(const QColor& color) { \
        if (m_##name##Color == color) return; \
        m_##name##Color = color; handleColorChanged(); emit name##ColorChanged(); }
EGRET_COLOR(Keyword, keyword)
EGRET_COLOR(Type, type)
EGRET_COLOR(Preprocessor, preprocessor)
EGRET_COLOR(String, string)
EGRET_COLOR(Char, char)
EGRET_COLOR(Number, number)
EGRET_COLOR(Comment, comment)
EGRET_COLOR(Function, function)
#undef EGRET_COLOR

/** @brief 刷新格式缓存。 */
void CodeEditorHighlighter::applyFormats() {
    m_keywordFormat.setForeground(m_keywordColor); m_typeFormat.setForeground(m_typeColor);
    m_preprocessorFormat.setForeground(m_preprocessorColor); m_stringFormat.setForeground(m_stringColor);
    m_charFormat.setForeground(m_charColor); m_numberFormat.setForeground(m_numberColor);
    m_commentFormat.setForeground(m_commentColor); m_commentFormat.setFontItalic(true);
    m_functionFormat.setForeground(m_functionColor);
}
/** @brief 在主题变化后重新着色。 */
void CodeEditorHighlighter::handleColorChanged() { applyFormats(); rehighlight(); }

/** @brief 按词法优先级扫描一行，并保留跨行注释状态。 */
void CodeEditorHighlighter::highlightBlock(const QString& text) {
    static const QSet<QString> keywords = {"if", "else", "for", "while", "do", "switch", "case", "default",
        "break", "continue", "return", "goto", "sizeof", "typedef", "static", "extern", "const", "volatile",
        "restrict", "inline", "register", "auto", "_Alignas", "_Alignof", "_Static_assert", "_Thread_local"};
    static const QSet<QString> types = {"void", "char", "short", "int", "long", "float", "double", "signed",
        "unsigned", "struct", "union", "enum", "bool", "_Bool", "_Atomic", "_Complex", "Entity", "Vector3",
        "Vec3", "true", "false", "NULL", "size_t", "uint64_t", "int64_t"};
    static const QRegularExpression number(QStringLiteral(
        R"((?:0[xX](?:[0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?|\.[0-9a-fA-F]+)(?:[pP][+-]?\d+)?|(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)[uUlLfF]*)"));
    if(m_lua) {
        static const QSet<QString> words={"and","break","do","else","elseif","end","false","for","function","goto","if","in","local","nil","not","or","repeat","return","then","true","until","while"};
        bool block=previousBlockState()==StateBlockComment;
        setCurrentBlockState(StateNormal);
        for(int i=0;i<text.size();) {
            int start=i;
            if(block || text.mid(i,4)=="--[[") {
                int end=text.indexOf("]]",i+(block?0:4));i=end<0?text.size():end+2;
                setFormat(start,i-start,m_commentFormat);block=end<0;if(block)setCurrentBlockState(StateBlockComment);
            }else if(text.mid(i,2)=="--"){setFormat(i,text.size()-i,m_commentFormat);break;}
            else if(text[i]=='"' || text[i]=='\'') {auto quote=text[i++];while(i<text.size()){if(text[i]=='\\'){i=qMin(i+2,int(text.size()));continue;}if(text[i++]==quote)break;}setFormat(start,i-start,m_stringFormat);}
            else if(text[i].isLetter() || text[i]=='_'){while(i<text.size()&&(text[i].isLetterOrNumber()||text[i]=='_'))++i;auto word=text.mid(start,i-start);int next=i;while(next<text.size()&&text[next].isSpace())++next;if(words.contains(word))setFormat(start,i-start,m_keywordFormat);else if(next<text.size()&&text[next]=='(')setFormat(start,i-start,m_functionFormat);}
            else if(text[i].isDigit()){auto match=number.match(text,i,QRegularExpression::NormalMatch,QRegularExpression::AnchorAtOffsetMatchOption);i+=match.hasMatch()?match.capturedLength():1;setFormat(start,i-start,m_numberFormat);}
            else ++i;
        }
        return;
    }
    bool comment = previousBlockState() == StateBlockComment;
    setCurrentBlockState(StateNormal);
    for (int i = 0; i < text.size();) {
        const int start = i;
        if (comment || text.mid(i, 2) == "/*") {
            const int end = text.indexOf("*/", i + (comment ? 0 : 2));
            i = end < 0 ? text.size() : end + 2;
            setFormat(start, i - start, m_commentFormat);
            comment = end < 0;
            if (comment) setCurrentBlockState(StateBlockComment);
        } else if (text.mid(i, 2) == "//") {
            setFormat(i, text.size() - i, m_commentFormat); break;
        } else if (text[i] == '"' || text[i] == '\'') {
            const QChar quote = text[i++];
            while (i < text.size()) {
                if (text[i] == '\\') { i = qMin(i + 2, int(text.size())); continue; }
                if (text[i++] == quote) break;
            }
            setFormat(start, i - start, quote == '"' ? m_stringFormat : m_charFormat);
        } else if (text[i] == '#' && text.left(i).trimmed().isEmpty()) {
            ++i; while (i < text.size() && text[i].isSpace()) ++i;
            while (i < text.size() && text[i].isLetter()) ++i;
            setFormat(start, i - start, m_preprocessorFormat);
        } else if (text[i].isDigit() || (text[i] == '.' && i + 1 < text.size() && text[i + 1].isDigit())) {
            const auto match = number.match(text, i, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
            i += match.hasMatch() ? match.capturedLength() : 1;
            setFormat(start, i - start, m_numberFormat);
        } else if (text[i].isLetter() || text[i] == '_') {
            while (i < text.size() && (text[i].isLetterOrNumber() || text[i] == '_')) ++i;
            const QString word = text.mid(start, i - start);
            int next = i; while (next < text.size() && text[next].isSpace()) ++next;
            if (keywords.contains(word)) setFormat(start, i - start, m_keywordFormat);
            else if (types.contains(word)) setFormat(start, i - start, m_typeFormat);
            else if (next < text.size() && text[next] == '(') setFormat(start, i - start, m_functionFormat);
        } else ++i;
    }
}
}
