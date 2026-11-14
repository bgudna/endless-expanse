#include "app/Application.h"
#include "core/Log.h"

#include <SDL.h>

int main(int, char**) {
    ee::Application app;
    if (!app.init()) {
        EE_LOG_ERROR("Initialization failed");
        return 1;
    }
    int code = app.run();
    SDL_Quit();
    return code;
}
