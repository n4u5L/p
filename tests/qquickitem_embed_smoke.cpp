#include <cstdio>
#include <cstring>

#include <QGuiApplication>
#include <QQuickItem>

#include "layout/layout.h"
#include "lexbor/css/property/const.h"
#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/html/parser.h"
#include "lexbor/style/style.h"

namespace {

bool failed = false;

void check(bool ok, const char *msg)
{
    if (!ok) {
        failed = true;
        std::printf("FAIL: %s\n", msg);
    }
}

lxb_dom_element_t *
findById(lxb_dom_node_t *node, const char *id)
{
    if (node == nullptr) {
        return nullptr;
    }

    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        lxb_dom_element_t *element = lxb_dom_interface_element(node);
        size_t len = 0;
        const lxb_char_t *value = lxb_dom_element_id(element, &len);

        if (value != nullptr && len == std::strlen(id)
            && std::memcmp(value, id, len) == 0) {
            return element;
        }
    }

    for (lxb_dom_node_t *child = node->first_child; child != nullptr;
         child = child->next) {
        if (lxb_dom_element_t *found = findById(child, id)) {
            return found;
        }
    }

    return nullptr;
}

layout_fragment_t *
makeFragment(layout_result_t *result, layout_object_t *object,
             double width, double height)
{
    layout_fragment_init_t init;

    std::memset(&init, 0, sizeof(init));
    init.object = object;
    init.size.width = width;
    init.size.height = height;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;

    return layout_fragment_create(result, &init);
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    const char *html =
        "<!doctype html><html><body>"
        "<div id='host'></div>"
        "</body></html>";
    constexpr layout_native_embed_token_t kHostToken = 1001;

    lxb_html_parser_t *parser = lxb_html_parser_create();
    if (parser == nullptr || lxb_html_parser_init(parser) != LXB_STATUS_OK) {
        std::printf("html parser init failed\n");
        return 1;
    }

    lxb_html_document_t *doc = lxb_html_parse(
        parser, reinterpret_cast<const lxb_char_t *>(html), std::strlen(html));
    if (doc == nullptr || lxb_style_init(doc) != LXB_STATUS_OK) {
        std::printf("html/style init failed\n");
        lxb_html_parser_destroy(parser);
        return 1;
    }

    lxb_dom_element_t *host_element =
        findById(lxb_dom_interface_node(doc), "host");
    check(host_element != nullptr, "html host element exists");

    lxb_style_resolver_t *resolver = lxb_style_resolver_create();
    check(resolver != nullptr, "style resolver allocates");
    if (resolver != nullptr) {
        check(lxb_style_resolver_init(resolver) == LXB_STATUS_OK,
              "style resolver initializes");
    }

    const lxb_style_computed_t *host_style = nullptr;
    if (resolver != nullptr && host_element != nullptr) {
        host_style = lxb_style_resolver_resolve_element(resolver,
                                                       host_element);
    }
    check(host_style != nullptr, "host element gets computed style");

    if (failed) {
        if (resolver != nullptr) {
            lxb_style_resolver_destroy(resolver, true);
        }
        lxb_style_destroy(doc);
        lxb_html_document_destroy(doc);
        lxb_html_parser_destroy(parser);
        return 1;
    }

    layout_t *layout = layout_create();
    check(layout != nullptr, "layout creates");
    if (layout != nullptr) {
        check(layout_init(layout) == LXB_STATUS_OK, "layout initializes");
    }

    layout_result_t *result = layout_result_create(layout);
    layout_object_t *root = layout_object_create_anonymous(layout, host_style);
    layout_object_t *host = layout_object_create_for_node(
        layout, lxb_dom_interface_node(host_element));
    layout_fragment_t *root_fragment = makeFragment(result, root, 320.0, 200.0);
    layout_fragment_t *host_fragment = makeFragment(result, host, 120.0, 44.0);
    layout_fragment_t *found_fragment = nullptr;

    check(result != nullptr, "layout result creates");
    check(root != nullptr, "root layout object creates");
    check(host != nullptr, "host layout object creates from html element");
    check(root_fragment != nullptr && host_fragment != nullptr,
          "layout fragments create");

    if (failed) {
        layout_result_destroy(result, true);
        layout_destroy(layout, true);
        lxb_style_resolver_destroy(resolver, true);
        lxb_style_destroy(doc);
        lxb_html_document_destroy(doc);
        lxb_html_parser_destroy(parser);
        return 1;
    }

    layout_object_set_native_embed_token(host, kHostToken);
    check(layout_object_is_native_embed_host(host),
          "host object is marked as native embed host");
    check(layout_object_native_embed_token(host) == kHostToken,
          "host object exposes native embed token");
    check(layout_object_set_parent(host, root) == LXB_STATUS_OK,
          "host layout parent is root object");
    check(layout_fragment_append_child(root_fragment, host_fragment,
                                       {48.0, 32.0})
              == LXB_STATUS_OK,
          "host fragment is placed by fragment link");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "layout result stores root fragment");
    check(layout_result_freeze(result) == LXB_STATUS_OK,
          "layout result freezes");

    check(layout_result_fragment_for_object(result, host, 0, &found_fragment)
              == LXB_STATUS_OK,
          "host object resolves to physical fragment");
    check(found_fragment == host_fragment,
          "resolved host fragment is the html element fragment");

    layout_rect_t local_rect = {0.0, 0.0, 120.0, 44.0};
    layout_rect_t root_rect = {0.0, 0.0, 0.0, 0.0};
    check(layout_result_fragment_rect_to_root(
              result, found_fragment, local_rect,
              LAYOUT_COORDINATE_APPLY_TRANSFORMS, &root_rect)
              == LXB_STATUS_OK,
          "host fragment maps to root coordinates");

    QQuickItem page;
    QQuickItem embedded;
    page.setWidth(320.0);
    page.setHeight(200.0);

    layout_native_embed_token_t item_token = kHostToken;
    check(item_token == layout_object_native_embed_token(host),
          "embedding layer maps item by layout token");

    embedded.setParentItem(&page);
    embedded.setX(root_rect.x);
    embedded.setY(root_rect.y);
    embedded.setWidth(root_rect.width);
    embedded.setHeight(root_rect.height);

    check(embedded.parentItem() == &page, "embedded item attaches outside C layout");
    check(embedded.x() == 48.0 && embedded.y() == 32.0,
          "embedded item position follows host fragment");
    check(embedded.width() == 120.0 && embedded.height() == 44.0,
          "embedded item size follows host fragment");
    check(layout_fragment_object(found_fragment) == host,
          "C layout fragment still owns only layout object identity");

    layout_result_destroy(result, true);
    layout_destroy(layout, true);
    if (resolver != nullptr) {
        lxb_style_resolver_destroy(resolver, true);
    }
    lxb_style_destroy(doc);
    lxb_html_document_destroy(doc);
    lxb_html_parser_destroy(parser);

    if (failed) {
        std::printf("\nqquickitem_embed_smoke failed\n");
        return 1;
    }

    std::printf("\nqquickitem_embed_smoke passed\n");
    return 0;
}
