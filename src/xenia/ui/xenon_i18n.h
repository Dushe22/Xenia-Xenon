/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_XENON_I18N_H_
#define XENIA_UI_XENON_I18N_H_

#include <string>

namespace xe {
namespace ui {
namespace i18n {

// Languages of the Xenon interface. English is the source language.
enum class Language {
  kEnglish,
  kSpanish,
  kPortuguese,
  kFrench,
  kGerman,
  kItalian,
  kJapanese,
  kKorean,
  kChinese,
  kRussian,
  kCount,
};

// Language picked in settings (ui_language), or the system one for "auto".
Language CurrentLanguage();

// Translation of an English interface string, or the string itself when there
// is none. The result stays valid for the lifetime of the app.
const char* Tr(const char* text);

// Font that provides the glyphs English text doesn't use, or null when the
// interface font already covers the language.
const char* FontFile(Language language, bool bold);

// Every character the interface may draw in this language, for building font
// glyph ranges.
std::string GlyphText(Language language);

// Name of the language in its own script, e.g. "Deutsch".
const char* NativeName(Language language);

// Length of the UTF-8 sequence that starts at text.
inline int Utf8Length(const char* text) {
  const unsigned char c = static_cast<unsigned char>(*text);
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

}  // namespace i18n
}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_XENON_I18N_H_
