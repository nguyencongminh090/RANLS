#include "i18n/i18n_resource.h"

#include <gio/gio.h>

namespace i18n {

std::string loadBundledCatalogText(std::string_view code)
{
    std::string path = "/org/ranls/lang/" + std::string(code) + ".tsv";
    GBytes *bytes = g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
    if (!bytes)
        return {};
    gsize size = 0;
    const char *data = static_cast<const char *>(g_bytes_get_data(bytes, &size));
    std::string out(data ? data : "", data ? size : 0);
    g_bytes_unref(bytes);
    return out;
}

}  // namespace i18n
