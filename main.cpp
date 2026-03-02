#include "globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include "fairvLayout.hpp"
#include <typeinfo>

static std::unique_ptr<CHyprFairvTiledAlgorithm> g_pFairvTiledAlgorithm;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void deleteWorkspaceData(int ws) {
    if (g_pFairvTiledAlgorithm)
        g_pFairvTiledAlgorithm->removeWorkspaceData(ws);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:fairv:layout:no_gaps_when_only", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:fairv:layout:special_scale_factor", Hyprlang::FLOAT{0.8f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:fairv:layout:cols", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:fairv:layout:rows", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:fairv:layout:auto_adjust", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:fairv:layout:inherit_fullscreen", Hyprlang::INT{1});

    g_pFairvTiledAlgorithm = std::make_unique<CHyprFairvTiledAlgorithm>();

    HyprlandAPI::addTiledAlgo(PHANDLE, "fairv", &typeid(CHyprFairvTiledAlgorithm), [&]() { return makeUnique<CHyprFairvTiledAlgorithm>(); });

    HyprlandAPI::reloadConfig();

    return {"hyprfairv", "Fair vertical tiling layout", "hyprfairv", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}
