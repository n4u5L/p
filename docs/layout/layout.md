# layout 模块
dom和layoutobject的生命周期绑定

## DOM 槽语义
`lxb_dom_node_t::layout_object` 是布局层专用槽。  
它不同于 `node->user`：
- `user` 仍保留给外部集成或调试用途。
- `layout_object` 只保存 `layout_object_t *`。
- clone/copy DOM node 时不能继承该指针，新节点必须重新 attach。
- layout/core 销毁或清理时必须清掉仍指向本 layout 的 DOM 槽。

## API
```c
// 宏 平凡的getter,不会检查指针是否为空。
layout_object_t *
layout_dom_node_layout_object(lxb_dom_node_t *node);

// 把一个 DOM 子树按 computed style 转换并挂接进 layout_tree_t 的 layout object tree
layout_dom_node_attach_layout_tree(layout_tree_t* tree, lxb_dom_node_t* node,
layout_object_t* parent);

void
layout_dom_node_detach_layout_tree(lxb_dom_node_t *node);

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
