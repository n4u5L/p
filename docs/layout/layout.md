# DOM 与 layout_object 生命周期绑定 API

## 设计原则

`layout_object_t` 的生命周期入口必须是 DOM node。

每个可渲染 DOM node 通过 `lxb_dom_node_t::layout_object` 持有自己的
`layout_object_t *`。这与 Blink 的 `Node::GetLayoutObject()` /
`Node::SetLayoutObject()` 边界一致：节点负责 attach/detach layout tree；
`layout_object` 只是被节点持有的布局表达，不应反过来成为生命周期入口。

因此：

- 创建、复用、销毁 DOM-backed layout object 都从 `lxb_dom_node_t *` API 发起。
- `layout_object_t` 内部可以被清理状态，但这只是 DOM detach 的实现细节。
- anonymous layout object 不绑定 DOM node，不写入 `node->layout_object`。

## DOM 槽语义

`lxb_dom_node_t::layout_object` 是布局层专用槽。

它不同于 `node->user`：

- `user` 仍保留给外部集成或调试用途。
- `layout_object` 只保存 `layout_object_t *`。
- clone/copy DOM node 时不能继承该指针，新节点必须重新 attach。
- layout/core 销毁或清理时必须清掉仍指向本 layout 的 DOM 槽。

## API

```c
// 宏 `node->layout_object`。
layout_object_t *
layout_dom_node_layout_object(lxb_dom_node_t *node);

// 确保 DOM node 拥有当前 `layout_t` 下的 layout object。
lxb_status_t
layout_dom_node_ensure_layout_object(layout_t *layout,
                                     lxb_dom_node_t *node,
                                     layout_object_t **out_object);
```

行为：

- `display:none`：detach 整个 subtree，`out_object` 返回 `NULL`。
- `display:contents`：detach 当前 node 的 layout object，但保留子节点后续 attach 机会。
- 无 computed style 或非可渲染节点：detach subtree，`out_object` 返回 `NULL`。
- 已有同一 `layout_t` 的对象：复用并更新 computed style 引用。2
- 已有其他 `layout_t` 的对象：先从 DOM node detach，再为当前 `layout_t` 创建新对象。

```c
void
layout_dom_node_detach_layout_tree(lxb_dom_node_t *node);
```

对应 Blink `Node::DetachLayoutTree()` 的最小 C 语义：销毁/解绑当前 node 的
layout object，清 `node->layout_object`，释放 layout object 持有的 style 引用，
断开 parent/sibling 关系。

```c
void
layout_dom_node_detach_layout_subtree(lxb_dom_node_t *root_node);
```

后序遍历 detach 整个 DOM subtree。DOM 移除、`display:none` 生效、文档销毁前清理
应走这个入口。

## layout_tree_build 的使用方式

`layout_tree_build(tree, root_node)` 不再盲目创建新 layout object。它应：

1. 从 DOM node 读取 computed style。
2. 对 `display:none` 调用 `layout_dom_node_detach_layout_subtree()` 并跳过 subtree。
3. 对 `display:contents` 调用 `layout_dom_node_detach_layout_tree()`，继续处理子节点。
4. 对普通可渲染 element 调用 `layout_dom_node_ensure_layout_object()`。
5. 复用 DOM node 已持有的对象，并由 tree builder 重新建立本次 layout tree 的 parent/sibling/child-list 关系。

## 样式变化

CSS computed style 换代后，调用方应对受影响 node 调用：

```c
layout_dom_node_ensure_layout_object(layout, node, &object);
```

若新 style 仍需要 layout object，则复用旧对象并更新 style 引用，同时标记 layout dirty。

若新 style 变为 `display:none`，则 detach subtree。

若新 style 变为 `display:contents`，则 detach 当前 node 的 layout object，但子节点可继续参与后续 tree build。

## 禁止事项

- 不要把 DOM-backed layout object 的创建 API 暴露成 `layout_object_create_for_node()`。
- 不要从 `layout_object_t *` 发起 attach/detach 生命周期操作。
- 不要把 `QSGNode *`、Qt 对象、material、texture 或 retained adapter handle 放进 DOM 槽或 `layout_object_t`。
- 不要让 cloned DOM node 继承原 node 的 `layout_object` 指针。
