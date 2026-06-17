#pragma once

// 基于 lexbor 分配器的 computed style arena。
//
// Computed style 的生命周期独立于 CSS parser/stylesheet memory：
//   * 固定大小、平凡析构的 style 对象走 lexbor_dobject_t 对象池；
//   * 字符串和后续 calc 这类变长数据走 lexbor_mraw_t。
//
// 命名跟随 lexbor 习惯（mraw/dobject），用适度 C++ 的薄 RAII 封装这两个分配器：
// 构造时建池、析构时整体释放、clear() 复用底层内存。
//
// 易错点：池只接受平凡析构类型（static_assert 保证）。非平凡 C++ 容器会让
// arena 的批量 clean 退回到逐对象析构，应先改成 arena 友好的数据形状。

#include <array>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

#include "lexbor/core/dobject.h"
#include "lexbor/core/mraw.h"

namespace style {

inline constexpr char kFontFamilySerif[] = "serif";
inline constexpr char kFontFamilySansSerif[] = "sans-serif";
inline constexpr char kFontFamilyMonospace[] = "monospace";
inline constexpr char kFontFamilyCursive[] = "cursive";
inline constexpr char kFontFamilyFantasy[] = "fantasy";
inline constexpr char kFontFamilySystemUi[] = "system-ui";
inline constexpr char kFontFamilyEmoji[] = "emoji";
inline constexpr char kFontFamilyMath[] = "math";
inline constexpr char kFontFamilyFangsong[] = "fangsong";
inline constexpr char kFontFamilyUiRounded[] = "ui-rounded";

class ComputedStyle;
struct CustomPropEntry;
struct CustomPropMap;
struct StyleInheritedData;
struct StyleRareInheritedData;
struct StyleBoxData;
struct StyleSurroundData;
struct StyleBackgroundData;
struct StyleSvgData;
struct StyleVisualData;

enum class StylePoolKind : size_t {
  ComputedStyle,
  CustomPropMap,
  CustomPropEntry,
  InheritedData,
  RareInheritedData,
  BoxData,
  SurroundData,
  BackgroundData,
  SvgData,
  VisualData,
  Count
};

template <class T>
struct StylePoolTraits;

#define STYLE_POOL_TRAIT(Type, KindValue)                           \
  template <>                                                       \
  struct StylePoolTraits<Type> {                                    \
    static constexpr StylePoolKind kind = StylePoolKind::KindValue; \
  }

STYLE_POOL_TRAIT(ComputedStyle, ComputedStyle);
STYLE_POOL_TRAIT(CustomPropMap, CustomPropMap);
STYLE_POOL_TRAIT(CustomPropEntry, CustomPropEntry);
STYLE_POOL_TRAIT(StyleInheritedData, InheritedData);
STYLE_POOL_TRAIT(StyleRareInheritedData, RareInheritedData);
STYLE_POOL_TRAIT(StyleBoxData, BoxData);
STYLE_POOL_TRAIT(StyleSurroundData, SurroundData);
STYLE_POOL_TRAIT(StyleBackgroundData, BackgroundData);
STYLE_POOL_TRAIT(StyleSvgData, SvgData);
STYLE_POOL_TRAIT(StyleVisualData, VisualData);

#undef STYLE_POOL_TRAIT

class StyleArena {
public:
  explicit StyleArena(size_t prepareCount = 1024);
  ~StyleArena();

  StyleArena(const StyleArena&) = delete;
  StyleArena& operator=(const StyleArena&) = delete;

  void clear();

  template <class T>
  T* allocPod() {
    return constructPod<T>();
  }

  template <class T>
  T* allocPod(const T& proto) {
    return constructPod<T>(proto);
  }

  template <class T, class... Args>
  T* create(Args&&... args) {
    return constructPod<T>(std::forward<Args>(args)...);
  }

  template <class T, class... Args>
  T* createData(Args&&... args) {
    return constructPod<T>(std::forward<Args>(args)...);
  }

  template <class T>
  void freePod(T* obj) noexcept {
    static_assert(std::is_trivially_destructible_v<T>,
                  "StyleArena pools only store trivially destructible objects");
    if (obj == nullptr) {
      return;
    }
    (void)lexbor_dobject_free(poolFor<T>(), obj);
  }

  void* allocRaw(size_t n);
  const char* internString(const char* s, size_t len);
  const char* internString(const char* s) {
    return internString(s, std::strlen(s));
  }

private:
  struct InternEntry {
    const char* data = nullptr;
    size_t length = 0;
    InternEntry* next = nullptr;
  };

  template <class T, class... Args>
  T* constructPod(Args&&... args) {
    static_assert(std::is_trivially_destructible_v<T>,
                  "StyleArena pools only store trivially destructible objects");
    void* mem = lexbor_dobject_alloc(poolFor<T>());
    if (mem == nullptr) {
      throw std::bad_alloc();
    }
    return new (mem) T(std::forward<Args>(args)...);
  }

  template <class T>
  lexbor_dobject_t* poolFor() {
    return ensurePool(StylePoolTraits<T>::kind, sizeof(T));
  }

  lexbor_dobject_t* ensurePool(StylePoolKind kind, size_t structSize);
  const char* canonicalString(const char* s, size_t len) const;

  lexbor_mraw_t* raw_ = nullptr;
  std::array<lexbor_dobject_t*, static_cast<size_t>(StylePoolKind::Count)> pools_{};
  size_t prepareCount_ = 0;
  InternEntry* internHead_ = nullptr;
};

// 过渡别名：历史代码沿用 StyleHeap 名称。新代码请直接用 StyleArena。
using StyleHeap = StyleArena;

} // namespace style
