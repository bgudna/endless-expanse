#pragma once

#include "sim/AlienType.h"

#include <vector>

namespace ee {

// Loads assets/data/aliens.json (looks in ./assets, then the source-tree asset directory).
// Logs an error and falls back to a single built-in raider if the file is missing or invalid.
std::vector<AlienType> loadAlienTypes();

} // namespace ee
