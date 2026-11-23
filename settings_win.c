/*
** =========================================================================
** settings_win.c - SimpleSlicer Configuration & Preferences Window for AmigaOS
** =========================================================================
*/

#include "settings_win.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

AppConfig g_config;

void InitDefaultConfig(AppConfig *cfg)
{
    if (!cfg) return;
    cfg->bed_x = 220.0f;
    cfg->bed_y = 220.0f;
    cfg->bed_z = 250.0f;
    cfg->auto_rotate_lwo = 1; /* Default: +90 deg X rotation */
    cfg->smart_downscale = TRUE;
}

BOOL LoadConfig(const char *filename, AppConfig *cfg)
{
    InitDefaultConfig(cfg);
    if (!filename) filename = "simpleslicer.cfg";

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        /* Try loading from PROGDIR if default failed */
        fp = fopen("PROGDIR:simpleslicer.cfg", "r");
    }

    if (!fp) return FALSE;

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        char key[64] = {0};
        char val[128] = {0};
        if (sscanf(line, "%63[^=]=%127s", key, val) == 2)
        {
            if (strcasecmp(key, "bed_x") == 0) cfg->bed_x = atof(val);
            else if (strcasecmp(key, "bed_y") == 0) cfg->bed_y = atof(val);
            else if (strcasecmp(key, "bed_z") == 0) cfg->bed_z = atof(val);
            else if (strcasecmp(key, "auto_rotate_lwo") == 0) cfg->auto_rotate_lwo = atoi(val);
            else if (strcasecmp(key, "smart_downscale") == 0) cfg->smart_downscale = (atoi(val) != 0);
        }
    }

    fclose(fp);
    return TRUE;
}

BOOL SaveConfig(const char *filename, const AppConfig *cfg)
{
    if (!cfg) return FALSE;
    if (!filename) filename = "PROGDIR:simpleslicer.cfg";

    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        /* Fallback to local directory if PROGDIR write failed */
        fp = fopen("simpleslicer.cfg", "w");
    }

    if (!fp) return FALSE;

    fprintf(fp, "# SimpleSlicer Printer & Model Preferences\n");
    fprintf(fp, "bed_x=%.1f\n", cfg->bed_x);
    fprintf(fp, "bed_y=%.1f\n", cfg->bed_y);
    fprintf(fp, "bed_z=%.1f\n", cfg->bed_z);
    fprintf(fp, "auto_rotate_lwo=%d\n", cfg->auto_rotate_lwo);
    fprintf(fp, "smart_downscale=%d\n", cfg->smart_downscale ? 1 : 0);

    fclose(fp);
    return TRUE;
}

BOOL OpenSettingsDialog(Object *app, Object *parent_win, AppConfig *cfg)
{
    if (!cfg) return FALSE;

    char str_x[32], str_y[32], str_z[32];
    snprintf(str_x, sizeof(str_x), "%.1f", cfg->bed_x);
    snprintf(str_y, sizeof(str_y), "%.1f", cfg->bed_y);
    snprintf(str_z, sizeof(str_z), "%.1f", cfg->bed_z);

    Object *obj_x, *obj_y, *obj_z, *cyc_rot, *cyc_downscale;
    Object *btn_save, *btn_cancel;
    Object *settings_win;

    static const char *RotOptions[] = { "+90 deg X (Rotate Up)", "-90 deg X (Rotate Down)", "Disabled (Keep Original)", NULL };
    static const char *DownscaleOptions[] = { "Smart Downscale (Only if > Bed)", "Always Fit Bed", "Disabled", NULL };

    settings_win = WindowObject,
        MUIA_Window_Title, "SimpleSlicer Printer & Model Preferences",
        MUIA_Window_RefWindow, parent_win,
        MUIA_Window_ID, MAKE_ID('S', 'E', 'T', 'T'),

        WindowContents, VGroup,
            Child, ColGroup(2),
                Child, Label2("Printer Bed Width X (mm):"),
                Child, obj_x = StringObject,
                    MUIA_String_Contents, str_x,
                End,

                Child, Label2("Printer Bed Depth Y (mm):"),
                Child, obj_y = StringObject,
                    MUIA_String_Contents, str_y,
                End,

                Child, Label2("Printer Bed Height Z (mm):"),
                Child, obj_z = StringObject,
                    MUIA_String_Contents, str_z,
                End,

                Child, Label2("LightWave Auto-Rotate (Y-Up -> Z-Up):"),
                Child, cyc_rot = CycleObject,
                    MUIA_Cycle_Entries, RotOptions,
                    MUIA_Cycle_Active, (cfg->auto_rotate_lwo == 1) ? 0 : ((cfg->auto_rotate_lwo == 2) ? 1 : 2),
                End,

                Child, Label2("Smart Auto-Fit Behavior:"),
                Child, cyc_downscale = CycleObject,
                    MUIA_Cycle_Entries, DownscaleOptions,
                    MUIA_Cycle_Active, cfg->smart_downscale ? 0 : 2,
                End,
            End,

            Child, HGroup,
                Child, btn_save = SimpleButton("Save Preferences"),
                Child, btn_cancel = SimpleButton("Cancel"),
            End,
        End,
    End;

    if (!settings_win) return FALSE;

    /* Add window to MUI application */
    DoMethod(app, OM_ADDMEMBER, settings_win);

    DoMethod(settings_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             app, 2, MUIM_Application_ReturnID, 201);

    DoMethod(btn_save, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, 200);

    DoMethod(btn_cancel, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, 201);

    SetAttrs(settings_win, MUIA_Window_Open, TRUE, TAG_END);

    ULONG sigs = 0;
    BOOL modal_running = TRUE;
    BOOL saved = FALSE;

    while (modal_running)
    {
        ULONG res = DoMethod(app, MUIM_Application_NewInput, &sigs);
        switch (res)
        {
            case 200: /* Save */
            {
                STRPTR val_x = NULL, val_y = NULL, val_z = NULL;
                ULONG rot_idx = 0, down_idx = 0;

                GetAttr(MUIA_String_Contents, obj_x, (ULONG *)(APTR)&val_x);
                GetAttr(MUIA_String_Contents, obj_y, (ULONG *)(APTR)&val_y);
                GetAttr(MUIA_String_Contents, obj_z, (ULONG *)(APTR)&val_z);
                GetAttr(MUIA_Cycle_Active, cyc_rot, &rot_idx);
                GetAttr(MUIA_Cycle_Active, cyc_downscale, &down_idx);

                if (val_x && *val_x) cfg->bed_x = atof(val_x);
                if (val_y && *val_y) cfg->bed_y = atof(val_y);
                if (val_z && *val_z) cfg->bed_z = atof(val_z);
                cfg->auto_rotate_lwo = (rot_idx == 0) ? 1 : ((rot_idx == 1) ? 2 : 0);
                cfg->smart_downscale = (down_idx == 0);

                SaveConfig("PROGDIR:simpleslicer.cfg", cfg);
                saved = TRUE;
                modal_running = FALSE;
                break;
            }

            case 201: /* Cancel / Close */
            case MUIV_Application_ReturnID_Quit:
                modal_running = FALSE;
                break;
        }

        if (modal_running && sigs)
        {
            sigs = Wait(sigs);
        }
    }

    SetAttrs(settings_win, MUIA_Window_Open, FALSE, TAG_END);
    DoMethod(app, OM_REMMEMBER, settings_win);
    MUI_DisposeObject(settings_win);

    return saved;
}
