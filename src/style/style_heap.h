#pragma once

// 基于 Lexbor 分配器形态的 computed style arena。
//
// Lexbor 解析 CSS 时已经使用 lxb_css_memory_t + lexbor_mraw_t。样式引擎沿用这种
// 模式：对象通过 placement new 放进 arena，在 resolver 清理/析构时批量析构和释放。
//
// 易错点：arena 负责内存生命周期，但 C++ 对象析构仍要显式记录并调用，尤其是
// std::string/std::vector/unordered_map 这类有析构逻辑的类型。
// TODO：如果后续所有样式数据都改成 POD 或 lexbor 字符串，可进一步减少析构表开销。

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

} // 命名空间 style
