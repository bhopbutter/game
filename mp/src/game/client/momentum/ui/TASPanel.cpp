// clang-format off
#include "cbase.h"

#include <vgui_controls/Button.h>
#include <vgui_controls/CVarSlider.h>
#include <vgui_controls/CvarToggleCheckButton.h>
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "ColorPicker.h"
#include "c_mom_player.h"
#include "mom_gamemovement.h"
#include "mom_shareddefs.h"
#include "prediction.h"
#include "clientmode_shared.h"
#include "debugoverlay_shared.h"
#include "materialsystem/imaterialvar.h"
#include "util/mom_util.h"
#include "util_shared.h"
#include "weapon/weapon_csbase.h"
#include "TASPanel.h"
#include "tier2/renderutils.h"

#include "tier0/memdbgon.h"

extern bool ScreenTransform(const Vector&, Vector&);

// clang-format on

using namespace vgui;

CTASPanel *vgui::g_pTASPanel = nullptr;

static MAKE_CONVAR(mom_tas_max_vispredmove_ticks, "1000", FCVAR_NONE, "Ticks to visualize predicting movements.", 0,
                   INT_MAX);

static MAKE_CONVAR(mom_tas_vispredmove, "1", FCVAR_NONE,
                   "0: Disabled, 1: Visualize predicted movements: stop when landing,"
                   "2: Visualize predicted movements by chosen ticks (mom_tas_max_vispredmove_ticks)\n",
                   0, 2);

CON_COMMAND(mom_tas_panel, "Toggle TAS panel")
{
    if (engine->IsInGame())
    {
        if (g_pTASPanel == nullptr)
            g_pTASPanel = new CTASPanel();

        g_pTASPanel->ToggleVisible();
    }
}

CTASPanel::CTASPanel() : BaseClass(g_pClientMode->GetViewport(), "TASPanel")
{
    SetSize(2, 2);
    SetProportional(false);
    SetScheme("ClientScheme");
    SetMouseInputEnabled(true);
    SetSizeable(false);
    SetClipToParent(true); // Needed so we won't go out of bounds

    surface()->CreatePopup(GetVPanel(), false, false, false, true, false);

    LoadControlSettings("resource/ui/TASPanel.res");

    m_pEnableTASMode = FindControl<ToggleButton>("EnableTASMode");

    SetVisible(false);
    SetTitle(L"TAS Panel", true);

    m_pVisualPanel = new CTASVisPanel();
}

CTASPanel::~CTASPanel()
{
    if (m_pVisualPanel != nullptr)
    {
        delete m_pVisualPanel;
        m_pVisualPanel = nullptr;
    }
}

void CTASPanel::OnThink()
{
    int x, y;
    input()->GetCursorPosition(x, y);
    SetKeyBoardInputEnabled(IsWithin(x, y));

    C_MomentumPlayer *pPlayer = ToCMOMPlayer(C_BasePlayer::GetLocalPlayer());
    if (pPlayer)
    {
        m_pEnableTASMode->SetText((pPlayer->m_SrvData.m_RunData.m_iRunFlags & RUNFLAG_TAS) ? "#MOM_EnabledTASMode"
                                                                                           : "#MOM_DisabledTASMode");
        m_pEnableTASMode->SetSelected(pPlayer->m_SrvData.m_RunData.m_iRunFlags & RUNFLAG_TAS);
    }

    BaseClass::OnThink();
}

void CTASPanel::OnCommand(const char *pcCommand)
{
    BaseClass::OnCommand(pcCommand);

    if (!Q_strcasecmp(pcCommand, "enabletasmode"))
    {
        engine->ServerCmd("mom_tas");
    }
}

void CTASPanel::ToggleVisible()
{
    // Center the mouse in the panel
    int x, y, w, h;
    GetBounds(x, y, w, h);
    input()->SetCursorPos(x + (w / 2), y + (h / 2));
    SetVisible(true);
}

void CTASVisPanel::RunVPM(C_MomentumPlayer *pPlayer)
{
    if (pPlayer == nullptr)
        return;

    float flMaxTime = mom_tas_max_vispredmove_ticks.GetInt() * gpGlobals->interval_per_tick;

    if (!m_vecOrigins.IsEmpty())
        m_vecOrigins.RemoveAll();

    m_flVPMTime = 0.0f;

    m_flOldCurtime = gpGlobals->curtime;
    m_flOldFrametime = gpGlobals->frametime;

    gpGlobals->curtime = pPlayer->GetTimeBase();
    gpGlobals->frametime = gpGlobals->interval_per_tick;

    prediction->StartCommand(pPlayer, &pPlayer->m_LastCreateMoveCmd);

    static CMoveData tmpData, tmpBackupData;
    pPlayer->AvoidPhysicsProps(&pPlayer->m_LastCreateMoveCmd);
    prediction->SetupMove(pPlayer, &pPlayer->m_LastCreateMoveCmd, MoveHelper(), &tmpBackupData);

    engine->Con_NPrintf(0, "%2f %2f %2f - %2f %2f %2f %f", tmpData.m_flForwardMove, tmpData.m_flSideMove,
                        tmpData.m_flUpMove, tmpData.m_vecAbsViewAngles.x, tmpData.m_vecAbsViewAngles.y,
                        tmpData.m_vecAbsViewAngles.z, pPlayer->GetGravity());

    pPlayer->m_bSimulatingMovements = true;

    bool bFirstTimePredicted = prediction->m_bFirstTimePredicted;

    prediction->m_bFirstTimePredicted = false;

    pPlayer->AvoidPhysicsProps(&pPlayer->m_LastCreateMoveCmd);
    tmpData = tmpBackupData;

    ConVarRef("sv_footsteps").SetInt(0);

    while (m_flVPMTime <= flMaxTime)
    {
        // gpGlobals->curtime = m_flOldCurtime + m_flVPMTime;

        // Process last movement data we know.
        // This might be changed because it can more accurate during jumps.
        // Imagine having a circle around you wich says you where is the best speed gain and where you will land at
        // the same time.. That would be my feature project I guess.
        // prediction->SetupMove(pPlayer, &pPlayer->m_LastCreateMoveCmd, MoveHelper(), &tmpData);
        g_pMomentumGameMovement->ProcessMovement(pPlayer, &tmpData);
        // prediction->FinishMove(pPlayer, &pPlayer->m_LastCreateMoveCmd, &tmpData);

        // gpGlobals->curtime = m_flOldCurtime;

        m_flVPMTime += gpGlobals->frametime;

        if (pPlayer->m_hGroundEntity.Get() != nullptr && mom_tas_vispredmove.GetInt() == 1)
        {
            break;
        }

        m_vecOrigins.AddToHead(tmpData.GetAbsOrigin());
    }

    ConVarRef("sv_footsteps").SetInt(1);

    prediction->FinishMove(pPlayer, &pPlayer->m_LastCreateMoveCmd, &tmpBackupData);

    prediction->m_bFirstTimePredicted = bFirstTimePredicted;

    engine->Con_NPrintf(2, "%i", m_vecOrigins.Count());

    pPlayer->m_bSimulatingMovements = false;

    prediction->FinishCommand(pPlayer);

    gpGlobals->curtime = m_flOldCurtime;
    gpGlobals->frametime = m_flOldFrametime;
}

void CTASVisPanel::Paint()
{
    int x, y;
    engine->GetScreenSize(x, y);
    SetSize(x, y);

    surface()->DrawSetColor(255, 255, 255, 255);

    if (m_vecOrigins.Count() > 1)
    {
        for (int i = 1; i < m_vecOrigins.Count(); i++)
        {
            // RenderLine(m_vecOrigins[i - 1], m_vecOrigins[i], Color(255, 255, 255, 255), true);
            Vector vecFirst, vecSecond;

            if (!debugoverlay->ScreenPosition(m_vecOrigins[i - 1], vecFirst) &&
                !debugoverlay->ScreenPosition(m_vecOrigins[i], vecSecond))
            {
                surface()->DrawLine(vecFirst.x, vecFirst.y, vecSecond.x, vecSecond.y);
            }
        }
    }

    BaseClass::Paint();
}

void CTASVisPanel::VisPredMovements() { RunVPM(ToCMOMPlayer(C_BasePlayer::GetLocalPlayer())); }

CTASVisPanel::CTASVisPanel() : BaseClass(g_pClientMode->GetViewport(), "tasvisgui")
{
    SetVisible(true);
    SetMouseInputEnabled(false);
    SetKeyBoardInputEnabled(false);
    surface()->CreatePopup(GetVPanel(), false, false, false, false, false);
}

CTASVisPanel::~CTASVisPanel() {}