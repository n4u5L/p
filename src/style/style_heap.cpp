#include "style_heap.h"

#include <stdexcept>

namespace style {

StyleHeap::StyleHeap(size_t prepareCount) {
  memory_ = lxb_css_memory_create();
  if (memory_ == nullptr) {
    throw std::bad_alloc();
  }

  if (lxb_css_memory_init(memory_, prepareCount) != LXB_STATUS_OK) {
    memory_ = lxb_css_memory_destroy(memory_, true);
    throw std::bad_alloc();
  }
}

StyleHeap::~StyleHeap() {
  clear();
  if (memory_ != nullptr) {
    memory_ = lxb_css_memory_ref_dec_destroy(memory_);
  }
}

void StyleHeap::clear() {
  destroyPhase(DestructorPhase::StyleObject);
  destroyPhase(DestructorPhase::StyleData);
  destructors_.clear();

  if (memory_ != nullptr) {
    lxb_css_memory_clean(memory_);
  }
}

void StyleHeap::destroyPhase(DestructorPhase phase) noexcept {
  for (auto it = destructors_.rbegin(); it != destructors_.rend(); ++it) {
    if (it->phase == phase && it->destroy != nullptr) {
      it->destroy(it->ptr);
      it->destroy = nullptr;
      it->ptr = nullptr;
    }
  }
}

} // 命名空间 style
