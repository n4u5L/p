#include "ua_stylesheet.h"

#include <cstring>

#include "lexbor/css/css.h"
#include "lexbor/css/stylesheet.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/style/html/interfaces/document.h"

namespace style {

namespace {

// A pragmatic subset of the HTML5 UA stylesheet (per the HTML spec's
// "Rendering" section). Enough to drive normal-flow layout: block-level
// elements, headings/paragraph margins, lists, basic table display, and the
// hidden head metadata. Extend as layout coverage grows.
constexpr char kUaCss[] = R"CSS(
html, address, blockquote, body, dd, div, dl, dt, fieldset, form,
frame, frameset, h1, h2, h3, h4, h5, h6, noframes, ol, p, ul,
center, dir, hr, menu, pre, section, article, aside, header, footer,
nav, figure, figcaption, main, details, summary { display: block; }

head, link, meta, script, style, title, base { display: none; }

li { display: list-item; }
table { display: table; }
tr { display: table-row; }
td, th { display: table-cell; }
thead { display: table-header-group; }
tbody { display: table-row-group; }
tfoot { display: table-footer-group; }

body { margin: 8px; }
h1 { font-size: 2em; margin: 0.67em 0; font-weight: bold; }
h2 { font-size: 1.5em; margin: 0.83em 0; font-weight: bold; }
h3 { font-size: 1.17em; margin: 1em 0; font-weight: bold; }
h4 { margin: 1.33em 0; font-weight: bold; }
h5 { font-size: 0.83em; margin: 1.67em 0; font-weight: bold; }
h6 { font-size: 0.67em; margin: 2.33em 0; font-weight: bold; }
p, blockquote, figure { margin: 1em 0; }
blockquote { margin-left: 40px; margin-right: 40px; }
ul, ol, dir, menu, dd { margin: 1em 0; padding-left: 40px; }
pre { margin: 1em 0; white-space: pre; }
hr { margin: 0.5em 0; border-style: inset; border-width: 1px; }

b, strong { font-weight: bold; }
i, em, cite { font-style: italic; }
a { color: rgb(0, 0, 238); }

table { border-spacing: 2px; }
td, th { padding: 1px; }
th { font-weight: bold; }
)CSS";

} // namespace

const char* uaStylesheetSource() {
  return kUaCss;
}

size_t uaStylesheetLength() {
  return sizeof(kUaCss) - 1;
}

bool attachUaStylesheet(lxb_html_document_t* doc, lxb_css_parser_t* parser) {
  lxb_css_stylesheet_t* sst = lxb_css_stylesheet_create(nullptr);
  if (sst == nullptr) {
    return false;
  }
  lxb_status_t status = lxb_css_stylesheet_parse(
      sst,
      parser,
      reinterpret_cast<const lxb_char_t*>(uaStylesheetSource()),
      uaStylesheetLength());
  if (status != LXB_STATUS_OK) {
    return false;
  }
  return lxb_html_document_stylesheet_attach(doc, sst) == LXB_STATUS_OK;
}

} // namespace style
