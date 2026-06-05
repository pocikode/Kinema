#include "modules/MarkerConfigIO.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
constexpr int kSchemaVersion = 1;

// Copy a std::string into a fixed char buffer, always NUL-terminating.
void CopyToBuf(char *dst, size_t cap, const std::string &src)
{
    std::strncpy(dst, src.c_str(), cap - 1);
    dst[cap - 1] = 0;
}
} // namespace

bool SaveMarkerConfig(const std::string &path, const std::vector<MarkerSlot> &markers)
{
    try
    {
        json arr = json::array();
        for (const auto &s : markers)
        {
            arr.push_back({
                {"name", s.name},
                {"color", {s.overlayColor[0], s.overlayColor[1], s.overlayColor[2], s.overlayColor[3]}},
                {"hsv", {s.hsv.hMin, s.hsv.hMax, s.hsv.sMin, s.hsv.sMax, s.hsv.vMin, s.hsv.vMax}},
                {"dualHue", s.hsv.dualHue},
                {"hsv2", {s.hsv.hMin2, s.hsv.hMax2}},
                {"rgb", {s.rgb.rgMin, s.rgb.rgMax, s.rgb.rbMin, s.rgb.rbMax, s.rgb.vMin}},
                {"binding", static_cast<int>(s.binding)},
                {"boneName", s.boneName},
                {"ikRootBone", s.ikRootBone},
                {"ikMidBone", s.ikMidBone},
                {"upperArmMarkerSlot", s.upperArmMarkerSlot},
                {"foreArmMarkerSlot", s.foreArmMarkerSlot},
            });
        }
        json j = {{"version", kSchemaVersion}, {"markers", arr}};

        std::ofstream f(path);
        if (!f)
            return false;
        f << j.dump(2);
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "SaveMarkerConfig failed: " << e.what() << std::endl;
        return false;
    }
}

bool LoadMarkerConfig(const std::string &path, std::vector<MarkerSlot> &markers)
{
    try
    {
        std::ifstream f(path);
        if (!f)
            return false;

        json j;
        f >> j;
        if (j.value("version", 0) != kSchemaVersion)
        {
            std::cerr << "LoadMarkerConfig: unsupported schema version (expected " << kSchemaVersion << ")"
                      << std::endl;
            return false;
        }

        std::vector<MarkerSlot> loaded;
        for (const auto &e : j.at("markers"))
        {
            MarkerSlot s;
            CopyToBuf(s.name, sizeof(s.name), e.value("name", std::string("marker")));

            auto col = e.at("color");
            for (int i = 0; i < 4; ++i)
                s.overlayColor[i] = col[i].get<float>();

            auto hsv = e.at("hsv");
            s.hsv.hMin = hsv[0];
            s.hsv.hMax = hsv[1];
            s.hsv.sMin = hsv[2];
            s.hsv.sMax = hsv[3];
            s.hsv.vMin = hsv[4];
            s.hsv.vMax = hsv[5];

            // Optional (added later): backward-compatible with single-band configs.
            s.hsv.dualHue = e.value("dualHue", false);
            if (e.contains("hsv2"))
            {
                auto hsv2 = e.at("hsv2");
                s.hsv.hMin2 = hsv2[0];
                s.hsv.hMax2 = hsv2[1];
            }

            // Optional (added later): RGB-ratio band. Missing keeps defaults.
            if (e.contains("rgb"))
            {
                auto rgb = e.at("rgb");
                s.rgb.rgMin = rgb[0];
                s.rgb.rgMax = rgb[1];
                s.rgb.rbMin = rgb[2];
                s.rgb.rbMax = rgb[3];
                s.rgb.vMin = rgb[4];
            }

            s.binding = static_cast<BindingKind>(e.value("binding", 0));
            CopyToBuf(s.boneName, sizeof(s.boneName), e.value("boneName", std::string("")));
            CopyToBuf(s.ikRootBone, sizeof(s.ikRootBone), e.value("ikRootBone", std::string("")));
            CopyToBuf(s.ikMidBone, sizeof(s.ikMidBone), e.value("ikMidBone", std::string("")));
            s.upperArmMarkerSlot = e.value("upperArmMarkerSlot", -1);
            s.foreArmMarkerSlot = e.value("foreArmMarkerSlot", -1);

            loaded.push_back(s);
        }

        markers = std::move(loaded);
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "LoadMarkerConfig failed: " << e.what() << std::endl;
        return false;
    }
}
