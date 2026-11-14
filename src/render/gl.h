#pragma once

// Single place that includes OpenGL headers. Keep GL includes out of everything else.
// Non-Apple platforms will need a loader (glad) added here in a later milestone.
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#error "Add an OpenGL loader (e.g. glad) for this platform in src/render/gl.h"
#endif
