#ifndef LAYOUT_ALGORITHM_H
#define LAYOUT_ALGORITHM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "layout/layout.h"

lxb_status_t
layout_algorithm_build_fragments(layout_tree_t *tree,
                                 layout_result_t *result,
                                 const layout_fragment_builder_t *builder);

#ifdef __cplusplus
}
#endif

#endif /* LAYOUT_ALGORITHM_H */
