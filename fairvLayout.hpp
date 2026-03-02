#pragma once

#include "globals.hpp"
#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <vector>
#include <list>
#include <memory>
#include <cmath>

namespace Layout::Tiled {

    struct SFairvNodeData {
        SP<ITarget> target;
        int         gridIndex   = 0;
        WORKSPACEID workspaceID = WORKSPACE_INVALID;
    };

    struct SFairvWorkspaceData {
        WORKSPACEID workspaceId          = WORKSPACE_INVALID;
        int         no_gaps_when_only    = 0;
        float       special_scale_factor = 0.8f;
        bool        inherit_fullscreen   = true;
    };

    class CFairvAlgorithm : public ITiledAlgorithm {
      public:
        CFairvAlgorithm()          = default;
        virtual ~CFairvAlgorithm() = default;

        virtual void                             newTarget(SP<ITarget> target) override;
        virtual void                             movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt) override;
        virtual void                             removeTarget(SP<ITarget> target) override;
        virtual void                             resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE) override;
        virtual void                             recalculate() override;
        virtual SP<ITarget>                      getNextCandidate(SP<ITarget> old) override;
        virtual std::expected<void, std::string> layoutMsg(const std::string_view& sv) override;
        virtual std::optional<Vector2D>          predictSizeForNewTarget() override;
        virtual void                             swapTargets(SP<ITarget> a, SP<ITarget> b) override;
        virtual void                             moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) override;

      private:
        std::vector<SP<SFairvNodeData>> m_nodesData;
        SFairvWorkspaceData             m_workspaceData;

        SP<SFairvNodeData>              getNodeFromTarget(SP<ITarget> target);
        SP<SFairvNodeData>              getNodeFromWindow(PHLWINDOW pWindow);
        void                            applyNodeDataToWindow(SP<SFairvNodeData> node, SP<ITarget> target);
        void                            calculateWorkspace();
        SP<ITarget>                     getNextTarget(SP<ITarget> current, bool next);
        int                             getNodesNo();
        int                             getNodesOnWorkspace(WORKSPACEID wsId);
    };

}
