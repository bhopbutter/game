#pragma once

#include "SettingsPage.h"

class CRenderPanel;
namespace vgui
{
    class ColorPicker;
}

class AppearanceSettingsPage : public SettingsPage
{
    DECLARE_CLASS_SIMPLE(AppearanceSettingsPage, SettingsPage);

    AppearanceSettingsPage(Panel *pParent);
    ~AppearanceSettingsPage();

    void SetButtonColors();

    void LoadSettings() OVERRIDE;
    void OnPageShow() OVERRIDE;
    void OnPageHide() OVERRIDE;
    void OnMainDialogClosed() OVERRIDE;
    void OnMainDialogShow() OVERRIDE;
    void OnTextChanged(Panel *p) OVERRIDE;

    // From the color picker
    MESSAGE_FUNC_PARAMS(OnColorSelected, "ColorSelected", pKv);
    void OnCommand(const char* command) OVERRIDE;
    void ApplySchemeSettings(vgui::IScheme* pScheme) OVERRIDE;
    void OnReloadControls() override;

private:
    void UpdateModelSettings();
    void DestroyModelPanel();
    void CreateModelPanel();
    void LoadModelData();

    vgui::DHANDLE<vgui::Frame> m_pModelPreviewFrame;
    vgui::DHANDLE<CRenderPanel> m_pModelPreview;

    ConVarRef ghost_color, ghost_bodygroup, ghost_trail_color; // MOM_TODO add the rest of visible things here

    vgui::CvarComboBox *m_pBodygroupCombo;

    vgui::CvarToggleCheckButton *m_pEnableTrail;

    vgui::CvarTextEntry *m_pTrailLengthEntry;
    vgui::ColorPicker *m_pColorPicker;
    vgui::Button *m_pPickTrailColorButton, *m_pPickBodyColorButton;

    bool m_bModelPreviewFrameIsFadingOut;
};
