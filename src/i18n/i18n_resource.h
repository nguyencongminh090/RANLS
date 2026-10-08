#pragma once

// I18N-01: loads a language's catalog text from the GResource
// (/org/ranls/lang/<code>.tsv). Kept apart from i18n.h so the core stays
// standard-library only; this one needs GIO (no gtk, no display server).

#include <string>
#include <string_view>

namespace i18n {

/// Catalog text for `code`, or "" if the language has no bundled catalog.
/// I18N-03 wires it: `setCatalogLoader(loadBundledCatalogText)`.
std::string loadBundledCatalogText(std::string_view code);

}  // namespace i18n
