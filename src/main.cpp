#include "application.h"
#include "version.h"

#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{
    // REL-02: --version / -v must work before any GTK/Gio initialization, with
    // no display server. An early argv scan is deliberately enough for one or
    // two flags — no argument-parsing dependency (see docs/todo/REL-02-*).
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            std::printf("%s\n", APP_VERSION);
            return 0;
        }
    }

    auto app = RapfiApplication::create();
    return app->run(argc, argv);
}
