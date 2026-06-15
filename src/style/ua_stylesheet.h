#pragma once

// User-agent 默认样式表。lexbor 不自带 UA style，所以这里提供一份，
// 并在任何 author stylesheet 之前 attach，作为最低 source-order。
// 这样 author normal 声明会按 source-order 覆盖 UA normal 声明。
//
// 易错点：lexbor 把 !important 编进 specificity，但没有独立的 origin 轴。
// 因此 “UA-important > author-important” 这条 CSS origin 规则无法只靠
// source-order 表达。M1 接受这个边界偏差。
// TODO：如果以后需要更完整 cascade origin，可在 Lexbor declaration 外层增加
// origin/importance 元数据。

#include <cstddef>

// lexbor 前置类型。
typedef struct lxb_html_document lxb_html_document_t;
typedef struct lxb_css_parser lxb_css_parser_t;

namespace style {

// UA 默认 CSS 文本（HTML 元素 display、margin 等）。
const char* uaStylesheetSource();
size_t uaStylesheetLength();

// 用 parser 解析 UA stylesheet 并 attach 到 doc。
// 必须在 author stylesheet attach 前调用；成功返回 true。
bool attachUaStylesheet(lxb_html_document_t* doc, lxb_css_parser_t* parser);

} // 命名空间 style
