#ifndef MYQT_LAYOUT_SCENE_ADAPTER_H_
#define MYQT_LAYOUT_SCENE_ADAPTER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <QQuickItem>
#include <QRectF>

#include "layout/layout.h"

class QSGNode;
class QSGTransformNode;
class QMatrix4x4;
namespace myqt {

struct LayoutFragmentKeyHash {
    std::size_t operator()(const layout_fragment_key_t &key) const noexcept;
};

struct LayoutFragmentKeyEqual {
    bool operator()(const layout_fragment_key_t &left,
                    const layout_fragment_key_t &right) const noexcept;
};

class LayoutSceneAdapter {
public:
    struct Handle {
        layout_fragment_key_t key {};
        layout_fragment_key_t parentKey {};
        layout_fragment_key_t previousSiblingKey {};
        std::size_t slot = LAYOUT_SCENE_NODE_NONE;
        std::size_t sequence = 0;
        layout_fragment_placement_t placement = LAYOUT_FRAGMENT_PLACEMENT_NORMAL;
        int stackingOrder = 0;
        QRectF localRect;
        QRectF clipRect;
        layout_overflow_clip_t clipAxes = LAYOUT_OVERFLOW_CLIP_NONE;
        double opacity = 1.0;
        unsigned flags = 0;
        unsigned layerHints = 0;
        unsigned dirtyBits = 0;
        layout_native_embed_token_t nativeEmbedToken = 0;
        QSGTransformNode *node = nullptr;
    };

    LayoutSceneAdapter();
    ~LayoutSceneAdapter();

    LayoutSceneAdapter(const LayoutSceneAdapter &) = delete;
    LayoutSceneAdapter &operator=(const LayoutSceneAdapter &) = delete;

    QSGNode *rootNode() const noexcept;
    std::size_t handleCount() const noexcept;
    uint64_t generation() const noexcept;

    QSGNode *releaseRootNode() noexcept;
    void adoptRootNode(QSGNode *root) noexcept;
    void resetRootNode(QSGNode *root) noexcept;

    bool rebuild(layout_scene_plan_t *plan);
    bool applyDiff(layout_scene_plan_t *oldPlan, layout_scene_plan_t *newPlan);
    bool applyDiff(layout_scene_plan_t *newPlan, layout_scene_diff_t *diff);

    const Handle *findHandle(layout_fragment_key_t key) const noexcept;
    QSGTransformNode *qsgNodeForKey(layout_fragment_key_t key) const noexcept;

private:
    using HandleMap = std::unordered_map<layout_fragment_key_t, Handle,
                                         LayoutFragmentKeyHash,
                                         LayoutFragmentKeyEqual>;

    struct SceneNodeSnapshot {
        layout_fragment_key_t key {};
        layout_fragment_key_t parentKey {};
        layout_fragment_key_t previousSiblingKey {};
        std::size_t slot = LAYOUT_SCENE_NODE_NONE;
        std::size_t sequence = 0;
        layout_fragment_placement_t placement = LAYOUT_FRAGMENT_PLACEMENT_NORMAL;
        int stackingOrder = 0;
        QRectF localRect;
        QRectF clipRect;
        layout_overflow_clip_t clipAxes = LAYOUT_OVERFLOW_CLIP_NONE;
        double opacity = 1.0;
        unsigned flags = 0;
        unsigned layerHints = 0;
        unsigned dirtyBits = 0;
        layout_native_embed_token_t nativeEmbedToken = 0;
        layout_point_t offset {};
        layout_transform_t transform {};
        std::size_t planIndex = LAYOUT_SCENE_NODE_NONE;
    };

    struct SceneUpdateEntry {
        layout_fragment_key_t key {};
        SceneNodeSnapshot node;
        bool remove = false;
        bool mustExist = false;
        bool mayCreate = false;
    };

    /*
     * One-shot retained reconciliation data, modeled after Flutter's
     * IndexedSlot(index, previousChild) placement idea.  This is not a render
     * command buffer and must stay private to the Qt adapter.
     */
    using SceneUpdateBatch = std::vector<SceneUpdateEntry>;

    static bool isEmptyKey(layout_fragment_key_t key) noexcept;
    static SceneNodeSnapshot snapshotForNode(const layout_scene_node_t *node,
                                             unsigned dirtyBits);
    static QMatrix4x4 matrixForSnapshot(const SceneNodeSnapshot &node);

    bool buildSceneUpdateBatch(layout_scene_plan_t *newPlan,
                               layout_scene_diff_t *diff,
                               SceneUpdateBatch &batch) const;
    bool validateSceneUpdateBatch(layout_scene_plan_t *newPlan,
                                  const SceneUpdateBatch &batch) const;
    bool applySceneUpdateBatch(const SceneUpdateBatch &batch);
    bool ensureNode(const SceneNodeSnapshot &node);
    bool createUnplacedNode(const SceneNodeSnapshot &node);
    bool insertNode(const SceneNodeSnapshot &node);
    bool updateNode(const SceneNodeSnapshot &node);
    void fillHandleFromSnapshot(Handle &handle,
                                const SceneNodeSnapshot &node);
    bool moveHandle(Handle &handle,
                    layout_fragment_key_t parentKey,
                    layout_fragment_key_t previousSiblingKey);
    void removeHandle(layout_fragment_key_t key);
    void removeHandleKeeping(layout_fragment_key_t key,
                             const std::vector<layout_fragment_key_t> &keptKeys);
    void collectSubtreeKeys(layout_fragment_key_t key,
                            std::vector<layout_fragment_key_t> &keys) const;
    void clear();

    const layout_scene_node_t *sceneNodeForKey(
        layout_scene_plan_t *plan,
        layout_fragment_key_t key) const noexcept;
    QSGNode *parentNodeForKey(layout_fragment_key_t parentKey) const noexcept;
    QSGNode *previousSiblingNodeForKey(
        layout_fragment_key_t previousSiblingKey) const noexcept;
    bool canInsertAfter(QSGNode *parent,
                        layout_fragment_key_t previousSiblingKey,
                        QSGNode *previousSibling) const noexcept;
    void insertQsgNode(QSGNode *parent,
                       QSGNode *previousSibling,
                       QSGNode *node);

    std::unique_ptr<QSGNode> root_;
    QSGNode *releasedRoot_ = nullptr;
    HandleMap handles_;
    uint64_t generation_ = 0;
};

class LayoutSceneItem : public QQuickItem {
public:
    explicit LayoutSceneItem(QQuickItem *parent = nullptr);
    ~LayoutSceneItem() override;

    LayoutSceneAdapter *adapter() noexcept;
    const LayoutSceneAdapter *adapter() const noexcept;

    bool scheduleRebuild(layout_scene_plan_t *plan);
    bool scheduleDiff(layout_scene_plan_t *oldPlan, layout_scene_plan_t *newPlan);
    bool scheduleDiff(layout_scene_plan_t *newPlan, layout_scene_diff_t *diff);

    bool rebuildScene(layout_scene_plan_t *plan);
    bool applySceneDiff(layout_scene_plan_t *oldPlan, layout_scene_plan_t *newPlan);
    bool applySceneDiff(layout_scene_plan_t *newPlan, layout_scene_diff_t *diff);

    void setScenePlan(layout_scene_plan_t *plan);
    layout_scene_plan_t *scenePlan() const noexcept;

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode,
                             UpdatePaintNodeData *data) override;

private:
    enum class PendingOperation {
        None,
        Rebuild,
        DiffPlans,
        Diff
    };

    LayoutSceneAdapter adapter_;
    PendingOperation pendingOperation_ = PendingOperation::None;
    layout_scene_plan_t *pendingOldPlan_ = nullptr;
    layout_scene_plan_t *pendingNewPlan_ = nullptr;
    layout_scene_diff_t *pendingDiff_ = nullptr;
};

} // namespace myqt

#endif // MYQT_LAYOUT_SCENE_ADAPTER_H_
