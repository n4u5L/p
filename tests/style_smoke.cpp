// M1 样式引擎端到端冒烟测试：HTML 通过 lexbor 分块流式解析，
// <style> 在建树过程中进入样式系统，然后通过增量 StyleEngine/Resolver
// 解析 computed style 并断言关键计算值。
//
// 构建：
//   cmake --build build --target style_smoke
//
// 易错点：测试里会在 late stylesheet 之前读取一次 <p> 的样式，
// 但不要在后续 stylesheet 触发重算后继续持有旧 ComputedStyle*。
// 当前 arena 会保留旧对象一段时间，指针未必立刻失效，但语义上已经过期。

#include <cmath>
#include <cstdio>
#include <cstring>

#include "lexbor/css/css.h"
#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/html/html.h"
#include "lexbor/html/parser.h"
#include "lexbor/style/style.h"
#include "lexbor/style/html/interfaces/document.h"

#include "computed_style.h"
#include "resolver.h"
#include "style_engine.h"
#include "ua_stylesheet.h"

namespace {

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

const char* kHtmlPrefix = "<html><head><style>";
const char* kAuthorStyleClose = "</style>";
const char* kBodyPrefix =
    "</head><body>"
    "<h1 id='title'>Hello</h1>"
    "<p id='para' style='color:#ff0000; margin-left: 2em; width: 50%'>para</p>"
    "<div id='box' class='box'>boxed</div>";
const char* kLateStyle =
    "<style>#para { font-size: 22px; }"
    "#child { font-size: 20px; }</style>";
const char* kHtmlSuffix =
    "<section id='text' class='text'><span id='child'>child</span></section>"
    "<aside id='pos' class='pos'>pos</aside>"
    "</body></html>";

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

bool strEq(const char* a, const char* b) {
  return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
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
      std::printf("display=%d ", static_cast<int>(cs->Display()));
      std::printf("font-size=%.1f ", cs->Font().sizePx);
      std::printf("weight=%d ", cs->Font().weight);
      std::printf("color=#%02x%02x%02x ", cs->InheritedData().color.r,
                  cs->InheritedData().color.g, cs->InheritedData().color.b);
      std::printf("bg=#%02x%02x%02x%02x ",
                  cs->BackgroundData().backgroundColor.r,
                  cs->BackgroundData().backgroundColor.g,
                  cs->BackgroundData().backgroundColor.b,
                  cs->BackgroundData().backgroundColor.a);
      dumpLen("w", cs->BoxData().width);
      dumpLen("ml", cs->BoxData().margin[style::kLeft]);
      dumpLen("bt", cs->BoxData().borderWidth[style::kTop]);
    }
    std::printf("\n");
  }
  for (lxb_dom_node_t* c = node->first_child; c; c = c->next) {
    dumpTree(c, resolver, depth + 1);
  }
}

} // 匿名命名空间

int main() {
  lxb_html_parser_t* htmlParser = lxb_html_parser_create();
  if (htmlParser == nullptr || lxb_html_parser_init(htmlParser) != LXB_STATUS_OK) {
    std::printf("html parser init failed\n");
    return 1;
  }

  lxb_css_parser_t* parser = lxb_css_parser_create();
  if (lxb_css_parser_init(parser, nullptr) != LXB_STATUS_OK) {
    std::printf("css parser init failed\n");
    return 1;
  }

  lxb_html_document_t* doc = lxb_html_parse_chunk_begin(htmlParser);
  if (!doc || lxb_style_init(doc) != LXB_STATUS_OK) {
    std::printf("init failed\n");
    return 1;
  }

  if (!style::attachUaStylesheet(doc, parser)) {
    std::printf("UA stylesheet attach failed\n");
    return 1;
  }

  style::ResolveContext root;
  root.viewportW = 1280.0f;
  root.viewportH = 720.0f;

  style::StyleEngine engine(doc, root);
  style::StyleResolver& resolver = engine.resolver();

  auto parseChunk = [&](const char* data) -> bool {
    if (lxb_html_parse_chunk_process(
            htmlParser, reinterpret_cast<const lxb_char_t*>(data),
            std::strlen(data)) != LXB_STATUS_OK) {
      return false;
    }
    engine.update();
    return true;
  };

  if (!parseChunk(kHtmlPrefix) || !parseChunk(kAuthorCss) ||
      !parseChunk(kAuthorStyleClose) || !parseChunk(kBodyPrefix)) {
    std::printf("html chunk parse failed\n");
    return 1;
  }

  const style::ComputedStyle* paraBeforeLate = nullptr;
  {
    lxb_dom_node_t* rootNode = lxb_dom_interface_node(doc);
    lxb_dom_element_t* para = findById(rootNode, "para");
    paraBeforeLate =
        para == nullptr ? nullptr : resolver.styleFor(lxb_dom_interface_node(para));
  }
  check(paraBeforeLate != nullptr &&
            colorIs(paraBeforeLate->InheritedData().color, 255, 0, 0),
        "pre-late inline color resolves before later stylesheet");
  check(paraBeforeLate != nullptr &&
            near(paraBeforeLate->Font().sizePx, 18.0f),
        "pre-late author font-size resolves before later stylesheet");
  paraBeforeLate = nullptr;

  if (!parseChunk(kLateStyle) || !parseChunk(kHtmlSuffix) ||
      lxb_html_parse_chunk_end(htmlParser) != LXB_STATUS_OK) {
    std::printf("html chunk parse failed\n");
    return 1;
  }
  engine.update();

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

  check(title != nullptr && near(title->Font().sizePx, 32.0f),
        "h1 UA font-size resolves to 2em");
  check(title != nullptr && title->Font().weight == 700,
        "h1 UA font-weight bold resolves to 700");

  check(para != nullptr && near(para->Font().sizePx, 22.0f),
        "late stylesheet invalidates and re-resolves previous element");
  check(para != nullptr && colorIs(para->InheritedData().color, 255, 0, 0),
        "inline color resolves");
  check(para != nullptr && lenPercent(para->BoxData().width, 50.0f),
        "inline width percentage is preserved");
  check(para != nullptr && lenPx(para->BoxData().margin[style::kLeft], 44.0f),
        "inline margin-left em resolves against element font-size");

  check(box != nullptr && box->Display() == LXB_CSS_DISPLAY_BLOCK,
        "box display block resolves");
  check(box != nullptr && lenPx(box->BoxData().width, 100.0f),
        "box width resolves");
  check(box != nullptr && lenPx(box->BoxData().height, 40.0f),
        "box height resolves");
  check(box != nullptr &&
            colorIs(box->BackgroundData().backgroundColor, 0, 128, 255),
        "box background-color resolves");
  check(box != nullptr && lenPx(box->BoxData().padding[style::kTop], 16.0f),
        "padding shorthand top resolves em");
  check(box != nullptr && lenPx(box->BoxData().padding[style::kRight], 2.0f),
        "padding shorthand right resolves px");
  check(box != nullptr &&
            box->BoxData().borderStyle[style::kTop] ==
                LXB_CSS_BORDER_DASHED,
        "border shorthand style resolves");
  check(box != nullptr &&
            lenPx(box->BoxData().borderWidth[style::kTop], 5.0f),
        "border thick resolves");
  check(box != nullptr &&
            colorIs(box->SurroundData().borderColor[style::kTop], 10, 20, 30),
        "border currentColor resolves");
  check(box != nullptr &&
            colorIs(box->SurroundData().borderColor[style::kLeft], 51, 102, 153),
        "border-left-color overrides shorthand color");
  check(box != nullptr && box->OverflowX() == LXB_CSS_OVERFLOW_X_HIDDEN,
        "overflow-x resolves");
  check(box != nullptr && box->OverflowY() == LXB_CSS_OVERFLOW_Y_SCROLL,
        "overflow-y resolves");

  check(text != nullptr && strEq(text->Font().family, "monospace"),
        "font-family generic resolves");
  check(text != nullptr && text->Font().weight == 650,
        "numeric font-weight resolves");
  check(text != nullptr && text->Font().style == LXB_CSS_FONT_STYLE_ITALIC,
        "font-style resolves");
  check(text != nullptr && near(text->Font().stretchPercent, 125.0f),
        "font-stretch percentage resolves");
  check(text != nullptr && text->TextAlign() == LXB_CSS_TEXT_ALIGN_CENTER,
        "text-align resolves");
  check(text != nullptr && text->WhiteSpace() == LXB_CSS_WHITE_SPACE_PRE_WRAP,
        "white-space resolves");
  check(text != nullptr && lenPx(text->RareInheritedData().textIndent, 32.0f),
        "text-indent em resolves");
  check(text != nullptr && lenPx(text->InheritedData().letterSpacing, 1.0f),
        "letter-spacing resolves");
  check(text != nullptr && lenPx(text->InheritedData().wordSpacing, 8.0f),
        "word-spacing em resolves");
  check(child != nullptr && child->TextAlign() == LXB_CSS_TEXT_ALIGN_CENTER,
        "inherited text-align flows to child");
  check(child != nullptr && child->WhiteSpace() == LXB_CSS_WHITE_SPACE_PRE_WRAP,
        "inherited white-space flows to child");
  check(child != nullptr && near(child->Font().sizePx, 20.0f),
        "late stylesheet applies to later inserted element");

  check(pos != nullptr && pos->Position() == LXB_CSS_POSITION_RELATIVE,
        "position resolves");
  check(pos != nullptr && lenPx(pos->SurroundData().inset[style::kTop], 10.0f),
        "top inset resolves");
  check(pos != nullptr &&
            lenPercent(pos->SurroundData().inset[style::kLeft], 5.0f),
        "left inset percentage is preserved");
  check(pos != nullptr && pos->Clear() == LXB_CSS_CLEAR_LEFT,
        "clear resolves");
  check(pos != nullptr && pos->VisualData().floating == LXB_CSS_FLOAT_LEFT,
        "float resolves");
  check(pos != nullptr && pos->BoxData().boxSizing == LXB_CSS_BOX_SIZING_BORDER_BOX,
        "box-sizing resolves");
  check(pos != nullptr && !pos->BoxData().zIndexAuto &&
            pos->BoxData().zIndex == 7,
        "z-index resolves");
  check(pos != nullptr && near(pos->SvgData().opacity, 0.5f),
        "opacity percentage resolves");

  if (failed) {
    std::printf("\nstyle_smoke failed\n");
    return 1;
  }

  std::printf("\nstyle_smoke passed\n");
  return 0;
}
