/** @file CIndent.js @brief C 代码换行与闭合花括号的四空格缩进规则。 */
.pragma library

/** @brief 将行首制表符转换为四空格制表位，返回等宽空格缩进。 */
function indentation(line) {
    var width = 0
    for (var i = 0; i < line.length; ++i) {
        if (line[i] === " ") ++width
        else if (line[i] === "\t") width += 4 - width % 4
        else break
    }
    return " ".repeat(width)
}

/** @brief 屏蔽字符串、字符、注释及预处理行，保持字符位置与换行位置不变。 */
function scan(text) {
    var code = "", state = "code", quote = "", escaped = false, lineStart = 0
    for (var i = 0; i < text.length; ++i) {
        var c = text[i], next = text[i + 1]
        if (c === "\n") {
            code += c
            if ((state === "line" || state === "directive") && text[i - 1] !== "\\") state = "code"
            if (state === "quote" && !escaped) state = "code"
            escaped = false
            lineStart = i + 1
            continue
        }
        if (state === "block") {
            code += " "
            if (c === "*" && next === "/") {code += " "; ++i; state = "code"}
        } else if (state === "quote") {
            code += " "
            if (escaped) escaped = false
            else if (c === "\\") escaped = true
            else if (c === quote) state = "code"
        } else if (state === "line" || state === "directive") {
            code += " "
        } else if (c === "/" && (next === "/" || next === "*")) {
            code += "  "; ++i; state = next === "/" ? "line" : "block"
        } else if (c === '"' || c === "'") {
            code += " "; quote = c; state = "quote"
        } else if (c === "#" && /^[ \t]*$/.test(text.slice(lineStart, i))) {
            code += " "; state = "directive"
        } else code += c
    }
    return {code: code, state: state}
}

/** @brief 找到光标前尚未闭合的最内层花括号。 */
function openingBrace(code) {
    var depth = 0
    for (var i = code.length - 1; i >= 0; --i) {
        if (code[i] === "}") ++depth
        else if (code[i] === "{") {
            if (depth === 0) return i
            --depth
        }
    }
    return -1
}

/** @brief 返回指定字符所在行的空格缩进。 */
function lineIndent(text, position) {
    return indentation(text.slice(text.lastIndexOf("\n", position - 1) + 1, position))
}

/** @brief 计算换行替换范围与光标位置；仅进入代码块时增加一级缩进。 */
function newline(text, start, end) {
    var before = text.slice(0, start), after = text.slice(end)
    var lineStart = before.lastIndexOf("\n") + 1
    var base = indentation(before.slice(lineStart))
    var context = scan(before), codeLine = context.code.slice(lineStart)
    var level = 0
    if (context.state === "code" || context.state === "line") {
        for (var i = 0; i < codeLine.length; ++i) {
            if (codeLine[i] === "{") ++level
            else if (codeLine[i] === "}") --level
        }
    }
    var indent = base + "    ".repeat(Math.max(0, level))
    var insertion = "\n" + indent, cursor = start + insertion.length
    var closing = after.match(/^[ \t]*}/)
    var open = openingBrace(context.code)
    if (closing && open >= 0 && (context.state === "code" || context.state === "line")) {
        var outer = lineIndent(text, open)
        end += closing[0].length - 1
        if (context.code.trim().endsWith("{")) insertion += "\n" + outer
        else {insertion = "\n" + outer; cursor = start + insertion.length}
    }
    return {start: start, end: end, text: insertion, cursor: cursor}
}

/** @brief 在空白行输入右花括号时，与对应代码块的起始行对齐。 */
function closingBrace(text, start, end) {
    var lineStart = text.lastIndexOf("\n", start - 1) + 1
    if (!/^[ \t]*$/.test(text.slice(lineStart, start))) return null
    var context = scan(text.slice(0, start))
    if (context.state !== "code") return null
    var open = openingBrace(context.code)
    if (open < 0) return null
    var insertion = lineIndent(text, open) + "}"
    return {start: lineStart, end: end, text: insertion, cursor: lineStart + insertion.length}
}
