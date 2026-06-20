#include <cmath>
#include <cstdio>
#include <cstring>

#include "lexbor/css/property/const.h"
#include "lexbor/css/unit/const.h"
#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/html/parser.h"
#include "lexbor/style/style.h"

namespace {

bool failed = false;

void check(bool ok, const char* msg) {
  if (!ok) {
    failed = true;
    std::printf("FAIL: %s\n", msg);
  }
}

bool near(double a, double b) {
  return std::fabs(a - b) < 0.01;
}

bool colorIs(const lxb_style_color_t& color, unsigned r, unsigned g,
             unsigned b, unsigned a = 255) {
  return near(color.r, static_cast<double>(r) / 255.0) &&
         near(color.g, static_cast<double>(g) / 255.0) &&
         near(color.b, static_cast<double>(b) / 255.0) &&
         near(color.a, static_cast<double>(a) / 255.0);
}

bool valueLengthPx(const lxb_style_computed_value_t& value, double px) {
  return value.type == LXB_STYLE_COMPUTED_VALUE_LENGTH &&
         static_cast<uintptr_t>(value.u.length.unit) == LXB_CSS_UNIT_PX &&
         near(value.u.length.num, px);
}

bool valuePercent(const lxb_style_computed_value_t& value, double percent) {
  return value.type == LXB_STYLE_COMPUTED_VALUE_PERCENTAGE &&
         near(value.u.number, percent);
}

lxb_dom_element_t* findById(lxb_dom_node_t* node, const char* id) {
  if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    size_t len = 0;
    const lxb_char_t* value = lxb_dom_element_id(element, &len);

    if (value != nullptr && len == std::strlen(id) &&
        std::memcmp(value, id, len) == 0) {
      return element;
    }
  }

  for (lxb_dom_node_t* child = node->first_child; child != nullptr;
       child = child->next) {
    if (lxb_dom_element_t* found = findById(child, id)) {
      return found;
    }
  }

  return nullptr;
}

bool parseInline(lxb_dom_element_t* element, const char* style) {
  return lxb_dom_element_style_parse(
             element, reinterpret_cast<const lxb_char_t*>(style),
             std::strlen(style)) == LXB_STATUS_OK;
}

const lxb_style_computed_t* styleForId(lxb_style_resolver_t* resolver,
                                       lxb_dom_node_t* root,
                                       const char* id) {
  lxb_dom_element_t* element = findById(root, id);
  check(element != nullptr, "expected element id to exist");

  if (element == nullptr) {
    return nullptr;
  }

  const lxb_style_computed_t* style =
      lxb_style_resolver_style_by_element(resolver, element);
  if (style == nullptr) {
    style = lxb_style_resolver_resolve_element(resolver, element);
  }

  check(style != nullptr, "expected computed style to exist");
  return style;
}

}  // namespace

int main() {
  const char* html =
      "<!doctype html>"
      "<html><body>"
      "<section id='parent'><p id='child'>child</p></section>"
      "<div id='box'>box</div>"
      "<aside id='pos'>pos</aside>"
      "</body></html>";

  lxb_html_parser_t* parser = lxb_html_parser_create();
  if (parser == nullptr || lxb_html_parser_init(parser) != LXB_STATUS_OK) {
    std::printf("html parser init failed\n");
    return 1;
  }

  lxb_html_document_t* doc = lxb_html_parse(
      parser, reinterpret_cast<const lxb_char_t*>(html), std::strlen(html));
  if (doc == nullptr || lxb_style_init(doc) != LXB_STATUS_OK) {
    std::printf("html/style init failed\n");
    lxb_html_parser_destroy(parser);
    return 1;
  }

  lxb_dom_node_t* root = lxb_dom_interface_node(doc);
  lxb_dom_element_t* parent = findById(root, "parent");
  lxb_dom_element_t* child = findById(root, "child");
  lxb_dom_element_t* box = findById(root, "box");
  lxb_dom_element_t* pos = findById(root, "pos");

  check(parent != nullptr, "parent element exists");
  check(child != nullptr, "child element exists");
  check(box != nullptr, "box element exists");
  check(pos != nullptr, "pos element exists");

  if (failed) {
    lxb_style_destroy(doc);
    lxb_html_document_destroy(doc);
    lxb_html_parser_destroy(parser);
    return 1;
  }

  check(parseInline(parent,
                    "color: rgb(10,20,30); font-size: 20px; "
                    "text-align: center; white-space: pre-wrap;"),
        "parent inline style parses");
  check(parseInline(child, "width: 50%; margin-left: 2em;"),
        "child inline style parses");
  check(parseInline(box,
                    "display: block; color: rgb(10,20,30); width: 100px; "
                    "height: 40px; background-color: rgb(0,128,255); "
                    "border: thick dashed currentColor; "
                    "border-left-color: #336699; padding: 1em 2px; "
                    "overflow-x: hidden; overflow-y: scroll;"),
        "box inline style parses");
  check(parseInline(pos,
                    "position: relative; top: 10px; left: 5%; clear: left; "
                    "z-index: 7; box-sizing: border-box; float: left; "
                    "opacity: 50%;"),
        "position inline style parses");

  if (failed) {
    lxb_style_destroy(doc);
    lxb_html_document_destroy(doc);
    lxb_html_parser_destroy(parser);
    return 1;
  }

  lxb_style_resolver_t* resolver = lxb_style_resolver_create();
  if (resolver == nullptr || lxb_style_resolver_init(resolver) != LXB_STATUS_OK) {
    std::printf("style resolver init failed\n");
    lxb_style_resolver_destroy(resolver, true);
    lxb_style_destroy(doc);
    lxb_html_document_destroy(doc);
    lxb_html_parser_destroy(parser);
    return 1;
  }

  lxb_style_resolve_context_t ctx = {1280.0, 720.0, 16.0, 19.2};
  lxb_style_resolver_context_set(resolver, &ctx);

  check(lxb_style_resolver_resolve_document(resolver, doc) == LXB_STATUS_OK,
        "document resolves");

  const lxb_style_computed_t* parent_style =
      styleForId(resolver, root, "parent");
  const lxb_style_computed_t* child_style = styleForId(resolver, root, "child");
  const lxb_style_computed_t* box_style = styleForId(resolver, root, "box");
  const lxb_style_computed_t* pos_style = styleForId(resolver, root, "pos");

  check(parent_style != nullptr &&
            colorIs(parent_style->inherited->color, 10, 20, 30),
        "parent color resolves");
  check(parent_style != nullptr &&
            near(parent_style->inherited->font_size, 20.0),
        "parent font-size resolves");
  check(parent_style != nullptr &&
            parent_style->inherited->rare->text_align ==
                LXB_CSS_TEXT_ALIGN_CENTER,
        "parent text-align resolves");
  check(parent_style != nullptr &&
            parent_style->inherited->rare->white_space ==
                LXB_CSS_WHITE_SPACE_PRE_WRAP,
        "parent white-space resolves");

  check(child_style != nullptr &&
            colorIs(child_style->inherited->color, 10, 20, 30),
        "child inherits color");
  check(child_style != nullptr &&
            near(child_style->inherited->font_size, 20.0),
        "child inherits font-size");
  check(child_style != nullptr &&
            child_style->inherited->rare->text_align ==
                LXB_CSS_TEXT_ALIGN_CENTER,
        "child inherits text-align");
  check(child_style != nullptr &&
            child_style->inherited->rare->white_space ==
                LXB_CSS_WHITE_SPACE_PRE_WRAP,
        "child inherits white-space");
  check(child_style != nullptr && valuePercent(child_style->box->width, 50.0),
        "child width percentage is preserved");
  check(child_style != nullptr &&
            valueLengthPx(child_style->surround->margin[LXB_STYLE_EDGE_LEFT],
                          40.0),
        "child margin-left em resolves against inherited font-size");

  check(box_style != nullptr &&
            box_style->non_inherited->display_outside == LXB_CSS_DISPLAY_BLOCK,
        "box display block resolves");
  check(box_style != nullptr &&
            box_style->non_inherited->display_inside == LXB_CSS_DISPLAY_FLOW,
        "box display flow resolves");
  check(box_style != nullptr && valueLengthPx(box_style->box->width, 100.0),
        "box width resolves");
  check(box_style != nullptr && valueLengthPx(box_style->box->height, 40.0),
        "box height resolves");
  check(box_style != nullptr &&
            colorIs(box_style->visual->background_color, 0, 128, 255),
        "box background-color resolves");
  check(box_style != nullptr &&
            valueLengthPx(box_style->surround->padding[LXB_STYLE_EDGE_TOP],
                          16.0),
        "box padding top em resolves");
  check(box_style != nullptr &&
            valueLengthPx(box_style->surround->padding[LXB_STYLE_EDGE_RIGHT],
                          2.0),
        "box padding right resolves");
  check(box_style != nullptr &&
            box_style->surround->border[LXB_STYLE_EDGE_TOP].style ==
                LXB_CSS_BORDER_DASHED,
        "box border style resolves");
  check(box_style != nullptr &&
            valueLengthPx(box_style->surround->border[LXB_STYLE_EDGE_TOP].width,
                          5.0),
        "box border thick resolves");
  check(box_style != nullptr &&
            colorIs(box_style->surround->border[LXB_STYLE_EDGE_TOP].color, 10,
                    20, 30),
        "box border currentColor resolves");
  check(box_style != nullptr &&
            colorIs(box_style->surround->border[LXB_STYLE_EDGE_LEFT].color, 51,
                    102, 153),
        "box border-left-color overrides shorthand");
  check(box_style != nullptr &&
            box_style->non_inherited->overflow_x == LXB_CSS_OVERFLOW_X_HIDDEN,
        "box overflow-x resolves");
  check(box_style != nullptr &&
            box_style->non_inherited->overflow_y == LXB_CSS_OVERFLOW_Y_SCROLL,
        "box overflow-y resolves");

  check(pos_style != nullptr &&
            pos_style->non_inherited->position == LXB_CSS_POSITION_RELATIVE,
        "position resolves");
  check(pos_style != nullptr &&
            valueLengthPx(pos_style->box->inset[LXB_STYLE_EDGE_TOP], 10.0),
        "top inset resolves");
  check(pos_style != nullptr &&
            valuePercent(pos_style->box->inset[LXB_STYLE_EDGE_LEFT], 5.0),
        "left inset percentage is preserved");
  check(pos_style != nullptr &&
            pos_style->non_inherited->clear == LXB_CSS_CLEAR_LEFT,
        "clear resolves");
  check(pos_style != nullptr &&
            pos_style->non_inherited->floatp == LXB_CSS_FLOAT_LEFT,
        "float resolves");
  check(pos_style != nullptr &&
            pos_style->non_inherited->box_sizing ==
                LXB_CSS_BOX_SIZING_BORDER_BOX,
        "box-sizing resolves");
  check(pos_style != nullptr && !pos_style->non_inherited->z_index_auto &&
            pos_style->non_inherited->z_index == 7,
        "z-index resolves");
  check(pos_style != nullptr && near(pos_style->non_inherited->opacity, 0.5),
        "opacity percentage resolves");

  check(parseInline(child, "font-size: 24px; width: 50%; margin-left: 1em;"),
        "child replacement inline style parses");
  lxb_style_resolver_invalidate_subtree(resolver, child);

  const lxb_style_computed_t* updated_child =
      lxb_style_resolver_resolve_element(resolver, child);
  const lxb_style_computed_t* unchanged_parent =
      lxb_style_resolver_style_by_element(resolver, parent);

  check(updated_child != nullptr &&
            near(updated_child->inherited->font_size, 24.0),
        "invalidate re-resolves changed child font-size");
  check(updated_child != nullptr &&
            valueLengthPx(
                updated_child->surround->margin[LXB_STYLE_EDGE_LEFT], 24.0),
        "invalidate re-resolves changed child em margin");
  check(unchanged_parent == parent_style,
        "child invalidation does not dirty parent style entry");
  check(unchanged_parent != nullptr &&
            near(unchanged_parent->inherited->font_size, 20.0),
        "parent computed style remains unchanged");

  lxb_style_resolver_destroy(resolver, true);
  lxb_style_destroy(doc);
  lxb_html_document_destroy(doc);
  lxb_html_parser_destroy(parser);

  if (failed) {
    std::printf("\nstyle_smoke failed\n");
    return 1;
  }

  std::printf("\nstyle_smoke passed\n");
  return 0;
}
