#include <cstdio>
#include <cstring>

#include <QGuiApplication>
#include <QMatrix4x4>
#include <QPointF>
#include <QSGNode>

#include "layout/layout.h"
#include "qt_adaptor/layout_scene_adapter.h"

namespace {

bool failed = false;

void check(bool ok, const char *msg)
{
    if (!ok) {
        failed = true;
        std::printf("FAIL: %s\n", msg);
    }
}

bool keyEqual(layout_fragment_key_t left, layout_fragment_key_t right)
{
    myqt::LayoutFragmentKeyEqual equal;
    return equal(left, right);
}

bool keyEmpty(layout_fragment_key_t key)
{
    layout_fragment_key_t empty {};
    return keyEqual(key, empty);
}

layout_object_t *makeObject(layout_t *layout)
{
    lxb_style_computed_t *style = lxb_style_computed_create();
    if (style == nullptr
        || lxb_style_computed_set_initial(style, 16.0, 19.2)
               != LXB_STATUS_OK) {
        return nullptr;
    }

    layout_object_t *object = layout_object_create_anonymous(layout, style);
    lxb_style_computed_unref(style);
    return object;
}

layout_fragment_t *makeFragment(layout_result_t *result,
                                layout_object_t *object,
                                double width,
                                double height)
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

layout_fragment_t *makeTransformedFragment(layout_result_t *result,
                                           layout_object_t *object,
                                           double width,
                                           double height,
                                           layout_transform_t transform)
{
    layout_fragment_init_t init;

    std::memset(&init, 0, sizeof(init));
    init.object = object;
    init.size.width = width;
    init.size.height = height;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    init.transform = transform;

    return layout_fragment_create(result, &init);
}

bool buildPlan(layout_scene_plan_t *plan, layout_result_t *result)
{
    return layout_result_freeze(result) == LXB_STATUS_OK
        && layout_scene_plan_build(plan, result) == LXB_STATUS_OK;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    layout_t *layout = layout_create();
    check(layout != nullptr, "layout creates");
    if (layout != nullptr) {
        check(layout_init(layout) == LXB_STATUS_OK, "layout initializes");
    }

    layout_result_t *oldResult = layout_result_create(layout);
    layout_result_t *newResult = layout_result_create(layout);
    layout_scene_plan_t *oldPlan = layout_scene_plan_create(layout);
    layout_scene_plan_t *newPlan = layout_scene_plan_create(layout);
    layout_scene_diff_t *diff = layout_scene_diff_create();

    layout_object_t *root = makeObject(layout);
    layout_object_t *kept = makeObject(layout);
    layout_object_t *moved = makeObject(layout);
    layout_object_t *updated = makeObject(layout);
    layout_object_t *removed = makeObject(layout);
    layout_object_t *inserted = makeObject(layout);
    layout_fragment_t *oldRoot = nullptr;
    layout_fragment_t *oldKept = nullptr;
    layout_fragment_t *oldMoved = nullptr;
    layout_fragment_t *oldUpdated = nullptr;
    layout_fragment_t *oldRemoved = nullptr;
    layout_fragment_t *newRoot = nullptr;
    layout_fragment_t *newMoved = nullptr;
    layout_fragment_t *newKept = nullptr;
    layout_fragment_t *newUpdated = nullptr;
    layout_fragment_t *newInserted = nullptr;

    check(oldResult != nullptr && newResult != nullptr,
          "adapter results allocate");
    check(oldPlan != nullptr && newPlan != nullptr && diff != nullptr,
          "adapter plans allocate");
    check(root != nullptr && kept != nullptr && moved != nullptr
              && updated != nullptr && removed != nullptr
              && inserted != nullptr,
          "adapter objects allocate");

    if (!failed) {
        oldRoot = makeFragment(oldResult, root, 200.0, 140.0);
        oldKept = makeFragment(oldResult, kept, 20.0, 20.0);
        oldMoved = makeFragment(oldResult, moved, 20.0, 20.0);
        oldUpdated = makeFragment(oldResult, updated, 20.0, 20.0);
        oldRemoved = makeFragment(oldResult, removed, 20.0, 20.0);

        check(oldRoot != nullptr && oldKept != nullptr && oldMoved != nullptr
                  && oldUpdated != nullptr && oldRemoved != nullptr,
              "adapter old fragments allocate");
        check(layout_fragment_append_child(oldRoot, oldKept, {0.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter old appends kept");
        check(layout_fragment_append_child(oldRoot, oldMoved, {30.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter old appends moved");
        check(layout_fragment_append_child(oldRoot, oldUpdated, {60.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter old appends updated");
        check(layout_fragment_append_child(oldRoot, oldRemoved, {90.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter old appends removed");
        check(layout_result_set_root_fragment(oldResult, oldRoot)
                  == LXB_STATUS_OK,
              "adapter old result stores root");

        newRoot = makeFragment(newResult, root, 200.0, 140.0);
        newMoved = makeTransformedFragment(newResult, moved, 20.0, 20.0,
                                           {2.0, 3.0, 5.0, 7.0, 11.0, 13.0});
        newKept = makeFragment(newResult, kept, 20.0, 20.0);
        newUpdated = makeFragment(newResult, updated, 30.0, 20.0);
        newInserted = makeFragment(newResult, inserted, 20.0, 20.0);

        check(newRoot != nullptr && newMoved != nullptr && newKept != nullptr
                  && newUpdated != nullptr && newInserted != nullptr,
              "adapter new fragments allocate");
        check(layout_fragment_append_child(newRoot, newMoved, {30.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter new appends moved first");
        check(layout_fragment_append_child(newRoot, newKept, {0.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter new appends kept second");
        check(layout_fragment_append_child(newRoot, newUpdated, {60.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter new appends updated");
        check(layout_fragment_append_child(newRoot, newInserted, {90.0, 0.0})
                  == LXB_STATUS_OK,
              "adapter new appends inserted");
        check(layout_result_set_root_fragment(newResult, newRoot)
                  == LXB_STATUS_OK,
              "adapter new result stores root");

        check(buildPlan(oldPlan, oldResult), "adapter old plan builds");
        check(buildPlan(newPlan, newResult), "adapter new plan builds");
        check(layout_scene_plan_diff(oldPlan, newPlan, diff) == LXB_STATUS_OK,
              "adapter diff builds");
        check(layout_scene_diff_old_generation(diff)
                      == layout_scene_plan_generation(oldPlan)
                  && layout_scene_diff_new_generation(diff)
                         == layout_scene_plan_generation(newPlan),
              "adapter diff carries old and new generation tokens");
    }

    if (!failed) {
        myqt::LayoutSceneAdapter adapter;

        check(adapter.rebuild(oldPlan), "adapter rebuilds retained tree");
        check(adapter.handleCount() == layout_scene_plan_node_count(oldPlan),
              "adapter creates one handle per old scene node");

        const layout_fragment_key_t movedKey = layout_fragment_key(newMoved);
        const layout_fragment_key_t keptKey = layout_fragment_key(newKept);
        const layout_fragment_key_t updatedKey = layout_fragment_key(newUpdated);
        const layout_fragment_key_t removedKey = layout_fragment_key(oldRemoved);
        const layout_fragment_key_t insertedKey = layout_fragment_key(newInserted);

        QSGTransformNode *movedNode = adapter.qsgNodeForKey(movedKey);
        QSGTransformNode *keptNode = adapter.qsgNodeForKey(keptKey);
        QSGTransformNode *updatedNode = adapter.qsgNodeForKey(updatedKey);

        check(movedNode != nullptr && keptNode != nullptr
                  && updatedNode != nullptr,
              "adapter exposes retained QSG nodes before diff");

        check(adapter.applyDiff(newPlan, diff),
              "adapter applies retained scene diff");
        check(adapter.generation() == layout_scene_plan_generation(newPlan),
              "adapter records new generation");
        check(adapter.handleCount() == layout_scene_plan_node_count(newPlan),
              "adapter handle count follows new plan");
        check(adapter.qsgNodeForKey(movedKey) == movedNode
                  && adapter.qsgNodeForKey(keptKey) == keptNode
                  && adapter.qsgNodeForKey(updatedKey) == updatedNode,
              "adapter reuses retained QSG nodes for moved/updated children");
        check(adapter.qsgNodeForKey(removedKey) == nullptr,
              "adapter removes stale retained node");
        check(adapter.qsgNodeForKey(insertedKey) != nullptr,
              "adapter inserts retained node for new child");
        check(adapter.rootNode()->firstChild() == adapter.qsgNodeForKey(
                  layout_fragment_key(newRoot)),
              "adapter keeps scene root as first retained child");
        check(movedNode->parent() == adapter.qsgNodeForKey(
                  layout_fragment_key(newRoot)),
              "adapter reparents moved node under retained parent");
        check(movedNode->previousSibling() == nullptr,
              "adapter places moved node at first visual slot");
        check(keptNode->previousSibling() == movedNode,
              "adapter preserves retained sibling order after move");
        const QPointF mapped =
            movedNode->matrix().map(QPointF(17.0, 19.0));
        check(mapped == QPointF(2.0 * 17.0 + 5.0 * 19.0 + 11.0 + 30.0,
                                3.0 * 17.0 + 7.0 * 19.0 + 13.0),
              "adapter maps layout affine transform into QSG matrix");

        const myqt::LayoutSceneAdapter::Handle *movedHandle =
            adapter.findHandle(movedKey);
        const myqt::LayoutSceneAdapter::Handle *keptHandle =
            adapter.findHandle(keptKey);
        const myqt::LayoutSceneAdapter::Handle *updatedHandle =
            adapter.findHandle(updatedKey);

        check(movedHandle != nullptr && movedHandle->slot == 0
                  && keyEmpty(movedHandle->previousSiblingKey),
              "adapter caches indexed slot for first moved child");
        check(keptHandle != nullptr && keptHandle->slot == 1
                  && keyEqual(keptHandle->previousSiblingKey, movedKey),
              "adapter caches previous sibling key for retained move");
        check(updatedHandle != nullptr
                  && (updatedHandle->dirtyBits & LAYOUT_SCENE_DIRTY_GEOMETRY)
                         != 0,
              "adapter preserves geometry dirty on moved update");

        const std::size_t handleCount = adapter.handleCount();
        check(adapter.applyDiff(newPlan, diff),
              "adapter treats applying same retained diff twice as no-op");
        check(adapter.handleCount() == handleCount,
              "adapter duplicate inserts do not create extra handles");

        myqt::LayoutSceneAdapter staleAdapter;
        check(!staleAdapter.applyDiff(newPlan, diff),
              "adapter rejects diff when retained generation is stale");
        check(staleAdapter.handleCount() == 0
                  && staleAdapter.generation() == 0,
              "adapter leaves retained state unchanged after stale diff");

        myqt::LayoutSceneAdapter mismatchAdapter;
        check(mismatchAdapter.rebuild(newPlan),
              "adapter builds mismatch generation fixture");
        const std::size_t mismatchHandleCount =
            mismatchAdapter.handleCount();
        check(!mismatchAdapter.applyDiff(oldPlan, newPlan),
              "adapter rejects plan diff when old plan generation mismatches");
        check(mismatchAdapter.handleCount() == mismatchHandleCount
                  && mismatchAdapter.generation()
                         == layout_scene_plan_generation(newPlan),
              "adapter preserves retained tree after mismatched plan diff");
    }

    if (!failed) {
        myqt::LayoutSceneAdapter adapter;
        layout_object_t *nested = makeObject(layout);
        layout_result_t *treeResult = layout_result_create(layout);
        layout_result_t *rootOnlyResult = layout_result_create(layout);
        layout_scene_plan_t *treePlan = layout_scene_plan_create(layout);
        layout_scene_plan_t *rootOnlyPlan = layout_scene_plan_create(layout);
        layout_scene_diff_t *removeDiff = layout_scene_diff_create();
        layout_fragment_t *treeRoot = nullptr;
        layout_fragment_t *treeParent = nullptr;
        layout_fragment_t *treeChild = nullptr;
        layout_fragment_t *rootOnly = nullptr;

        check(nested != nullptr && treeResult != nullptr
                  && rootOnlyResult != nullptr && treePlan != nullptr
                  && rootOnlyPlan != nullptr && removeDiff != nullptr,
              "adapter subtree remove fixtures allocate");

        if (!failed) {
            treeRoot = makeFragment(treeResult, root, 200.0, 140.0);
            treeParent = makeFragment(treeResult, inserted, 80.0, 40.0);
            treeChild = makeFragment(treeResult, nested, 20.0, 20.0);
            rootOnly = makeFragment(rootOnlyResult, root, 200.0, 140.0);

            check(treeRoot != nullptr && treeParent != nullptr
                      && treeChild != nullptr && rootOnly != nullptr,
                  "adapter subtree remove fragments allocate");
            check(layout_fragment_append_child(treeRoot, treeParent,
                                               {10.0, 10.0})
                      == LXB_STATUS_OK,
                  "adapter subtree remove appends parent");
            check(layout_fragment_append_child(treeParent, treeChild,
                                               {4.0, 4.0})
                      == LXB_STATUS_OK,
                  "adapter subtree remove appends child");
            check(layout_result_set_root_fragment(treeResult, treeRoot)
                      == LXB_STATUS_OK,
                  "adapter subtree result stores tree root");
            check(layout_result_set_root_fragment(rootOnlyResult, rootOnly)
                      == LXB_STATUS_OK,
                  "adapter root-only result stores root");
            check(buildPlan(treePlan, treeResult),
                  "adapter subtree plan builds");
            check(buildPlan(rootOnlyPlan, rootOnlyResult),
                  "adapter root-only plan builds");
            check(layout_scene_plan_diff(treePlan, rootOnlyPlan, removeDiff)
                      == LXB_STATUS_OK,
                  "adapter subtree remove diff builds");
        }

        if (!failed) {
            const layout_fragment_key_t parentKey =
                layout_fragment_key(treeParent);
            const layout_fragment_key_t childKey =
                layout_fragment_key(treeChild);

            check(adapter.rebuild(treePlan),
                  "adapter rebuilds tree with nested retained nodes");
            check(adapter.qsgNodeForKey(parentKey) != nullptr
                      && adapter.qsgNodeForKey(childKey) != nullptr,
                  "adapter exposes nested retained nodes before remove");
            check(adapter.applyDiff(rootOnlyPlan, removeDiff),
                  "adapter applies subtree remove diff");
            check(adapter.qsgNodeForKey(parentKey) == nullptr
                      && adapter.qsgNodeForKey(childKey) == nullptr,
                  "adapter removes stale retained subtree handles");
        }

        layout_scene_diff_destroy(removeDiff, true);
        layout_scene_plan_destroy(rootOnlyPlan, true);
        layout_scene_plan_destroy(treePlan, true);
        layout_result_destroy(rootOnlyResult, true);
        layout_result_destroy(treeResult, true);
    }

    if (!failed) {
        myqt::LayoutSceneAdapter adapter;
        layout_object_t *oldParentObject = makeObject(layout);
        layout_object_t *movingChildObject = makeObject(layout);
        layout_object_t *newSiblingObject = makeObject(layout);
        layout_result_t *oldMoveResult = layout_result_create(layout);
        layout_result_t *newMoveResult = layout_result_create(layout);
        layout_scene_plan_t *oldMovePlan = layout_scene_plan_create(layout);
        layout_scene_plan_t *newMovePlan = layout_scene_plan_create(layout);
        layout_scene_diff_t *moveOutDiff = layout_scene_diff_create();
        layout_fragment_t *oldMoveRoot = nullptr;
        layout_fragment_t *oldParentFragment = nullptr;
        layout_fragment_t *oldMovingChild = nullptr;
        layout_fragment_t *newMoveRoot = nullptr;
        layout_fragment_t *newMovingChild = nullptr;
        layout_fragment_t *newSibling = nullptr;

        check(oldParentObject != nullptr && movingChildObject != nullptr
                  && newSiblingObject != nullptr && oldMoveResult != nullptr
                  && newMoveResult != nullptr && oldMovePlan != nullptr
                  && newMovePlan != nullptr && moveOutDiff != nullptr,
              "adapter retained descendant move fixtures allocate");

        if (!failed) {
            oldMoveRoot = makeFragment(oldMoveResult, root, 200.0, 140.0);
            oldParentFragment =
                makeFragment(oldMoveResult, oldParentObject, 80.0, 40.0);
            oldMovingChild =
                makeFragment(oldMoveResult, movingChildObject, 20.0, 20.0);
            newMoveRoot = makeFragment(newMoveResult, root, 200.0, 140.0);
            newMovingChild =
                makeFragment(newMoveResult, movingChildObject, 20.0, 20.0);
            newSibling =
                makeFragment(newMoveResult, newSiblingObject, 20.0, 20.0);

            check(oldMoveRoot != nullptr && oldParentFragment != nullptr
                      && oldMovingChild != nullptr && newMoveRoot != nullptr
                      && newMovingChild != nullptr && newSibling != nullptr,
                  "adapter retained descendant move fragments allocate");
            check(layout_fragment_append_child(oldMoveRoot, oldParentFragment,
                                               {10.0, 10.0})
                      == LXB_STATUS_OK,
                  "adapter retained descendant old appends parent");
            check(layout_fragment_append_child(oldParentFragment,
                                               oldMovingChild,
                                               {4.0, 4.0})
                      == LXB_STATUS_OK,
                  "adapter retained descendant old appends child");
            check(layout_fragment_append_child(newMoveRoot, newMovingChild,
                                               {24.0, 16.0})
                      == LXB_STATUS_OK,
                  "adapter retained descendant new moves child to root");
            check(layout_fragment_append_child(newMoveRoot, newSibling,
                                               {60.0, 16.0})
                      == LXB_STATUS_OK,
                  "adapter retained descendant new appends sibling");
            check(layout_result_set_root_fragment(oldMoveResult, oldMoveRoot)
                      == LXB_STATUS_OK,
                  "adapter retained descendant old result stores root");
            check(layout_result_set_root_fragment(newMoveResult, newMoveRoot)
                      == LXB_STATUS_OK,
                  "adapter retained descendant new result stores root");
            check(buildPlan(oldMovePlan, oldMoveResult),
                  "adapter retained descendant old plan builds");
            check(buildPlan(newMovePlan, newMoveResult),
                  "adapter retained descendant new plan builds");
            check(layout_scene_plan_diff(oldMovePlan, newMovePlan, moveOutDiff)
                      == LXB_STATUS_OK,
                  "adapter retained descendant move diff builds");
        }

        if (!failed) {
            const layout_fragment_key_t oldParentKey =
                layout_fragment_key(oldParentFragment);
            const layout_fragment_key_t movingChildKey =
                layout_fragment_key(newMovingChild);

            check(adapter.rebuild(oldMovePlan),
                  "adapter retained descendant old tree builds");
            QSGTransformNode *movingNode =
                adapter.qsgNodeForKey(movingChildKey);
            check(movingNode != nullptr,
                  "adapter retained descendant exposes child before move");
            check(adapter.applyDiff(newMovePlan, moveOutDiff),
                  "adapter moves retained child out of removed subtree");
            check(adapter.qsgNodeForKey(oldParentKey) == nullptr,
                  "adapter removes old parent after moving child out");
            check(adapter.qsgNodeForKey(movingChildKey) == movingNode,
                  "adapter preserves retained child node moved out of subtree");
            check(movingNode->parent()
                      == adapter.qsgNodeForKey(layout_fragment_key(newMoveRoot)),
                  "adapter reparents moved child under new retained parent");
            check(movingNode->previousSibling() == nullptr,
                  "adapter moves retained child to first final slot");
        }

        layout_scene_diff_destroy(moveOutDiff, true);
        layout_scene_plan_destroy(newMovePlan, true);
        layout_scene_plan_destroy(oldMovePlan, true);
        layout_result_destroy(newMoveResult, true);
        layout_result_destroy(oldMoveResult, true);
    }

    if (!failed) {
        myqt::LayoutSceneItem item;

        check(item.adapter()->handleCount() == 0,
              "scene item starts with empty adapter state");
        check(item.scheduleRebuild(newPlan),
              "scene item accepts queued rebuild plan");
        check(item.scenePlan() == newPlan,
              "scene item stores pending scene plan until sync");
    }

    layout_scene_diff_destroy(diff, true);
    layout_scene_plan_destroy(newPlan, true);
    layout_scene_plan_destroy(oldPlan, true);
    layout_result_destroy(newResult, true);
    layout_result_destroy(oldResult, true);
    layout_destroy(layout, true);

    if (failed) {
        std::printf("\nlayout_scene_adapter_smoke failed\n");
        return 1;
    }

    std::printf("\nlayout_scene_adapter_smoke passed\n");
    return 0;
}
