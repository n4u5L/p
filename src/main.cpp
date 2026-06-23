#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QColor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointF>
#include <QQuickWindow>
#include <QRectF>
#include <QSGNode>
#include <QSGSimpleRectNode>
#include <QSGTransformNode>
#include <QString>

#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/html/interface.h"
#include "lexbor/html/interfaces/body_element.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/html/parser.h"
#include "lexbor/style/style.h"

#include "layout/layout.h"
#include "qt_adaptor/layout_scene_adapter.h"

namespace {

constexpr double kViewportWidth = 900.0;
constexpr double kViewportHeight = 640.0;

QString statusToString(lxb_status_t status)
{
    return QStringLiteral("0x%1").arg(static_cast<unsigned>(status), 0, 16);
}

bool keyEqual(layout_fragment_key_t left, layout_fragment_key_t right)
{
    return left.object_id == right.object_id
        && left.fragment_index == right.fragment_index
        && left.role == right.role
        && left.ordinal == right.ordinal;
}

std::string makeHtml(bool expanded)
{
    const char *buttonStyle = expanded
        ? "display:block; width:260px; height:58px; margin:12px 0 0 0; "
          "padding:12px 16px; background-color:rgb(32,129,99); "
          "border:2px solid rgb(16,80,62);"
        : "display:block; width:190px; height:48px; margin:12px 0 0 0; "
          "padding:10px 14px; background-color:rgb(41,112,184); "
          "border:2px solid rgb(22,72,130);";

    const char *panelStyle = expanded
        ? "display:block; position:relative; width:68%; min-height:180px; "
          "margin:18px 0 0 28px; padding:18px; "
          "background-color:rgb(238,246,242); "
          "border:2px solid rgb(124,172,150); overflow:hidden;"
        : "display:block; position:relative; width:58%; min-height:138px; "
          "margin:18px 0 0 28px; padding:16px; "
          "background-color:rgb(245,248,252); "
          "border:2px solid rgb(143,170,205); overflow:hidden;";

    std::string html;
    html.reserve(2400);
    html += "<!doctype html><html><body id='viewport' style='display:block; "
            "width:900px; min-height:640px; margin:0; padding:24px; "
            "background-color:rgb(248,248,246);'>";
    html += "<main id='page' style='display:block; position:relative; "
            "width:820px; min-height:540px; padding:24px; "
            "background-color:rgb(255,255,255); "
            "border:2px solid rgb(34,38,44); overflow:hidden;'>";
    html += "<section id='hero' style='display:block; width:72%; height:96px; "
            "margin:0; padding:20px; background-color:rgb(232,239,245); "
            "border:2px solid rgb(76,99,122);'>";
    html += "<div id='button' style='";
    html += buttonStyle;
    html += "'></div>";
    html += "</section>";
    html += "<section id='panel' style='";
    html += panelStyle;
    html += "'>";
    html += "<div id='meter' style='display:block; width:";
    html += expanded ? "82%" : "54%";
    html += "; height:28px; margin:8px 0 0 0; "
            "background-color:rgb(214,175,65); "
            "border:1px solid rgb(120,86,24);'></div>";
    html += "<div id='absolute' style='display:block; position:absolute; "
            "width:128px; height:46px; margin:0; padding:8px; "
            "background-color:rgb(168,82,92); "
            "border:2px solid rgb(94,42,50);'></div>";
    html += "</section>";
    html += "<aside id='badge' style='display:block; position:fixed; "
            "width:132px; height:40px; margin:14px 0 0 580px; padding:8px; "
            "background-color:rgb(43,45,54); "
            "border:2px solid rgb(92,95,112);'></aside>";
    html += "</main></body></html>";
    return html;
}

std::string elementId(lxb_dom_node_t *node)
{
    if (node == nullptr || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return {};
    }

    size_t len = 0;
    lxb_dom_element_t *element = lxb_dom_interface_element(node);
    const lxb_char_t *value = lxb_dom_element_id(element, &len);
    if (value == nullptr || len == 0) {
        return {};
    }

    return std::string(reinterpret_cast<const char *>(value), len);
}

QColor colorFromStyle(const lxb_style_color_t &color)
{
    const auto channel = [](float value) {
        const double clamped = std::clamp(static_cast<double>(value), 0.0, 1.0);
        return static_cast<int>(std::lround(clamped * 255.0));
    };

    return QColor(channel(color.r), channel(color.g), channel(color.b),
                  channel(color.a));
}

QColor fillForObject(layout_object_t *object, size_t fallbackIndex)
{
    if (object == nullptr) {
        return QColor::fromHsv(static_cast<int>((fallbackIndex * 47) % 360),
                               44, 225, 220);
    }

    const lxb_style_computed_t *style = layout_object_style(object);
    if (style != nullptr && style->visual != nullptr
        && style->visual->background_color.a > 0.0f) {
        QColor color = colorFromStyle(style->visual->background_color);
        color.setAlpha(std::max(color.alpha(), 220));
        return color;
    }

    return QColor::fromHsv(static_cast<int>((fallbackIndex * 47) % 360),
                           34, 236, 210);
}

struct SceneVisual {
    layout_fragment_key_t key {};
    QRectF localRect;
    QColor fill;
};

struct DemoBundle {
    ~DemoBundle()
    {
        plan = layout_scene_plan_destroy(plan, true);
        result = layout_result_destroy(result, true);
        tree = layout_tree_destroy(tree, true);
        layout = layout_destroy(layout, true);

        if (resolver != nullptr) {
            resolver = lxb_style_resolver_destroy(resolver, true);
        }
        if (doc != nullptr) {
            if (styleInitialized) {
                lxb_style_destroy(doc);
            }
            doc = lxb_html_document_destroy(doc);
        }
        if (parser != nullptr) {
            parser = lxb_html_parser_destroy(parser);
        }
    }

    std::string html;
    lxb_html_parser_t *parser = nullptr;
    lxb_html_document_t *doc = nullptr;
    lxb_style_resolver_t *resolver = nullptr;
    layout_t *layout = nullptr;
    layout_tree_t *tree = nullptr;
    layout_result_t *result = nullptr;
    layout_scene_plan_t *plan = nullptr;
    std::unordered_map<layout_object_t *, std::string> idsByObject;
    std::vector<SceneVisual> visuals;
    QString error;
    bool expanded = false;
    bool styleInitialized = false;
};

bool failBundle(DemoBundle &bundle, QString message)
{
    bundle.error = std::move(message);
    std::fprintf(stderr, "%s\n", bundle.error.toLocal8Bit().constData());
    return false;
}

void collectObjectIds(DemoBundle &bundle, lxb_dom_node_t *node)
{
    if (node == nullptr) {
        return;
    }

    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        std::string id = elementId(node);
        if (!id.empty()) {
            layout_object_t *object =
                layout_tree_object_for_node(bundle.tree, node);
            if (object != nullptr) {
                bundle.idsByObject.emplace(object, std::move(id));
            }
        }
    }

    for (lxb_dom_node_t *child = node->first_child; child != nullptr;
         child = child->next) {
        collectObjectIds(bundle, child);
    }
}

QColor colorForSceneNode(DemoBundle &bundle,
                         layout_fragment_key_t key,
                         size_t fallbackIndex)
{
    for (const auto &[object, id] : bundle.idsByObject) {
        (void) id;

        layout_fragment_t *candidate = nullptr;
        if (layout_result_fragment_for_object(bundle.result, object, 0,
                                              &candidate)
                != LXB_STATUS_OK
            || candidate == nullptr) {
            continue;
        }

        if (keyEqual(layout_fragment_key(candidate), key)) {
            return fillForObject(object, fallbackIndex);
        }
    }

    return fillForObject(nullptr, fallbackIndex);
}

void buildSceneVisuals(DemoBundle &bundle)
{
    bundle.visuals.clear();

    const size_t count = layout_scene_plan_node_count(bundle.plan);
    bundle.visuals.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const layout_scene_node_t *node =
            layout_scene_plan_node_at(bundle.plan, i);
        if (node == nullptr) {
            continue;
        }
        if ((layout_scene_node_flags(node) & LAYOUT_SCENE_NODE_VISIBLE) == 0) {
            continue;
        }

        layout_rect_t local = layout_scene_node_local_rect(node);
        if (local.width <= 0.0 || local.height <= 0.0) {
            continue;
        }

        SceneVisual visual;
        visual.key = layout_scene_node_key(node);
        visual.localRect =
            QRectF(local.x, local.y, local.width, local.height);
        visual.fill = colorForSceneNode(bundle, visual.key, i);
        bundle.visuals.push_back(visual);
    }
}

bool buildBundle(DemoBundle &bundle, bool expanded)
{
    bundle.expanded = expanded;
    bundle.html = makeHtml(expanded);

    bundle.parser = lxb_html_parser_create();
    if (bundle.parser == nullptr
        || lxb_html_parser_init(bundle.parser) != LXB_STATUS_OK) {
        return failBundle(bundle,
                          QStringLiteral("Lexbor HTML parser init failed"));
    }

    bundle.doc = lxb_html_parse(
        bundle.parser, reinterpret_cast<const lxb_char_t *>(bundle.html.data()),
        bundle.html.size());
    if (bundle.doc == nullptr) {
        return failBundle(bundle, QStringLiteral("Lexbor HTML parse failed"));
    }
    if (lxb_style_init(bundle.doc) != LXB_STATUS_OK) {
        return failBundle(bundle, QStringLiteral("Lexbor style init failed"));
    }
    bundle.styleInitialized = true;

    bundle.resolver = lxb_style_resolver_create();
    if (bundle.resolver == nullptr
        || lxb_style_resolver_init(bundle.resolver) != LXB_STATUS_OK) {
        return failBundle(bundle,
                          QStringLiteral("Lexbor style resolver init failed"));
    }

    lxb_style_resolve_context_t ctx = {
        kViewportWidth,
        kViewportHeight,
        16.0,
        19.2,
    };
    lxb_style_resolver_context_set(bundle.resolver, &ctx);

    lxb_status_t status =
        lxb_style_resolver_resolve_document(bundle.resolver, bundle.doc);
    if (status != LXB_STATUS_OK) {
        return failBundle(bundle,
                          QStringLiteral("Style resolve failed: %1")
                              .arg(statusToString(status)));
    }

    lxb_html_body_element_t *body = lxb_html_document_body_element(bundle.doc);
    if (body == nullptr) {
        return failBundle(bundle, QStringLiteral("Parsed document has no body"));
    }

    bundle.layout = layout_create();
    if (bundle.layout == nullptr || layout_init(bundle.layout) != LXB_STATUS_OK) {
        return failBundle(bundle, QStringLiteral("layout_create/init failed"));
    }

    bundle.tree = layout_tree_create(bundle.layout);
    if (bundle.tree == nullptr) {
        return failBundle(bundle, QStringLiteral("layout_tree_create failed"));
    }

    lxb_dom_node_t *bodyNode = lxb_dom_interface_node(body);
    status = layout_tree_build(bundle.tree, bodyNode);
    if (status != LXB_STATUS_OK) {
        return failBundle(bundle,
                          QStringLiteral("layout_tree_build failed: %1")
                              .arg(statusToString(status)));
    }

    bundle.result = layout_result_create(bundle.layout);
    if (bundle.result == nullptr) {
        return failBundle(bundle, QStringLiteral("layout_result_create failed"));
    }

    layout_fragment_builder_t builder {};
    builder.viewport_size = {kViewportWidth, kViewportHeight};
    builder.default_object_size = {80.0, 44.0};
    builder.default_block_gap = 10.0;

    status = layout_tree_build_fragments(bundle.tree, bundle.result, &builder);
    if (status != LXB_STATUS_OK) {
        return failBundle(bundle,
                          QStringLiteral("layout_tree_build_fragments failed: %1")
                              .arg(statusToString(status)));
    }

    status = layout_result_freeze(bundle.result);
    if (status != LXB_STATUS_OK) {
        return failBundle(bundle,
                          QStringLiteral("layout_result_freeze failed: %1")
                              .arg(statusToString(status)));
    }

    bundle.plan = layout_scene_plan_create(bundle.layout);
    if (bundle.plan == nullptr) {
        return failBundle(bundle,
                          QStringLiteral("layout_scene_plan_create failed"));
    }

    status = layout_scene_plan_build(bundle.plan, bundle.result);
    if (status != LXB_STATUS_OK) {
        return failBundle(bundle,
                          QStringLiteral("layout_scene_plan_build failed: %1")
                              .arg(statusToString(status)));
    }

    collectObjectIds(bundle, bodyNode);
    buildSceneVisuals(bundle);
    return true;
}

class HtmlDemoItem final : public myqt::LayoutSceneItem {
public:
    explicit HtmlDemoItem(QQuickItem *parent = nullptr)
        : myqt::LayoutSceneItem(parent)
    {
        setAcceptedMouseButtons(Qt::LeftButton);
    }

    bool initialize()
    {
        return rebuild(false);
    }

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode,
                             UpdatePaintNodeData *data) override
    {
        QSGNode *root = myqt::LayoutSceneItem::updatePaintNode(oldNode, data);

        const uint64_t generation = adapter()->generation();
        if (generation != renderedGeneration_) {
            rectNodes_.clear();
            renderedGeneration_ = generation;
        }

        syncVisualNodes(currentBundle());
        return root;
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        DemoBundle *bundle = currentBundle();
        if (bundle == nullptr || bundle->result == nullptr
            || layout_result_root_fragment(bundle->result) == nullptr) {
            event->ignore();
            return;
        }

        layout_point_t rootPoint {
            event->position().x(),
            event->position().y(),
        };

        layout_fragment_t *hit = nullptr;
        if (layout_hit_test(bundle->result,
                            layout_result_root_fragment(bundle->result),
                            rootPoint, &hit)
                != LXB_STATUS_OK
            || hit == nullptr) {
            hasHighlightedKey_ = false;
            update();
            event->accept();
            return;
        }

        highlightedKey_ = layout_fragment_key(hit);
        hasHighlightedKey_ = true;

        bool toggle = false;
        layout_object_t *object = layout_fragment_object(hit);
        if (object != nullptr) {
            auto idIt = bundle->idsByObject.find(object);
            if (idIt != bundle->idsByObject.end()) {
                toggle = idIt->second == "button" || idIt->second == "hero"
                    || idIt->second == "panel";
            }
        }

        if (toggle) {
            rebuild(!expanded_);
        }
        else {
            update();
        }

        event->accept();
    }

private:
    using RectNodeMap =
        std::unordered_map<layout_fragment_key_t,
                           QSGSimpleRectNode *,
                           myqt::LayoutFragmentKeyHash,
                           myqt::LayoutFragmentKeyEqual>;

    DemoBundle *currentBundle() const
    {
        return bundles_.empty() ? nullptr : bundles_.back().get();
    }

    bool rebuild(bool expanded)
    {
        auto bundle = std::make_unique<DemoBundle>();
        if (!buildBundle(*bundle, expanded)) {
            bundles_.push_back(std::move(bundle));
            update();
            return false;
        }

        expanded_ = expanded;
        hasHighlightedKey_ = false;

        DemoBundle *raw = bundle.get();
        bundles_.push_back(std::move(bundle));
        scheduleRebuild(raw->plan);
        return true;
    }

    void syncVisualNodes(DemoBundle *bundle)
    {
        if (bundle == nullptr || bundle->plan == nullptr) {
            return;
        }

        for (const SceneVisual &visual : bundle->visuals) {
            QSGTransformNode *parentNode = adapter()->qsgNodeForKey(visual.key);
            if (parentNode == nullptr) {
                continue;
            }

            QSGSimpleRectNode *rectNode = nullptr;
            auto it = rectNodes_.find(visual.key);
            if (it == rectNodes_.end()) {
                rectNode = new QSGSimpleRectNode();
                rectNodes_.emplace(visual.key, rectNode);
            }
            else {
                rectNode = it->second;
            }

            QColor fill = visual.fill;
            if (hasHighlightedKey_ && keyEqual(visual.key, highlightedKey_)) {
                fill = fill.lighter(118);
                fill.setAlpha(255);
            }

            rectNode->setRect(visual.localRect);
            rectNode->setColor(fill);

            if (rectNode->parent() != parentNode) {
                if (rectNode->parent() != nullptr) {
                    rectNode->parent()->removeChildNode(rectNode);
                }
                parentNode->prependChildNode(rectNode);
            }
            else if (parentNode->firstChild() != rectNode) {
                parentNode->removeChildNode(rectNode);
                parentNode->prependChildNode(rectNode);
            }
        }
    }

    std::vector<std::unique_ptr<DemoBundle>> bundles_;
    RectNodeMap rectNodes_;
    layout_fragment_key_t highlightedKey_ {};
    uint64_t renderedGeneration_ = 0;
    bool hasHighlightedKey_ = false;
    bool expanded_ = false;
};

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QQuickWindow window;
    window.setTitle(QStringLiteral("MyQt HTML layout/adaptor demo"));
    window.resize(static_cast<int>(kViewportWidth),
                  static_cast<int>(kViewportHeight));
    window.setColor(QColor(236, 234, 229));

    auto *item = new HtmlDemoItem(window.contentItem());
    item->setWidth(kViewportWidth);
    item->setHeight(kViewportHeight);

    if (!item->initialize()) {
        return 1;
    }

    window.show();
    return app.exec();
}
