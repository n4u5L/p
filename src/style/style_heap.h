#pragma once

// Lexbor-backed allocation arena for computed style objects.
//
// Lexbor already uses lxb_css_memory_t + lexbor_mraw_t for CSS parse data.  The
// style engine reuses that allocator shape: objects are placement-new'd out of
// the arena and destroyed as a batch when the resolver is cleared/destroyed.

#include <cstddef>
#include <new>
#include <utility>
#include <vector>

#include "lexbor/core/mraw.h"
#include "lexbor/css/base.h"

namespace style {

class StyleHeap {
public:
  explicit StyleHeap(size_t prepareCount = 1024);
  ~StyleHeap();

  StyleHeap(const StyleHeap&) = delete;
  StyleHeap& operator=(const StyleHeap&) = delete;

  void clear();
  lxb_css_memory_t* memory() const {
    return memory_;
  }

  template <class T, class... Args>
  T* create(Args&&... args) {
    return createWithPhase<T>(DestructorPhase::StyleObject,
                              std::forward<Args>(args)...);
  }

  template <class T, class... Args>
  T* createData(Args&&... args) {
    return createWithPhase<T>(DestructorPhase::StyleData,
                              std::forward<Args>(args)...);
  }

private:
  enum class DestructorPhase { StyleObject,
                               StyleData };

  struct Destructor {
    void* ptr = nullptr;
    void (*destroy)(void*) noexcept = nullptr;
    DestructorPhase phase = DestructorPhase::StyleObject;
  };

  template <class T, class... Args>
  T* createWithPhase(DestructorPhase phase, Args&&... args) {
    void* mem = lexbor_mraw_alloc(memory_->mraw, sizeof(T));
    if (mem == nullptr) {
      throw std::bad_alloc();
    }

    T* obj = new (mem) T(std::forward<Args>(args)...);
    try {
      destructors_.push_back(
          Destructor{obj, [](void* p) noexcept { static_cast<T*>(p)->~T(); }, phase});
    } catch (...) {
      obj->~T();
      throw;
    }
    return obj;
  }

  void destroyPhase(DestructorPhase phase) noexcept;

  lxb_css_memory_t* memory_ = nullptr;
  std::vector<Destructor> destructors_;
};

} // namespace style
