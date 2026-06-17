#include "style_arena.h"

#include <cstring>
#include <stdexcept>

namespace style {

StyleArena::StyleArena(size_t prepareCount)
    : prepareCount_(prepareCount < 64 ? 64 : prepareCount) {
  raw_ = lexbor_mraw_create();
  if (raw_ == nullptr) {
    throw std::bad_alloc();
  }

  if (lexbor_mraw_init(raw_, 4096) != LXB_STATUS_OK) {
    raw_ = lexbor_mraw_destroy(raw_, true);
    throw std::bad_alloc();
  }
}

StyleArena::~StyleArena() {
  for (lexbor_dobject_t*& pool : pools_) {
    pool = lexbor_dobject_destroy(pool, true);
  }
  raw_ = lexbor_mraw_destroy(raw_, true);
}

void* StyleArena::allocRaw(size_t n) {
  void* mem = lexbor_mraw_alloc(raw_, n);
  if (mem == nullptr) {
    throw std::bad_alloc();
  }
  return mem;
}

const char* StyleArena::internString(const char* s, size_t len) {
  if (const char* canonical = canonicalString(s, len)) {
    return canonical;
  }

  char* copy = static_cast<char*>(allocRaw(len + 1));
  std::memcpy(copy, s, len);
  copy[len] = '\0';

  InternEntry* entry = static_cast<InternEntry*>(allocRaw(sizeof(InternEntry)));
  entry->data = copy;
  entry->length = len;
  entry->next = internHead_;
  internHead_ = entry;

  return copy;
}

const char* StyleArena::canonicalString(const char* s, size_t len) const {
  if (len == sizeof(kFontFamilySerif) - 1 &&
      std::memcmp(s, kFontFamilySerif, len) == 0) {
    return kFontFamilySerif;
  }
  if (len == sizeof(kFontFamilySansSerif) - 1 &&
      std::memcmp(s, kFontFamilySansSerif, len) == 0) {
    return kFontFamilySansSerif;
  }
  if (len == sizeof(kFontFamilyMonospace) - 1 &&
      std::memcmp(s, kFontFamilyMonospace, len) == 0) {
    return kFontFamilyMonospace;
  }
  if (len == sizeof(kFontFamilyCursive) - 1 &&
      std::memcmp(s, kFontFamilyCursive, len) == 0) {
    return kFontFamilyCursive;
  }
  if (len == sizeof(kFontFamilyFantasy) - 1 &&
      std::memcmp(s, kFontFamilyFantasy, len) == 0) {
    return kFontFamilyFantasy;
  }
  if (len == sizeof(kFontFamilySystemUi) - 1 &&
      std::memcmp(s, kFontFamilySystemUi, len) == 0) {
    return kFontFamilySystemUi;
  }
  if (len == sizeof(kFontFamilyEmoji) - 1 &&
      std::memcmp(s, kFontFamilyEmoji, len) == 0) {
    return kFontFamilyEmoji;
  }
  if (len == sizeof(kFontFamilyMath) - 1 &&
      std::memcmp(s, kFontFamilyMath, len) == 0) {
    return kFontFamilyMath;
  }
  if (len == sizeof(kFontFamilyFangsong) - 1 &&
      std::memcmp(s, kFontFamilyFangsong, len) == 0) {
    return kFontFamilyFangsong;
  }
  if (len == sizeof(kFontFamilyUiRounded) - 1 &&
      std::memcmp(s, kFontFamilyUiRounded, len) == 0) {
    return kFontFamilyUiRounded;
  }

  for (InternEntry* entry = internHead_; entry != nullptr; entry = entry->next) {
    if (entry->length == len && std::memcmp(entry->data, s, len) == 0) {
      return entry->data;
    }
  }

  return nullptr;
}

void StyleArena::clear() {
  for (lexbor_dobject_t* pool : pools_) {
    if (pool != nullptr) {
      lexbor_dobject_clean(pool);
    }
  }

  if (raw_ != nullptr) {
    lexbor_mraw_clean(raw_);
  }
  internHead_ = nullptr;
}

lexbor_dobject_t* StyleArena::ensurePool(StylePoolKind kind, size_t structSize) {
  const size_t index = static_cast<size_t>(kind);
  lexbor_dobject_t*& pool = pools_[index];
  if (pool != nullptr) {
    return pool;
  }

  pool = lexbor_dobject_create();
  if (pool == nullptr) {
    throw std::bad_alloc();
  }

  if (lexbor_dobject_init(pool, prepareCount_, structSize) != LXB_STATUS_OK) {
    pool = lexbor_dobject_destroy(pool, true);
    throw std::bad_alloc();
  }

  return pool;
}

} // namespace style
