#pragma once

// I18N-03: glue between the persisted `language=` setting and the i18n core.
// The core (src/i18n/i18n.h) is GTK-free; the desktop locale lookup
// (g_get_language_names) lives here in the UI layer.

#include <string>
#include <vector>

namespace language_setting {

/// Locale names in preference order from GLib ("vi_VN.UTF-8", "vi_VN", "vi", "C").
std::vector<std::string> systemLocaleNames();

/// Startup: install the bundled-catalog loader. Call once, before any window.
void installBundledCatalogs();

/// Resolve `setting` ("system" | "en" | "vi" | anything) against `localeNames`
/// and switch the active language. Returns the concrete code that was selected.
/// Long-lived UI refreshes through the i18n language listeners.
std::string applySetting(const std::string &setting, const std::vector<std::string> &localeNames);

/// applySetting() with the real desktop locale.
std::string applySetting(const std::string &setting);

}  // namespace language_setting
