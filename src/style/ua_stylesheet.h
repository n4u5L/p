#pragma once

// User-agent default stylesheet. lexbor ships no UA styles, so we provide our
// own and attach it as the lowest-source-order sheet, before any author sheet —
// so author `normal` declarations win over UA `normal` by source order.
//
// Caveat (documented in the design): lexbor encodes `!important` into the
// specificity but has no separate origin axis, so the CSS rule
// "UA-important > author-important" is not expressible by source order alone.
// M1 accepts this edge deviation.

#include <cstddef>

// lexbor forward types.
typedef struct lxb_html_document lxb_html_document_t;
typedef struct lxb_css_parser lxb_css_parser_t;

namespace style {

// The UA default CSS (HTML5 element defaults: display, margins, etc.).
const char* uaStylesheetSource();
size_t uaStylesheetLength();

// Parse the UA stylesheet with `parser` and attach it to `doc`. Must be called
// before author stylesheets are attached. Returns true on success.
bool attachUaStylesheet(lxb_html_document_t* doc, lxb_css_parser_t* parser);

} // namespace style
