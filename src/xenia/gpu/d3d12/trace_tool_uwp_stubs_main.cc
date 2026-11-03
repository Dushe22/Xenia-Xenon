// Desktop stand-ins for symbols the shared libraries expect from the UWP app
// (xenia-canary-uwp), so the PC GPU trace tools can link.

#include <string>

#include "xenia/base/cvar.h"

DEFINE_string(ui_text_effect, "none", "Trace tool stub.", "UI");
DEFINE_string(ui_text_color, "white", "Trace tool stub.", "UI");
DEFINE_string(ui_border_color, "xenon", "Trace tool stub.", "UI");
DEFINE_double(ui_text_brightness, 1.0, "Trace tool stub.", "UI");
DEFINE_double(ui_text_contrast, 1.0, "Trace tool stub.", "UI");

namespace UWP {
std::string GetLocalState() { return "."; }
void LaunchUri(const std::string&) {}
void SetUIOpen(bool) {}
void ShowKeyboard() {}
}  // namespace UWP
