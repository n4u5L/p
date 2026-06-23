#include "qt_adaptor/layout_scene_adapter.h"

#include <algorithm>
#include <utility>

#include <QMatrix4x4>
#include <QQuickItem>
#include <QSGNode>

namespace myqt {
namespace {

QRectF rectFromLayout(layout_rect_t rect)
{
    return QRectF(rect.x, rect.y, rect.width, rect.height);
}

} // namespace

std::size_t
LayoutFragmentKeyHash::operator()(const layout_fragment_key_t &key) const noexcept
{
    std::size_t seed = static_cast<std::size_t>(key.object_id);

    seed ^= static_cast<std::size_t>(key.fragment_index)
        + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::size_t>(key.role)
        + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::size_t>(key.ordinal)
        + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);

    return seed;
}

bool
LayoutFragmentKeyEqual::operator()(const layout_fragment_key_t &left,
                                   const layout_fragment_key_t &right) const noexcept
{
    return left.object_id == right.object_id
        && left.fragment_index == right.fragment_index
        && left.role == right.role
        && left.ordinal == right.ordinal;
}

LayoutSceneAdapter::LayoutSceneAdapter()
    : root_(std::make_unique<QSGNode>())
{
}

LayoutSceneAdapter::~LayoutSceneAdapter() = default;

QSGNode *
LayoutSceneAdapter::rootNode() const noexcept
{
    return root_ != nullptr ? root_.get() : releasedRoot_;
}

std::size_t
LayoutSceneAdapter::handleCount() const noexcept
{
    return handles_.size();
}

uint64_t
LayoutSceneAdapter::generation() const noexcept
{
    return generation_;
}

QSGNode *
LayoutSceneAdapter::releaseRootNode() noexcept
{
    releasedRoot_ = root_.release();
    return releasedRoot_;
}

void
LayoutSceneAdapter::adoptRootNode(QSGNode *root) noexcept
{
    if (root == nullptr || root_.get() == root) {
        return;
    }

    if (releasedRoot_ == root) {
        releasedRoot_ = nullptr;
        root_.reset(root);
    }
}

void
LayoutSceneAdapter::resetRootNode(QSGNode *root) noexcept
{
    handles_.clear();
    releasedRoot_ = nullptr;
    root_.reset(root == nullptr ? new QSGNode() : root);
    generation_ = 0;
}

bool
LayoutSceneAdapter::rebuild(layout_scene_plan_t *plan)
{
    if (plan == nullptr) {
        clear();
        return false;
    }

    clear();

    const std::size_t count = layout_scene_plan_node_count(plan);
    for (std::size_t i = 0; i < count; ++i) {
        const layout_scene_node_t *node = layout_scene_plan_node_at(plan, i);
        if (node == nullptr
            || !insertNode(snapshotForNode(
                node, layout_scene_node_dirty_bits(node)))) {
            clear();
            return false;
        }
    }

    generation_ = layout_scene_plan_generation(plan);
    return true;
}

bool
LayoutSceneAdapter::applyDiff(layout_scene_plan_t *oldPlan,
                              layout_scene_plan_t *newPlan)
{
    if (oldPlan != nullptr
        && generation_ != layout_scene_plan_generation(oldPlan)) {
        return false;
    }

    layout_scene_diff_t *diff = layout_scene_diff_create();
    if (diff == nullptr) {
        return false;
    }

    const lxb_status_t status =
        layout_scene_plan_diff(oldPlan, newPlan, diff);
    if (status != LXB_STATUS_OK) {
        layout_scene_diff_destroy(diff, true);
        return false;
    }

    const bool ok = applyDiff(newPlan, diff);
    layout_scene_diff_destroy(diff, true);
    return ok;
}

bool
LayoutSceneAdapter::applyDiff(layout_scene_plan_t *newPlan,
                              layout_scene_diff_t *diff)
{
    if (newPlan == nullptr || diff == nullptr) {
        return false;
    }

    const uint64_t oldGeneration = layout_scene_diff_old_generation(diff);
    const uint64_t newGeneration = layout_scene_diff_new_generation(diff);
    const uint64_t planGeneration = layout_scene_plan_generation(newPlan);

    if (newGeneration == 0) {
        return false;
    }

    if (newGeneration != 0 && newGeneration != planGeneration) {
        return false;
    }

    if (generation_ == planGeneration
        && (newGeneration == 0 || newGeneration == planGeneration)) {
        return true;
    }

    if (oldGeneration != generation_) {
        return false;
    }

    SceneUpdateBatch batch;
    if (!buildSceneUpdateBatch(newPlan, diff, batch)
        || !validateSceneUpdateBatch(newPlan, batch)) {
        return false;
    }

    if (!applySceneUpdateBatch(batch)) {
        return false;
    }

    generation_ = planGeneration;
    return true;
}

const LayoutSceneAdapter::Handle *
LayoutSceneAdapter::findHandle(layout_fragment_key_t key) const noexcept
{
    auto it = handles_.find(key);
    if (it == handles_.end()) {
        return nullptr;
    }

    return &it->second;
}

QSGTransformNode *
LayoutSceneAdapter::qsgNodeForKey(layout_fragment_key_t key) const noexcept
{
    const Handle *handle = findHandle(key);
    return handle == nullptr ? nullptr : handle->node;
}

bool
LayoutSceneAdapter::isEmptyKey(layout_fragment_key_t key) noexcept
{
    return key.object_id == 0 && key.fragment_index == 0 && key.role == 0
        && key.ordinal == 0;
}

QMatrix4x4
LayoutSceneAdapter::matrixForSnapshot(const SceneNodeSnapshot &node)
{
    QMatrix4x4 matrix;
    const layout_point_t offset = node.offset;
    const layout_transform_t transform = node.transform;

    matrix.translate(static_cast<float>(offset.x), static_cast<float>(offset.y));
    matrix *= QMatrix4x4(static_cast<float>(transform.a),
                         static_cast<float>(transform.b),
                         0.0f,
                         static_cast<float>(transform.e),
                         static_cast<float>(transform.c),
                         static_cast<float>(transform.d),
                         0.0f,
                         static_cast<float>(transform.f),
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f);

    return matrix;
}

LayoutSceneAdapter::SceneNodeSnapshot
LayoutSceneAdapter::snapshotForNode(const layout_scene_node_t *node,
                                    unsigned dirtyBits)
{
    SceneNodeSnapshot snapshot;

    if (node == nullptr) {
        return snapshot;
    }

    snapshot.key = layout_scene_node_key(node);
    snapshot.parentKey = layout_scene_node_parent_key(node);
    snapshot.previousSiblingKey = layout_scene_node_previous_sibling_key(node);
    snapshot.slot = layout_scene_node_child_slot(node);
    snapshot.sequence = layout_scene_node_sequence(node);
    snapshot.placement = layout_scene_node_placement(node);
    snapshot.stackingOrder = layout_scene_node_stacking_order(node);
    snapshot.localRect = rectFromLayout(layout_scene_node_local_rect(node));
    snapshot.clipRect = rectFromLayout(layout_scene_node_clip_rect(node));
    snapshot.clipAxes = layout_scene_node_clip_axes(node);
    snapshot.opacity = layout_scene_node_opacity(node);
    snapshot.flags = layout_scene_node_flags(node);
    snapshot.layerHints = layout_scene_node_layer_hints(node);
    snapshot.dirtyBits = dirtyBits;
    snapshot.nativeEmbedToken = layout_scene_node_native_embed_token(node);
    snapshot.offset = layout_scene_node_offset(node);
    snapshot.transform = layout_scene_node_transform(node);
    snapshot.planIndex = layout_scene_node_index(node);

    return snapshot;
}

bool
LayoutSceneAdapter::buildSceneUpdateBatch(
    layout_scene_plan_t *newPlan,
    layout_scene_diff_t *diff,
    SceneUpdateBatch &batch) const
{
    if (newPlan == nullptr || diff == nullptr) {
        return false;
    }

    batch.clear();
    batch.reserve(layout_scene_diff_patch_count(diff));

    const std::size_t patchCount = layout_scene_diff_patch_count(diff);
    for (std::size_t i = 0; i < patchCount; ++i) {
        const layout_scene_patch_t *patch = layout_scene_diff_patch_at(diff, i);
        if (patch == nullptr) {
            return false;
        }

        SceneUpdateEntry entry;
        entry.key = layout_scene_patch_key(patch);

        switch (layout_scene_patch_op(patch)) {
        case LAYOUT_SCENE_PATCH_REMOVE:
            entry.remove = true;
            break;
        case LAYOUT_SCENE_PATCH_INSERT:
            entry.mayCreate = true;
            break;
        case LAYOUT_SCENE_PATCH_MOVE:
            entry.mustExist = true;
            break;
        case LAYOUT_SCENE_PATCH_UPDATE:
        case LAYOUT_SCENE_PATCH_KEEP:
            entry.mustExist = true;
            break;
        }

        if (!entry.remove) {
            const std::size_t newIndex = layout_scene_patch_new_index(patch);
            const layout_scene_node_t *newNode =
                layout_scene_plan_node_at(newPlan, newIndex);
            if (newNode == nullptr) {
                return false;
            }
            entry.node = snapshotForNode(
                newNode, layout_scene_patch_dirty_bits(patch));
        }

        batch.push_back(entry);
    }

    return true;
}

bool
LayoutSceneAdapter::validateSceneUpdateBatch(
    layout_scene_plan_t *newPlan,
    const SceneUpdateBatch &batch) const
{
    if (newPlan == nullptr) {
        return false;
    }

    std::unordered_map<layout_fragment_key_t,
                       layout_fragment_key_t,
                       LayoutFragmentKeyHash,
                       LayoutFragmentKeyEqual> simulatedParents;
    simulatedParents.reserve(handles_.size() + batch.size());
    for (const auto &entry : handles_) {
        simulatedParents.emplace(entry.first, entry.second.parentKey);
    }

    for (const SceneUpdateEntry &entry : batch) {
        if (isEmptyKey(entry.key)) {
            return false;
        }

        if (entry.remove) {
            std::vector<layout_fragment_key_t> stack;
            stack.push_back(entry.key);
            while (!stack.empty()) {
                const layout_fragment_key_t removeKey = stack.back();
                stack.pop_back();

                std::vector<layout_fragment_key_t> children;
                for (const auto &entry : simulatedParents) {
                    if (LayoutFragmentKeyEqual {}(entry.second, removeKey)) {
                        children.push_back(entry.first);
                    }
                }
                stack.insert(stack.end(), children.begin(), children.end());
                simulatedParents.erase(removeKey);
            }
        }
    }

    for (const SceneUpdateEntry &entry : batch) {
        if (entry.remove) {
            continue;
        }

        if (isEmptyKey(entry.key)) {
            return false;
        }
        if (isEmptyKey(entry.node.key)
            || !LayoutFragmentKeyEqual {}(entry.key, entry.node.key)) {
            return false;
        }

        if (entry.mustExist && handles_.find(entry.key) == handles_.end()) {
            return false;
        }

        if (!isEmptyKey(entry.node.parentKey)
            && sceneNodeForKey(newPlan, entry.node.parentKey) == nullptr) {
            return false;
        }

        simulatedParents[entry.key] = entry.node.parentKey;
    }

    for (const SceneUpdateEntry &entry : batch) {
        if (entry.remove) {
            continue;
        }

        if (!isEmptyKey(entry.node.parentKey)
            && simulatedParents.find(entry.node.parentKey)
                   == simulatedParents.end()) {
            return false;
        }

        if (!isEmptyKey(entry.node.previousSiblingKey)) {
            const layout_scene_node_t *previous =
                sceneNodeForKey(newPlan, entry.node.previousSiblingKey);
            if (previous == nullptr
                || !LayoutFragmentKeyEqual {}(
                    layout_scene_node_parent_key(previous),
                    entry.node.parentKey)) {
                return false;
            }

            auto previousIt =
                simulatedParents.find(entry.node.previousSiblingKey);
            if (previousIt == simulatedParents.end()
                || !LayoutFragmentKeyEqual {}(previousIt->second,
                                              entry.node.parentKey)) {
                return false;
            }
        }
    }

    return true;
}

bool
LayoutSceneAdapter::applySceneUpdateBatch(const SceneUpdateBatch &batch)
{
    std::vector<const SceneUpdateEntry *> ordered;
    ordered.reserve(batch.size());
    for (const SceneUpdateEntry &entry : batch) {
        ordered.push_back(&entry);
    }

    std::sort(ordered.begin(), ordered.end(),
              [](const SceneUpdateEntry *left,
                 const SceneUpdateEntry *right) {
                  return left->node.planIndex < right->node.planIndex;
              });

    std::vector<layout_fragment_key_t> keptKeys;
    keptKeys.reserve(ordered.size());
    for (const SceneUpdateEntry *entry : ordered) {
        if (!entry->remove) {
            keptKeys.push_back(entry->key);
        }
    }

    for (const SceneUpdateEntry *entry : ordered) {
        if (entry->remove) {
            removeHandleKeeping(entry->key, keptKeys);
        }
    }

    for (const SceneUpdateEntry *entry : ordered) {
        if (!entry->remove && !ensureNode(entry->node)) {
            return false;
        }
    }

    for (const SceneUpdateEntry *entry : ordered) {
        if (entry->remove) {
            continue;
        }

        auto it = handles_.find(entry->key);
        if (it == handles_.end()
            || !moveHandle(it->second, entry->node.parentKey,
                           entry->node.previousSiblingKey)) {
            return false;
        }
    }

    for (const SceneUpdateEntry *entry : ordered) {
        if (!entry->remove && !updateNode(entry->node)) {
            return false;
        }
    }

    return true;
}

bool
LayoutSceneAdapter::ensureNode(const SceneNodeSnapshot &node)
{
    if (isEmptyKey(node.key)) {
        return false;
    }

    auto existing = handles_.find(node.key);
    if (existing != handles_.end()) {
        SceneNodeSnapshot updated = node;
        updated.dirtyBits |= LAYOUT_SCENE_DIRTY_CHILD_LIST;
        return updateNode(updated);
    }

    return createUnplacedNode(node);
}

bool
LayoutSceneAdapter::createUnplacedNode(const SceneNodeSnapshot &node)
{
    if (isEmptyKey(node.key)) {
        return false;
    }

    Handle handle;
    fillHandleFromSnapshot(handle, node);
    handle.node = new QSGTransformNode();
    handle.node->setMatrix(matrixForSnapshot(node));

    handles_.emplace(handle.key, std::move(handle));
    return true;
}

bool
LayoutSceneAdapter::insertNode(const SceneNodeSnapshot &node)
{
    if (isEmptyKey(node.key)) {
        return false;
    }

    Handle handle;
    fillHandleFromSnapshot(handle, node);
    handle.node = new QSGTransformNode();
    handle.node->setMatrix(matrixForSnapshot(node));

    QSGNode *parent = parentNodeForKey(handle.parentKey);
    QSGNode *previousSibling =
        previousSiblingNodeForKey(handle.previousSiblingKey);
    if (parent == nullptr
        || !canInsertAfter(parent, handle.previousSiblingKey,
                           previousSibling)) {
        delete handle.node;
        return false;
    }

    insertQsgNode(parent, previousSibling, handle.node);
    handles_.emplace(handle.key, std::move(handle));
    return true;
}

bool
LayoutSceneAdapter::updateNode(const SceneNodeSnapshot &node)
{
    auto it = handles_.find(node.key);
    if (it == handles_.end()) {
        return false;
    }

    Handle &handle = it->second;
    fillHandleFromSnapshot(handle, node);

    if ((node.dirtyBits & (LAYOUT_SCENE_DIRTY_GEOMETRY
                           | LAYOUT_SCENE_DIRTY_TRANSFORM
                           | LAYOUT_SCENE_DIRTY_LAYOUT)) != 0) {
        handle.node->setMatrix(matrixForSnapshot(node));
        handle.node->markDirty(QSGNode::DirtyMatrix);
    }

    return true;
}

void
LayoutSceneAdapter::fillHandleFromSnapshot(
    Handle &handle,
    const SceneNodeSnapshot &node)
{
    handle.key = node.key;
    handle.parentKey = node.parentKey;
    handle.previousSiblingKey = node.previousSiblingKey;
    handle.slot = node.slot;
    handle.sequence = node.sequence;
    handle.placement = node.placement;
    handle.stackingOrder = node.stackingOrder;
    handle.localRect = node.localRect;
    handle.clipRect = node.clipRect;
    handle.clipAxes = node.clipAxes;
    handle.opacity = node.opacity;
    handle.flags = node.flags;
    handle.layerHints = node.layerHints;
    handle.dirtyBits = node.dirtyBits;
    handle.nativeEmbedToken = node.nativeEmbedToken;
}

bool
LayoutSceneAdapter::moveHandle(Handle &handle,
                               layout_fragment_key_t parentKey,
                               layout_fragment_key_t previousSiblingKey)
{
    QSGNode *parent = parentNodeForKey(parentKey);
    QSGNode *previousSibling = previousSiblingNodeForKey(previousSiblingKey);

    if (parent == nullptr || handle.node == nullptr
        || !canInsertAfter(parent, previousSiblingKey, previousSibling)) {
        return false;
    }

    insertQsgNode(parent, previousSibling, handle.node);
    handle.parentKey = parentKey;
    handle.previousSiblingKey = previousSiblingKey;
    return true;
}

void
LayoutSceneAdapter::removeHandle(layout_fragment_key_t key)
{
    removeHandleKeeping(key, {});
}

void
LayoutSceneAdapter::removeHandleKeeping(
    layout_fragment_key_t key,
    const std::vector<layout_fragment_key_t> &keptKeys)
{
    std::vector<layout_fragment_key_t> keys;
    collectSubtreeKeys(key, keys);
    if (keys.empty()) {
        return;
    }

    std::vector<layout_fragment_key_t> keptSubtreeKeys;
    for (layout_fragment_key_t removeKey : keys) {
        bool keep = false;
        for (const layout_fragment_key_t keptKey : keptKeys) {
            if (LayoutFragmentKeyEqual {}(removeKey, keptKey)) {
                keep = true;
                break;
            }
        }
        if (keep) {
            keptSubtreeKeys.push_back(removeKey);
        }
    }

    for (layout_fragment_key_t keepKey : keptSubtreeKeys) {
        auto keepIt = handles_.find(keepKey);
        if (keepIt != handles_.end() && keepIt->second.node != nullptr) {
            if (QSGNode *parent = keepIt->second.node->parent()) {
                parent->removeChildNode(keepIt->second.node);
            }
        }
    }

    for (layout_fragment_key_t removeKey : keys) {
        bool keep = false;
        for (const layout_fragment_key_t keptKey : keptSubtreeKeys) {
            if (LayoutFragmentKeyEqual {}(removeKey, keptKey)) {
                keep = true;
                break;
            }
        }
        if (keep) {
            continue;
        }

        auto it = handles_.find(removeKey);
        if (it == handles_.end()) {
            continue;
        }

        if (LayoutFragmentKeyEqual {}(removeKey, key)
            && it->second.node != nullptr) {
            if (QSGNode *parent = it->second.node->parent()) {
                parent->removeChildNode(it->second.node);
            }
            delete it->second.node;
        }

        handles_.erase(it);
    }
}

void
LayoutSceneAdapter::collectSubtreeKeys(
    layout_fragment_key_t key,
    std::vector<layout_fragment_key_t> &keys) const
{
    if (handles_.find(key) == handles_.end()) {
        return;
    }

    keys.push_back(key);

    for (const auto &entry : handles_) {
        if (!LayoutFragmentKeyEqual {}(entry.first, key)
            && LayoutFragmentKeyEqual {}(entry.second.parentKey, key)) {
            collectSubtreeKeys(entry.first, keys);
        }
    }
}

void
LayoutSceneAdapter::clear()
{
    handles_.clear();
    root_ = std::make_unique<QSGNode>();
    generation_ = 0;
}

const layout_scene_node_t *
LayoutSceneAdapter::sceneNodeForKey(
    layout_scene_plan_t *plan,
    layout_fragment_key_t key) const noexcept
{
    const std::size_t count = layout_scene_plan_node_count(plan);
    for (std::size_t i = 0; i < count; ++i) {
        const layout_scene_node_t *node = layout_scene_plan_node_at(plan, i);
        if (node != nullptr
            && LayoutFragmentKeyEqual {}(layout_scene_node_key(node), key)) {
            return node;
        }
    }

    return nullptr;
}

QSGNode *
LayoutSceneAdapter::parentNodeForKey(layout_fragment_key_t parentKey) const noexcept
{
    if (isEmptyKey(parentKey)) {
        return rootNode();
    }

    const Handle *parent = findHandle(parentKey);
    return parent == nullptr ? nullptr : parent->node;
}

QSGNode *
LayoutSceneAdapter::previousSiblingNodeForKey(
    layout_fragment_key_t previousSiblingKey) const noexcept
{
    if (isEmptyKey(previousSiblingKey)) {
        return nullptr;
    }

    const Handle *previous = findHandle(previousSiblingKey);
    return previous == nullptr ? nullptr : previous->node;
}

bool
LayoutSceneAdapter::canInsertAfter(
    QSGNode *parent,
    layout_fragment_key_t previousSiblingKey,
    QSGNode *previousSibling) const noexcept
{
    if (isEmptyKey(previousSiblingKey)) {
        return true;
    }

    return parent != nullptr && previousSibling != nullptr
        && previousSibling->parent() == parent;
}

void
LayoutSceneAdapter::insertQsgNode(QSGNode *parent,
                                  QSGNode *previousSibling,
                                  QSGNode *node)
{
    if (node == nullptr || parent == nullptr) {
        return;
    }

    if (QSGNode *oldParent = node->parent()) {
        oldParent->removeChildNode(node);
    }
    if (previousSibling == nullptr) {
        parent->prependChildNode(node);
    }
    else {
        parent->insertChildNodeAfter(node, previousSibling);
    }
}

LayoutSceneItem::LayoutSceneItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

LayoutSceneItem::~LayoutSceneItem() = default;

LayoutSceneAdapter *
LayoutSceneItem::adapter() noexcept
{
    return &adapter_;
}

const LayoutSceneAdapter *
LayoutSceneItem::adapter() const noexcept
{
    return &adapter_;
}

bool
LayoutSceneItem::scheduleRebuild(layout_scene_plan_t *plan)
{
    pendingOperation_ = PendingOperation::Rebuild;
    pendingOldPlan_ = nullptr;
    pendingNewPlan_ = plan;
    pendingDiff_ = nullptr;
    update();
    return plan != nullptr;
}

bool
LayoutSceneItem::scheduleDiff(layout_scene_plan_t *oldPlan,
                              layout_scene_plan_t *newPlan)
{
    pendingOperation_ = PendingOperation::DiffPlans;
    pendingOldPlan_ = oldPlan;
    pendingNewPlan_ = newPlan;
    pendingDiff_ = nullptr;
    update();
    return oldPlan != nullptr && newPlan != nullptr;
}

bool
LayoutSceneItem::scheduleDiff(layout_scene_plan_t *newPlan,
                              layout_scene_diff_t *diff)
{
    pendingOperation_ = PendingOperation::Diff;
    pendingOldPlan_ = nullptr;
    pendingNewPlan_ = newPlan;
    pendingDiff_ = diff;
    update();
    return newPlan != nullptr && diff != nullptr;
}

bool
LayoutSceneItem::rebuildScene(layout_scene_plan_t *plan)
{
    return scheduleRebuild(plan);
}

bool
LayoutSceneItem::applySceneDiff(layout_scene_plan_t *oldPlan,
                                layout_scene_plan_t *newPlan)
{
    return scheduleDiff(oldPlan, newPlan);
}

bool
LayoutSceneItem::applySceneDiff(layout_scene_plan_t *newPlan,
                                layout_scene_diff_t *diff)
{
    return scheduleDiff(newPlan, diff);
}

void
LayoutSceneItem::setScenePlan(layout_scene_plan_t *plan)
{
    if (pendingOperation_ == PendingOperation::Rebuild
        && pendingNewPlan_ == plan) {
        return;
    }

    scheduleRebuild(plan);
}

layout_scene_plan_t *
LayoutSceneItem::scenePlan() const noexcept
{
    return pendingNewPlan_;
}

QSGNode *
LayoutSceneItem::updatePaintNode(QSGNode *oldNode,
                                 UpdatePaintNodeData *data)
{
    (void) data;

    if (oldNode != nullptr && oldNode != adapter_.rootNode()) {
        adapter_.resetRootNode(oldNode);
    }
    else {
        adapter_.adoptRootNode(oldNode);
    }

    bool operationApplied = true;

    switch (pendingOperation_) {
    case PendingOperation::Rebuild:
        operationApplied = adapter_.rebuild(pendingNewPlan_);
        break;
    case PendingOperation::DiffPlans:
        operationApplied = adapter_.applyDiff(pendingOldPlan_, pendingNewPlan_);
        if (!operationApplied && pendingNewPlan_ != nullptr) {
            operationApplied = adapter_.rebuild(pendingNewPlan_);
        }
        break;
    case PendingOperation::Diff:
        operationApplied = adapter_.applyDiff(pendingNewPlan_, pendingDiff_);
        if (!operationApplied && pendingNewPlan_ != nullptr) {
            operationApplied = adapter_.rebuild(pendingNewPlan_);
        }
        break;
    case PendingOperation::None:
        break;
    }

    if (pendingOperation_ != PendingOperation::None) {
        (void) operationApplied;
        pendingOperation_ = PendingOperation::None;
        pendingOldPlan_ = nullptr;
        pendingNewPlan_ = nullptr;
        pendingDiff_ = nullptr;
    }
    else if (oldNode == nullptr && adapter_.rootNode() == nullptr) {
        adapter_.resetRootNode(nullptr);
    }

    return adapter_.releaseRootNode();
}

} // namespace myqt
