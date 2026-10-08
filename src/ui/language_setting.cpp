#include "ui/language_setting.h"

#include "i18n/i18n.h"
#include "i18n/i18n_resource.h"

#include <glib.h>

namespace language_setting {

std::vector<std::string> systemLocaleNames()
{
    std::vector<std::string> names;
    for (const char *const *n = g_get_language_names(); n && *n; ++n)
        names.emplace_back(*n);
    return names;
}

void installBundledCatalogs()
{
    i18n::setCatalogLoader(i18n::loadBundledCatalogText);
}

std::string applySetting(const std::string &setting, const std::vector<std::string> &localeNames)
{
    const std::string code = i18n::resolveSetting(setting, localeNames);
    i18n::setLanguage(code);
    return code;
}

std::string applySetting(const std::string &setting)
{
    return applySetting(setting, systemLocaleNames());
}

}  // namespace language_setting
