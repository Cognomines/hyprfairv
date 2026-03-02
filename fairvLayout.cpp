#include "fairvLayout.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <format>
#include <cmath>

using namespace Layout::Tiled;

static void applyWorkspaceLayoutOptions(SFairvWorkspaceData* wsData) {
    const auto         wsrule       = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(wsData->workspaceId));
    const auto         wslayoutopts = wsrule.layoutopts;

    static auto* const NGWO   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:fairv:layout:no_gaps_when_only")->getDataStaticPtr();
    auto               wsngwo = **NGWO;
    if (wslayoutopts.contains("fairv-no_gaps_when_only"))
        wsngwo = configStringToInt(wslayoutopts.at("fairv-no_gaps_when_only")).value_or(0);
    wsData->no_gaps_when_only = wsngwo;

    static auto* const SSFACT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:fairv:layout:special_scale_factor")->getDataStaticPtr();
    auto               wsssfact = **SSFACT;
    if (wslayoutopts.contains("fairv-special_scale_factor")) {
        std::string ssfactstr = wslayoutopts.at("fairv-special_scale_factor");
        try {
            wsssfact = std::stof(ssfactstr);
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Fairv layoutopt fairv-special_scale_factor format error: {}", e.what()); }
    }
    wsData->special_scale_factor = wsssfact;

    static auto* const INHERITFS   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:fairv:layout:inherit_fullscreen")->getDataStaticPtr();
    auto               wsinheritfs = **INHERITFS;
    if (wslayoutopts.contains("fairv-inherit_fullscreen"))
        wsinheritfs = configStringToInt(wslayoutopts.at("fairv-inherit_fullscreen")).value_or(0);
    wsData->inherit_fullscreen = wsinheritfs;
}

SP<SFairvNodeData> CFairvAlgorithm::getNodeFromTarget(SP<ITarget> target) {
    for (auto& nd : m_nodesData) {
        if (nd->target == target)
            return nd;
    }
    return nullptr;
}

SP<SFairvNodeData> CFairvAlgorithm::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& nd : m_nodesData) {
        if (nd->target->window() == pWindow)
            return nd;
    }
    return nullptr;
}

int CFairvAlgorithm::getNodesOnWorkspace(WORKSPACEID wsId) {
    int no = 0;
    for (auto& n : m_nodesData) {
        if (n->workspaceID == wsId)
            no++;
    }
    return no;
}

void CFairvAlgorithm::newTarget(SP<ITarget> target) {
    if (target->floating())
        return;

    const auto pWindow = target->window();
    const auto wsId    = pWindow->workspaceID();

    if (m_workspaceData.workspaceId == WORKSPACE_INVALID) {
        m_workspaceData.workspaceId = wsId;
        applyWorkspaceLayoutOptions(&m_workspaceData);
    }

    auto node         = makeShared<SFairvNodeData>();
    node->target      = target;
    node->workspaceID = wsId;
    node->gridIndex   = m_nodesData.size();

    m_nodesData.push_back(node);

    if (auto parent = m_parent.lock()) {
        parent->recalculate();
    }
}

void CFairvAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    newTarget(target);
}

void CFairvAlgorithm::removeTarget(SP<ITarget> target) {
    const auto node = getNodeFromTarget(target);
    if (!node)
        return;

    const auto removedIndex = node->gridIndex;
    const auto wsId         = node->workspaceID;

    m_nodesData.erase(std::remove(m_nodesData.begin(), m_nodesData.end(), node), m_nodesData.end());

    for (auto& n : m_nodesData) {
        if (n->workspaceID == wsId && n->gridIndex > removedIndex) {
            n->gridIndex--;
        }
    }

    if (auto parent = m_parent.lock()) {
        parent->recalculate();
    }
}

void CFairvAlgorithm::applyNodeDataToWindow(SP<SFairvNodeData> node, SP<ITarget> target) {
    const auto wsId     = node->workspaceID;
    PHLMONITOR PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(wsId)) {
        for (auto& m : g_pCompositor->m_monitors) {
            if (m->activeSpecialWorkspaceID() == wsId) {
                PMONITOR = m;
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getWorkspaceByID(wsId)->m_monitor.lock();
    }

    if (!PMONITOR) {
        Log::logger->log(Log::ERR, "Orphaned Node (workspace ID: {})!!", wsId);
        return;
    }

    const bool DISPLAYLEFT  = STICKS(node->target->position().x, PMONITOR->m_position.x + PMONITOR->m_reservedArea.left());
    const bool DISPLAYRIGHT = STICKS(node->target->position().x + node->target->position().width, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedArea.right());
    const bool DISPLAYTOP   = STICKS(node->target->position().y, PMONITOR->m_position.y + PMONITOR->m_reservedArea.top());
    const bool DISPLAYBOTTOM =
        STICKS(node->target->position().y + node->target->position().height, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedArea.bottom());

    const auto pWindow = target->window();
    if (!validMapped(pWindow))
        return;

    if (pWindow->isFullscreen())
        return;

    pWindow->m_size     = node->target->position().size();
    pWindow->m_position = node->target->position().pos();

    static auto* const PANIMATE     = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes");
    static auto* const PGAPSINDATA  = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_in");
    static auto* const PGAPSOUTDATA = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_out");
    auto* const        PGAPSIN      = (CCssGapData*)(*PGAPSINDATA)->getData();
    auto* const        PGAPSOUT     = (CCssGapData*)(*PGAPSOUTDATA)->getData();

    const auto         WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(wsId));
    auto               gapsIn        = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto               gapsOut       = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);

    if (m_workspaceData.no_gaps_when_only && !g_pCompositor->isWorkspaceSpecial(wsId) && (getNodesOnWorkspace(wsId) == 1 || pWindow->isEffectiveInternalFSMode(FSMODE_MAXIMIZED))) {
        pWindow->updateWindowDecos();
        const auto RESERVED      = pWindow->getFullWindowReservedArea();
        *pWindow->m_realPosition = pWindow->m_position + RESERVED.topLeft;
        *pWindow->m_realSize     = pWindow->m_size - (RESERVED.topLeft + RESERVED.bottomRight);
        return;
    }

    auto       calcPos  = pWindow->m_position;
    auto       calcSize = pWindow->m_size;

    const auto OFFSETTOPLEFT     = Vector2D((double)(DISPLAYLEFT ? gapsOut.m_left : gapsIn.m_left), (double)(DISPLAYTOP ? gapsOut.m_top : gapsIn.m_top));
    const auto OFFSETBOTTOMRIGHT = Vector2D((double)(DISPLAYRIGHT ? gapsOut.m_right : gapsIn.m_right), (double)(DISPLAYBOTTOM ? gapsOut.m_bottom : gapsIn.m_bottom));

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = pWindow->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(wsId)) {
        CBox wb = {calcPos + (calcSize - calcSize * m_workspaceData.special_scale_factor) / 2.f, calcSize * m_workspaceData.special_scale_factor};
        wb.round();
        *pWindow->m_realPosition = wb.pos();
        *pWindow->m_realSize     = wb.size();
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round();
        *pWindow->m_realSize     = wb.size();
        *pWindow->m_realPosition = wb.pos();
    }

    pWindow->updateWindowDecos();
}

void CFairvAlgorithm::calculateWorkspace() {
    const auto wsId = m_workspaceData.workspaceId;
    if (wsId == WORKSPACE_INVALID)
        return;

    const auto pWorkspace = g_pCompositor->getWorkspaceByID(wsId);
    if (!pWorkspace)
        return;

    const auto pMonitor = pWorkspace->m_monitor.lock();
    if (!pMonitor)
        return;

    if (pWorkspace->m_hasFullscreenWindow) {
        const auto pFullWindow = pWorkspace->getFullscreenWindow();
        if (pWorkspace->m_fullscreenMode == FSMODE_FULLSCREEN) {
            *pFullWindow->m_realPosition = pMonitor->m_position;
            *pFullWindow->m_realSize     = pMonitor->m_size;
        } else if (pWorkspace->m_fullscreenMode == FSMODE_MAXIMIZED) {
            Vector2D pos  = pMonitor->m_position + Vector2D(pMonitor->m_reservedArea.left(), pMonitor->m_reservedArea.top());
            Vector2D size = pMonitor->m_size - Vector2D(pMonitor->m_reservedArea.left(), pMonitor->m_reservedArea.top()) -
                Vector2D(pMonitor->m_reservedArea.right(), pMonitor->m_reservedArea.bottom());
            pFullWindow->m_position = pos;
            pFullWindow->m_size     = size;
            CBox wb                 = {pos, size};
            wb.round();
            *pFullWindow->m_realPosition = wb.pos();
            *pFullWindow->m_realSize     = wb.size();
        }
        return;
    }

    const int n = getNodesOnWorkspace(wsId);
    if (n == 0)
        return;

    const float waX      = pMonitor->m_position.x + pMonitor->m_reservedArea.left();
    const float waY      = pMonitor->m_position.y + pMonitor->m_reservedArea.top();
    const float waWidth  = pMonitor->m_size.x - pMonitor->m_reservedArea.left() - pMonitor->m_reservedArea.right();
    const float waHeight = pMonitor->m_size.y - pMonitor->m_reservedArea.top() - pMonitor->m_reservedArea.bottom();

    int         rows, cols;
    if (n == 2) {
        rows = 1;
        cols = 2;
    } else {
        rows = (int)std::ceil(std::sqrt(n));
        cols = (int)std::ceil((double)n / rows);
    }

    for (int k = 0; k < n; k++) {
        SP<SFairvNodeData> node = nullptr;
        for (auto& nd : m_nodesData) {
            if (nd->workspaceID == wsId && nd->gridIndex == k) {
                node = nd;
                break;
            }
        }
        if (!node)
            continue;

        float gx, gy, gwidth, gheight;

        int   row = k % rows;
        int   col = k / rows;

        int   lrows, lcols;
        if (k >= rows * cols - rows) {
            lrows = n - (rows * cols - rows);
            lcols = cols;
        } else {
            lrows = rows;
            lcols = cols;
        }

        if (row == lrows - 1) {
            gheight = waHeight - (int)(std::ceil(waHeight / lrows) * row);
            gy      = waY + waHeight - gheight;
        } else {
            gheight = std::ceil(waHeight / lrows);
            gy      = waY + gheight * row;
        }

        if (col == lcols - 1) {
            gwidth = waWidth - (int)(std::ceil(waWidth / lcols) * col);
            gx     = waX + waWidth - gwidth;
        } else {
            gwidth = std::ceil(waWidth / lcols);
            gx     = waX + gwidth * col;
        }

        CBox box = {gx, gy, gwidth, gheight};
        node->target->setPositionGlobal(box);

        applyNodeDataToWindow(node, node->target);
    }
}

void CFairvAlgorithm::recalculate() {
    if (m_nodesData.empty())
        return;

    applyWorkspaceLayoutOptions(&m_workspaceData);
    calculateWorkspace();
}

void CFairvAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    recalculate();
}

SP<Layout::ITarget> CFairvAlgorithm::getNextCandidate(SP<Layout::ITarget> old) {
    return getNextTarget(old, true);
}

int CFairvAlgorithm::getNodesNo() {
    return m_nodesData.size();
}

SP<Layout::ITarget> CFairvAlgorithm::getNextTarget(SP<Layout::ITarget> current, bool next) {
    if (m_nodesData.empty())
        return nullptr;

    const auto currentNode = getNodeFromTarget(current);
    if (!currentNode)
        return m_nodesData.front()->target;

    std::vector<SP<SFairvNodeData>> sortedNodes = m_nodesData;
    std::sort(sortedNodes.begin(), sortedNodes.end(), [](SP<SFairvNodeData> a, SP<SFairvNodeData> b) { return a->gridIndex < b->gridIndex; });

    auto it = std::find(sortedNodes.begin(), sortedNodes.end(), currentNode);
    if (it == sortedNodes.end())
        return sortedNodes.front()->target;

    if (next) {
        it++;
        if (it == sortedNodes.end())
            it = sortedNodes.begin();
    } else {
        if (it == sortedNodes.begin())
            it = sortedNodes.end();
        it--;
    }

    return (*it)->target;
}

void CFairvAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    const auto nodeA = getNodeFromTarget(a);
    const auto nodeB = getNodeFromTarget(b);

    if (!nodeA || !nodeB)
        return;

    std::swap(nodeA->gridIndex, nodeB->gridIndex);

    recalculate();

    g_pHyprRenderer->damageWindow(a->window());
    g_pHyprRenderer->damageWindow(b->window());
}

void CFairvAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    const auto node = getNodeFromTarget(t);
    if (!node)
        return;

    const int n = getNodesOnWorkspace(node->workspaceID);
    if (n == 0)
        return;

    int rows, cols;
    if (n == 2) {
        rows = 1;
        cols = 2;
    } else {
        rows = (int)std::ceil(std::sqrt(n));
        cols = (int)std::ceil((double)n / rows);
    }

    int currentRow = node->gridIndex % rows;
    int currentCol = node->gridIndex / rows;

    int targetRow = currentRow;
    int targetCol = currentCol;

    switch (dir) {
        case Math::eDirection::DIRECTION_UP: targetRow = currentRow - 1; break;
        case Math::eDirection::DIRECTION_DOWN: targetRow = currentRow + 1; break;
        case Math::eDirection::DIRECTION_LEFT: targetCol = currentCol - 1; break;
        case Math::eDirection::DIRECTION_RIGHT: targetCol = currentCol + 1; break;
        default: return;
    }

    if (targetCol < 0 || targetCol >= cols)
        return;

    if (targetRow < 0 || targetRow >= rows)
        return;

    int targetIndex = targetCol * rows + targetRow;
    if (targetIndex >= n)
        return;

    SP<SFairvNodeData> targetNode = nullptr;
    for (auto& nd : m_nodesData) {
        if (nd->workspaceID == node->workspaceID && nd->gridIndex == targetIndex) {
            targetNode = nd;
            break;
        }
    }

    if (targetNode) {
        swapTargets(node->target, targetNode->target);
    }
}

std::expected<void, std::string> CFairvAlgorithm::layoutMsg(const std::string_view& sv) {
    return std::expected<void, std::string>{};
}

std::optional<Vector2D> CFairvAlgorithm::predictSizeForNewTarget() {
    if (m_nodesData.empty())
        return std::nullopt;

    const auto wsId       = m_workspaceData.workspaceId;
    const auto pWorkspace = g_pCompositor->getWorkspaceByID(wsId);
    if (!pWorkspace)
        return std::nullopt;

    const auto pMonitor = pWorkspace->m_monitor.lock();

    const int  n = getNodesOnWorkspace(wsId) + 1;
    int        rows, cols;
    if (n == 2) {
        rows = 1;
        cols = 2;
    } else {
        rows = (int)std::ceil(std::sqrt(n));
        cols = (int)std::ceil((double)n / rows);
    }

    float waWidth  = pMonitor->m_size.x - pMonitor->m_reservedArea.left() - pMonitor->m_reservedArea.right();
    float waHeight = pMonitor->m_size.y - pMonitor->m_reservedArea.top() - pMonitor->m_reservedArea.bottom();

    return Vector2D(std::ceil(waWidth / cols), std::ceil(waHeight / rows));
}
