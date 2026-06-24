# layout 模块
dom和layoutobject的生命周期绑定

## LayoutObject
LayoutText LayoutInline LayoutBlock LayoutBlockFlow LayoutFlexibleBox LayoutGrid LayoutImage LayoutSVGRoot LayoutHTMLCanvas LayoutTable

## Fragment
isCssBox: !isLineBox && !isFragmentainerBox  
const ComputedStyle& Style() const {
    return layout_object_->EffectiveStyle(GetStyleVariant());
}

## 重要方法
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