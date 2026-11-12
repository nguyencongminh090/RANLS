#pragma once

// Shared GLib log capture for the real-widget UI tests (ranls-gui-ui-tests).
//
// GTK4 logs through the structured-logging writer, which
// g_log_set_default_handler does not see, and g_log_set_writer_func() may be
// installed only ONCE per process -- so every test that needs to assert on
// GTK warnings must share this one writer (extracted from test_ui11 for
// UI-23). It stays installed and records MESSAGE fields while an instance is
// alive; output is still forwarded to the default writer.

#include <glib.h>

#include <string>
#include <vector>

struct WarningCounter {
    static inline std::vector<std::string> messages;
    static inline bool active = false;
    static inline bool installed = false;

    WarningCounter()
    {
        if (!installed) {
            installed = true;
            g_log_set_writer_func(
                [](GLogLevelFlags level, const GLogField *fields, gsize n, gpointer) {
                    if (active)
                        for (gsize i = 0; i < n; ++i)
                            if (std::string(fields[i].key) == "MESSAGE" && fields[i].value)
                                messages.emplace_back(static_cast<const char *>(fields[i].value));
                    return g_log_writer_default(level, fields, n, nullptr);
                },
                nullptr, nullptr);
        }
        messages.clear();
        active = true;
    }
    ~WarningCounter() { active = false; }

    /// Captured messages containing `needle` (unrelated host noise such as
    /// gvfs/EGL lines is filtered out by choosing a specific needle).
    static std::vector<std::string> containing(const std::string &needle)
    {
        std::vector<std::string> out;
        for (const auto &m : messages)
            if (m.find(needle) != std::string::npos) out.push_back(m);
        return out;
    }

    static std::vector<std::string> markupErrors() { return containing("markup"); }
};
