// End-to-end smoke test for the M1 style engine: parse HTML + CSS via lexbor,
// attach the UA stylesheet, run StyleResolver, and assert computed values.
//
// Build:
//   cmake --build build --target style_smoke

#include <cmath>
#include <cstdio>
#include <cstring>

#include "lexbor/css/css.h"
#include "lexbor/css/stylesheet.h"
#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/html/html.h"
#include "lexbor/style/style.h"
#include "lexbor/style/html/interfaces/document.h"

#include "computed_style.h"
#include "resolver.h"
#include "ua_stylesheet.h"

namespace {

const char* kHtml =
    "<html><head><style></style></head><body>"
    "<h1 id='title'>Hello</h1>"
    "<p id='para' style='color:#ff0000; margin-left: 2em; width: 50%'>para</p>"
    "<div id='box' class='box'>boxed</div>"
    "<section id='text' class='text'><span id='child'>child</span></section>"
    "<aside id='pos' class='pos'>pos</aside>"
    "</body></html>";

const char* kAuthorCss =
    ".box { display: block; width: 100px; height: 40px;"
    "       background-color: rgb(0,128,255); color: rgb(10,20,30);"
    "       border: thick dashed currentColor; border-left-color: #336699;"
    "       padding: 1em 2px; overflow-x: hidden; overflow-y: scroll; }"
    "p { font-size: 18px; }"
    ".text { font-family: monospace; font-weight: 650; font-style: italic;"
    "        font-stretch: 125%; text-align: center; white-space: pre-wrap;"
    "        text-indent: 2em; letter-spacing: 1px; word-spacing: 0.5em; }"
    ".pos { position: relative; top: 10px; left: 5%; clear: left;"
    "       z-index: 7; box-sizing: border-box; float: left; opacity: 50%; }";

bool failed = false;

void check(bool ok, const char* msg) {
  if (!ok) {
    failed = true;
    std::printf("FAIL: %s\n", msg);
  }
}

bool near(float a, float b) {
  return std::fabs(a - b) < 0.01f;
}

bool lenPx(const style::LengthValue& v, float px) {
  return v.tag == style::LengthValue::Tag::Px && near(v.px, px);
}

bool lenPercent(const style::LengthValue& v, float pct) {
  return v.tag == style::LengthValue::Tag::Percent && near(v.px, pct);
}

bool colorIs(const style::Color& c, unsigned r, unsigned g, unsigned b,
             unsigned a = 255) {
  return c.r == r && c.g == g && c.b == b && c.a == a;
}

lxb_dom_element_t* findById(lxb_dom_node_t* node, const char* id) {
  if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    lxb_dom_element_t* el = lxb_dom_interface_element(node);
    size_t len = 0;
    const lxb_char_t* value = lxb_dom_element_id(el, &len);
    if (value != nullptr && len == std::strlen(id) &&
        std::memcmp(value, id, len) == 0) {
      return el;
    }
  }

  for (lxb_dom_node_t* c = node->first_child; c != nullptr; c = c->next) {
    if (lxb_dom_element_t* found = findById(c, id)) {
      return found;
    }
  }
  return nullptr;
}

void dumpLen(const char* name, const style::LengthValue& v) {
  switch (v.tag) {
  case style::LengthValue::Tag::Px:
    std::printf("%s=%.1fpx ", name, v.px);
    break;
  case style::LengthValue::Tag::Percent:
    std::printf("%s=%.1f%% ", name, v.px);
    break;
  case style::LengthValue::Tag::Auto:
    std::printf("%s=auto ", name);
    break;
  case style::LengthValue::Tag::None:
    std::printf("%s=none ", name);
    break;
  case style::LengthValue::Tag::Calc:
    std::printf("%s=calc ", name);
    break;
  }
}

void dumpTree(lxb_dom_node_t* node, const style::StyleResolver& resolver,
              int depth) {
  if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    lxb_dom_element_t* el = lxb_dom_interface_element(node);
    size_t len = 0;
    const lxb_char_t* name = lxb_dom_element_local_name(el, &len);
    const style::ComputedStyle* cs = resolver.styleFor(node);

    for (int i = 0; i < depth; ++i) std::printf("  ");
    std::printf("<%.*s> ", static_cast<int>(len), name);
    if (cs) {
      std::printf("display=%d ", static_cast<int>(cs->box().display));
      std::printf("font-size=%.1f ", cs->text().font.sizePx);
      std::printf("weight=%d ", cs->text().font.weight);
      std::printf("color=#%02x%02x%02x ", cs->text().color.r, cs->text().color.g, cs->text().color.b);
      std::printf("bg=#%02x%02x%02x%02x ", cs->visual().background.r, cs->visual().background.g, cs->visual().background.b, cs->visual().background.a);
      dumpLen("w", cs->box().width);
      dumpLen("ml", cs->surround().margin[style::kLeft]);
      dumpLen("bt", cs->surround().border[style::kTop].width);
    }
    std::printf("\n");
  }
  for (lxb_dom_node_t* c = node->first_child; c; c = c->next) {
    dumpTree(c, resolver, depth + 1);
  }
}

} // namespace

int main() {
  lxb_html_document_t* doc = lxb_html_document_create();
  if (!doc || lxb_style_init(doc) != LXB_STATUS_OK) {
    std::printf("init failed\n");
    return 1;
  }

  lxb_css_parser_t* parser = lxb_css_parser_create();
  if (lxb_css_parser_init(parser, nullptr) != LXB_STATUS_OK) {
    std::printf("css parser init failed\n");
    return 1;
  }

  if (!style::attachUaStylesheet(doc, parser)) {
    std::printf("UA stylesheet attach failed\n");
    return 1;
  }

  lxb_css_stylesheet_t* author = lxb_css_stylesheet_create(nullptr);
  if (lxb_css_stylesheet_parse(author, parser, reinterpret_cast<const lxb_char_t*>(kAuthorCss), std::strlen(kAuthorCss)) != LXB_STATUS_OK) {
    std::printf("author css parse failed\n");
    return 1;
  }

  if (lxb_html_document_parse(doc, reinterpret_cast<const lxb_char_t*>(kHtml), std::strlen(kHtml)) != LXB_STATUS_OK) {
    std::printf("html parse failed\n");
    return 1;
  }
  if (lxb_html_document_stylesheet_attach(doc, author) != LXB_STATUS_OK) {
    std::printf("author attach failed\n");
    return 1;
  }

  style::ResolveContext root;
  root.viewportW = 1280.0f;
  root.viewportH = 720.0f;

  style::StyleResolver resolver;
  resolver.resolveDocument(doc, root);

  lxb_dom_node_t* rootNode = lxb_dom_interface_node(doc);
  dumpTree(rootNode, resolver, 0);

  auto styleForId = [&](const char* id) -> const style::ComputedStyle* {
    lxb_dom_element_t* el = findById(rootNode, id);
    check(el != nullptr, "expected element id to exist");
    return el == nullptr ? nullptr : resolver.styleFor(lxb_dom_interface_node(el));
  };

  const style::ComputedStyle* title = styleForId("title");
  const style::ComputedStyle* para = styleForId("para");
  const style::ComputedStyle* box = styleForId("box");
  const style::ComputedStyle* text = styleForId("text");
  const style::ComputedStyle* child = styleForId("child");
  const style::ComputedStyle* pos = styleForId("pos");

  check(title != nullptr && near(title->text().font.sizePx, 32.0f),
        "h1 UA font-size resolves to 2em");
  check(title != nullptr && title->text().font.weight == 700,
        "h1 UA font-weight bold resolves to 700");

  check(para != nullptr && near(para->text().font.sizePx, 18.0f),
        "p author font-size resolves");
  check(para != nullptr && colorIs(para->text().color, 255, 0, 0),
        "inline color resolves");
  check(para != nullptr && lenPercent(para->box().width, 50.0f),
        "inline width percentage is preserved");
  check(para != nullptr && lenPx(para->surround().margin[style::kLeft], 36.0f),
        "inline margin-left em resolves against element font-size");

  check(box != nullptr && box->box().display == style::Display::Block,
        "box display block resolves");
  check(box != nullptr && lenPx(box->box().width, 100.0f),
        "box width resolves");
  check(box != nullptr && lenPx(box->box().height, 40.0f),
        "box height resolves");
  check(box != nullptr && colorIs(box->visual().background, 0, 128, 255),
        "box background-color resolves");
  check(box != nullptr && lenPx(box->surround().padding[style::kTop], 16.0f),
        "padding shorthand top resolves em");
  check(box != nullptr && lenPx(box->surround().padding[style::kRight], 2.0f),
        "padding shorthand right resolves px");
  check(box != nullptr &&
            box->surround().border[style::kTop].styleKind ==
                style::BorderStyle::Dashed,
        "border shorthand style resolves");
  check(box != nullptr &&
            lenPx(box->surround().border[style::kTop].width, 5.0f),
        "border thick resolves");
  check(box != nullptr &&
            colorIs(box->surround().border[style::kTop].color, 10, 20, 30),
        "border currentColor resolves");
  check(box != nullptr &&
            colorIs(box->surround().border[style::kLeft].color, 51, 102, 153),
        "border-left-color overrides shorthand color");
  check(box != nullptr && box->visual().overflowX == style::Overflow::Hidden,
        "overflow-x resolves");
  check(box != nullptr && box->visual().overflowY == style::Overflow::Scroll,
        "overflow-y resolves");

  check(text != nullptr && text->text().font.family == "monospace",
        "font-family generic resolves");
  check(text != nullptr && text->text().font.weight == 650,
        "numeric font-weight resolves");
  check(text != nullptr && text->text().font.style == style::FontStyle::Italic,
        "font-style resolves");
  check(text != nullptr && near(text->text().font.stretchPercent, 125.0f),
        "font-stretch percentage resolves");
  check(text != nullptr && text->text().textAlign == style::TextAlign::Center,
        "text-align resolves");
  check(text != nullptr && text->text().whiteSpace == style::WhiteSpace::PreWrap,
        "white-space resolves");
  check(text != nullptr && lenPx(text->text().textIndent, 32.0f),
        "text-indent em resolves");
  check(text != nullptr && lenPx(text->text().letterSpacing, 1.0f),
        "letter-spacing resolves");
  check(text != nullptr && lenPx(text->text().wordSpacing, 8.0f),
        "word-spacing em resolves");
  check(child != nullptr && child->text().textAlign == style::TextAlign::Center,
        "inherited text-align flows to child");
  check(child != nullptr && child->text().whiteSpace == style::WhiteSpace::PreWrap,
        "inherited white-space flows to child");

  check(pos != nullptr && pos->box().position == style::Position::Relative,
        "position resolves");
  check(pos != nullptr && lenPx(pos->surround().inset[style::kTop], 10.0f),
        "top inset resolves");
  check(pos != nullptr && lenPercent(pos->surround().inset[style::kLeft], 5.0f),
        "left inset percentage is preserved");
  check(pos != nullptr && pos->box().clear == style::Clear::Left,
        "clear resolves");
  check(pos != nullptr && pos->box().floatKind == style::Float::Left,
        "float resolves");
  check(pos != nullptr && pos->box().boxSizing == style::BoxSizing::BorderBox,
        "box-sizing resolves");
  check(pos != nullptr && !pos->box().zIndexAuto && pos->box().zIndex == 7,
        "z-index resolves");
  check(pos != nullptr && near(pos->visual().opacity, 0.5f),
        "opacity percentage resolves");

  if (failed) {
    std::printf("\nstyle_smoke failed\n");
    return 1;
  }

  std::printf("\nstyle_smoke passed\n");
  return 0;
}
