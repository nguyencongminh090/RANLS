#include "application.h"

int main(int argc, char *argv[])
{
    auto app = RapfiApplication::create();
    return app->run(argc, argv);
}
