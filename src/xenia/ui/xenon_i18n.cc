/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/xenon_i18n.h"

#include <array>
#include <string_view>
#include <unordered_map>

#include "xenia/base/cvar.h"
#include "xenia/base/platform.h"

#if XE_PLATFORM_WIN32
#include "xenia/base/platform_win.h"
#endif

DEFINE_string(ui_language, "auto",
              "Language of the Xenon interface: auto (system language), en, "
              "es, pt, fr, de, it, ja, ko, zh or ru.",
              "UI");

namespace xe {
namespace ui {
namespace i18n {

namespace {

constexpr size_t kTranslations = size_t(Language::kCount) - 1;

struct Entry {
  const char* english;
  // Spanish, Portuguese, French, German, Italian, Japanese, Korean, Chinese
  // (simplified), Russian.
  std::array<const char*, kTranslations> text;
};

const Entry kEntries[] = {
#include "xenia/ui/xenon_strings.inc"
};

struct LanguageInfo {
  const char* code;
  const char* native_name;
  const char* font;
  const char* bold_font;
};

constexpr LanguageInfo kLanguages[] = {
    {"en", "English", nullptr, nullptr},
    {"es", "Español", nullptr, nullptr},
    {"pt", "Português", nullptr, nullptr},
    {"fr", "Français", nullptr, nullptr},
    {"de", "Deutsch", nullptr, nullptr},
    {"it", "Italiano", nullptr, nullptr},
    {"ja", "日本語", "Assets/fonts/NotoSansJP-Medium.ttf",
     "Assets/fonts/NotoSansJP-Bold.ttf"},
    {"ko", "한국어", "Assets/fonts/NotoSansKR-Medium.ttf",
     "Assets/fonts/NotoSansKR-Bold.ttf"},
    {"zh", "简体中文", "Assets/fonts/NotoSansSC-Medium.ttf",
     "Assets/fonts/NotoSansSC-Bold.ttf"},
    {"ru", "Русский", "Assets/fonts/NotoSans-Medium.ttf",
     "Assets/fonts/NotoSans-Bold.ttf"},
};
static_assert(std::size(kLanguages) == size_t(Language::kCount));

Language FromCode(std::string_view code) {
  for (size_t i = 0; i < std::size(kLanguages); ++i) {
    if (code.substr(0, 2) == kLanguages[i].code) {
      return Language(i);
    }
  }
  return Language::kEnglish;
}

Language SystemLanguage() {
#if XE_PLATFORM_WIN32
  wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
  if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH)) {
    char code[3] = {char(name[0]), char(name[0] ? name[1] : 0), 0};
    return FromCode(code);
  }
#endif
  return Language::kEnglish;
}

}  // namespace

Language CurrentLanguage() {
  static std::string cached_setting;
  static Language cached_language = Language::kEnglish;
  static bool cached = false;
  if (!cached || cached_setting != cvars::ui_language) {
    cached_setting = cvars::ui_language;
    cached_language = cached_setting == "auto" ? SystemLanguage()
                                               : FromCode(cached_setting);
    cached = true;
  }
  return cached_language;
}

const char* Tr(const char* text) {
  const Language language = CurrentLanguage();
  if (!text || language == Language::kEnglish) {
    return text;
  }
  static const auto table = [] {
    std::unordered_map<std::string_view, const Entry*> map;
    for (const Entry& entry : kEntries) {
      map.emplace(entry.english, &entry);
    }
    return map;
  }();
  auto it = table.find(text);
  if (it == table.end()) {
    return text;
  }
  const char* translated = it->second->text[size_t(language) - 1];
  return translated && *translated ? translated : text;
}

const char* FontFile(Language language, bool bold) {
  const LanguageInfo& info = kLanguages[size_t(language)];
  return bold ? info.bold_font : info.font;
}

std::string GlyphText(Language language) {
  std::string text = NativeName(language);
  if (language == Language::kEnglish) {
    return text;
  }
  for (const Entry& entry : kEntries) {
    if (const char* translated = entry.text[size_t(language) - 1]) {
      text += translated;
    }
  }
  return text;
}

const char* NativeName(Language language) {
  return kLanguages[size_t(language)].native_name;
}

}  // namespace i18n
}  // namespace ui
}  // namespace xe
