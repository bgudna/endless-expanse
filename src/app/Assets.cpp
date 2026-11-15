#include "app/Assets.h"

#include "core/Log.h"

#include <fstream>
#include <sstream>
#include <string>

namespace ee {

namespace {
bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
} // namespace

std::vector<AlienType> loadAlienTypes() {
    const char* candidates[] = {"assets/data/aliens.json", EE_ASSET_DIR "/data/aliens.json"};
    for (const char* path : candidates) {
        std::string text;
        if (!readFile(path, text)) continue;
        std::string error;
        std::vector<AlienType> types = parseAlienTypes(text, &error);
        if (!types.empty()) {
            EE_LOG_INFO("loaded %zu alien types from %s", types.size(), path);
            return types;
        }
        EE_LOG_ERROR("%s: %s", path, error.c_str());
    }
    EE_LOG_ERROR("aliens.json not usable; falling back to a built-in raider");
    AlienType t;
    t.id = "fallback";
    t.name = "RAIDER";
    return {t};
}

} // namespace ee
