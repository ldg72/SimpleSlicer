/*
** =========================================================================
** CuraEngine 15.6.4 GUI Frontend for AmigaOS 4
** Built with MUI (Magic User Interface)
** =========================================================================
*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#include <libraries/asl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#include "lwo_parser.h"
#include "settings_win.h"
#include "mgl_viewport.h"
#include "model_transform.h"

/*
** Macro for MAKE_ID if not provided by iffparse.h
*/
#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG)(a)<<24 | (ULONG)(b)<<16 | (ULONG)(c)<<8 | (ULONG)(d))
#endif

/*
** AmigaOS 4 Library & Interface Pointers
*/
struct Library        *MUIMasterBase = NULL;
struct MUIMasterIFace *IMUIMaster    = NULL;

struct Library        *IntuitionBase = NULL;
struct IntuitionIFace *IIntuition    = NULL;
struct Library        *UtilityBase   = NULL;
struct UtilityIFace   *IUtility      = NULL;

/*
** Custom Return IDs for MUI Notification Events
*/
enum
{
    ID_QUIT = 1,
    ID_SLICE,
    ID_OPEN_STL,
    ID_SAVE_GCODE,
    ID_SETTINGS,
    ID_IMPORT_SCALE,
    ID_TRANSFORM_APPLY,
    ID_TRANSFORM_CENTER,
    ID_TRANSFORM_DROP,
    ID_TRANSFORM_RESET
};

/*
** Cycle Gadget Selection Option Arrays
*/
static const char *Profiles[]  = { "Standard (0.2mm)", "Draft (0.3mm)", "Fine (0.1mm)", NULL };
static const char *Supports[]  = { "None", "Touching Buildplate", "Everywhere", NULL };
static const char *Adhesions[] = { "None", "Brim", "Raft", "Skirt", NULL };
static const char *ScalePresets[] = { "1x (Native mm)", "Auto-Fit (Fit 20cm Bed)", "1000x (LW Meters)", "100x (LW Deci)", "10x (LW cm)", NULL };
static const char *TabTitles[] = { "Basic Settings", "Advanced Settings", NULL };

/*
** Original geometry, transformed preview and model editing state
*/
static Mesh3D g_active_mesh;
static Mesh3D g_original_mesh;
static ModelTransform g_transform;
static BOOL g_mesh_loaded = FALSE;
static BOOL g_convert_units = FALSE;
static char g_loaded_path[512];

typedef struct {
    Object *position[3], *rotation[3], *scale[3];
    Object *lock, *dimensions, *status, *group;
} TransformControls;
static TransformControls g_controls;

/* Shared by preview loading and STL export. Negative means Auto-Fit. */
static float SelectedModelScale(Object *cycle)
{
    ULONG index = 0;
    GetAttr(MUIA_Cycle_Active, cycle, &index);
    switch (index)
    {
        case 1: return -1.0f;
        case 2: return 1000.0f;
        case 3: return 100.0f;
        case 4: return 10.0f;
        default: return 1.0f;
    }
}

static void RefreshTransformControls(void)
{
    float values[9] = {g_transform.position.x, g_transform.position.y, g_transform.position.z,
        g_transform.rotation.x, g_transform.rotation.y, g_transform.rotation.z,
        g_transform.scale.x*100, g_transform.scale.y*100, g_transform.scale.z*100};
    char text[192];
    for (int i=0; i<9; i++) {
        Object *field = i<3 ? g_controls.position[i] :
            (i<6 ? g_controls.rotation[i-3] : g_controls.scale[i-6]);
        snprintf(text, sizeof(text), "%.6g", values[i]);
        SetAttrs(field, MUIA_String_Contents, text, TAG_END);
    }
    SetAttrs(g_controls.group, MUIA_Disabled, !g_mesh_loaded, TAG_END);
    if (g_mesh_loaded) {
        snprintf(text, sizeof(text), "Size: %.2f x %.2f x %.2f mm",
            g_active_mesh.max_bounds.x-g_active_mesh.min_bounds.x,
            g_active_mesh.max_bounds.y-g_active_mesh.min_bounds.y,
            g_active_mesh.max_bounds.z-g_active_mesh.min_bounds.z);
    } else snprintf(text, sizeof(text), "Size: --");
    SetAttrs(g_controls.dimensions, MUIA_Text_Contents, text, TAG_END);
    const char *status = "Load a model to edit it.";
    if (g_mesh_loaded) {
        if (!MeshFitsBuildVolume(&g_active_mesh, g_config.bed_x, g_config.bed_y, g_config.bed_z))
            status = "WARNING: model outside build volume";
        else if (g_transform.position.z > 0.001f)
            status = "Model above bed - use Drop to Bed if needed";
        else status = "Model inside build volume";
    }
    SetAttrs(g_controls.status, MUIA_Text_Contents, status, TAG_END);
}

static BOOL ReadTransformNumber(Object *field, float *value)
{
    STRPTR raw = NULL;
    GetAttr(MUIA_String_Contents, field, (ULONG *)(APTR)&raw);
    if (!raw || strlen(raw) >= 64) return FALSE;
    char text[64], *end;
    strcpy(text, raw);
    for (char *p=text; *p; p++) if (*p == ',') *p = '.';
    errno = 0;
    double parsed = strtod(text, &end);
    if (end == text || errno || !isfinite(parsed) || fabs(parsed) > 1000000.0) return FALSE;
    while (isspace((unsigned char)*end)) end++;
    if (*end) return FALSE;
    *value = (float)parsed;
    return TRUE;
}

static BOOL ReadTransformControls(ModelTransform *next)
{
    float v[9];
    for (int i=0; i<9; i++) {
        Object *field = i<3 ? g_controls.position[i] :
            (i<6 ? g_controls.rotation[i-3] : g_controls.scale[i-6]);
        if (!ReadTransformNumber(field, &v[i])) return FALSE;
    }
    if (v[6] <= 0 || v[7] <= 0 || v[8] <= 0) return FALSE;
    next->position = (Vector3D){v[0],v[1],v[2]};
    next->rotation = (Vector3D){fmodf(v[3],360),fmodf(v[4],360),fmodf(v[5],360)};
    next->scale = (Vector3D){v[6]/100,v[7]/100,v[8]/100};
    ULONG locked = FALSE;
    GetAttr(MUIA_Selected, g_controls.lock, &locked);
    if (locked) {
        /* A change to any one scale field resizes all axes proportionally. */
        float old[3] = {g_transform.scale.x,g_transform.scale.y,g_transform.scale.z};
        float factor = 1.0f;
        BOOL changed = FALSE;
        for (int i=0; i<3; i++) {
            float ratio = (v[i+6]/100)/old[i];
            if (fabsf(ratio-1) > 0.00001f) {
                if (changed && fabsf(ratio-factor) > 0.00001f*fmaxf(1,fabsf(factor))) return FALSE;
                factor = ratio;
                changed = TRUE;
            }
        }
        next->scale = (Vector3D){old[0]*factor,old[1]*factor,old[2]*factor};
    }
    return TRUE;
}

static BOOL UpdatePreview(Object *viewport, Object *cycle, const ModelTransform *next)
{
    Mesh3D candidate = {0};
    float units = g_convert_units ? ResolveMeshScale(&g_original_mesh, SelectedModelScale(cycle)) : 1.0f;
    if (!BuildTransformedMesh(&g_original_mesh, &candidate, next, units)) return FALSE;
    /* The viewport points to this stable struct, never to freed geometry. */
    FreeMesh3D(&g_active_mesh);
    g_active_mesh = candidate;
    g_transform = *next;
    SetAttrs(viewport, MUIA_MGLViewport_Mesh, (ULONG)&g_active_mesh, TAG_END);
    MUI_Redraw(viewport, MADF_DRAWOBJECT);
    RefreshTransformControls();
    return TRUE;
}

static BOOL ApplyTransformControls(Object *app, Object *win, Object *viewport, Object *cycle, ULONG action)
{
    if (!g_mesh_loaded) return FALSE;
    ModelTransform next;
    if (action == ID_TRANSFORM_RESET) ResetModelTransform(&next);
    else if (!ReadTransformControls(&next)) {
        MUI_Request(app, win, 0, "Invalid transform", "*_OK",
            "Enter finite numbers and positive scale percentages.\nWith proportions locked, edit one scale axis at a time.", TAG_END);
        return FALSE;
    }
    if (action == ID_TRANSFORM_CENTER) next.position.x = next.position.y = 0;
    if (action == ID_TRANSFORM_DROP) next.position.z = 0;
    if (!UpdatePreview(viewport, cycle, &next)) {
        MUI_Request(app, win, 0, "Transform error", "*_OK",
            "Cannot transform this mesh: insufficient memory or coordinates out of range.", TAG_END);
        return FALSE;
    }
    return TRUE;
}

static void UpdatePreviewBed(Object *viewport)
{
    union { float f; ULONG u; } x, y;
    x.f = isfinite(g_config.bed_x) && g_config.bed_x > 0 && g_config.bed_x <= 1000000 ? g_config.bed_x : 220.0f;
    y.f = isfinite(g_config.bed_y) && g_config.bed_y > 0 && g_config.bed_y <= 1000000 ? g_config.bed_y : 220.0f;
    SetAttrs(viewport, MUIA_MGLViewport_BedX, x.u, MUIA_MGLViewport_BedY, y.u, TAG_END);
    MUI_Redraw(viewport, MADF_DRAWOBJECT);
}

static Object *TransformField(const char *initial)
{
    return StringObject,
        StringFrame,
        MUIA_String_Contents, initial,
        MUIA_String_MaxLen, 32,
        MUIA_String_Format, MUIV_String_Format_Right,
    End;
}

/*
** Main Application Entry Point
*/
int main(int argc, char *argv[])
{
    Object *app, *win;
    Object *str_stl, *str_gcode, *cyc_profile, *cyc_scale, *str_layer_height, *str_wall_thick;
    Object *sld_infill, *str_speed, *str_temp, *str_bed_temp, *cyc_support, *cyc_adhesion;
    Object *str_travel_speed, *str_retract_speed, *str_retract_len, *str_filament_dia;
    Object *str_nozzle_dia, *sld_fan_speed, *str_extra_args;
    Object *btn_slice, *btn_settings, *area_3d_viewport;
    Object *btn_apply, *btn_center, *btn_drop, *btn_reset;

    ULONG signals = 0;
    BOOL running = TRUE;

    (void)argc;
    (void)argv;

    /*
    ** Open intuition.library, utility.library and muimaster.library for AmigaOS 4
    */
    IntuitionBase = OpenLibrary("intuition.library", 50);
    if (!IntuitionBase)
    {
        printf("Error: Could not open intuition.library v50+\n");
        return 20;
    }

    IIntuition = (struct IntuitionIFace *)GetInterface(IntuitionBase, "main", 1, NULL);
    if (!IIntuition)
    {
        printf("Error: Could not obtain IIntuition interface\n");
        CloseLibrary(IntuitionBase);
        return 20;
    }

    UtilityBase = OpenLibrary("utility.library", 50);
    if (!UtilityBase)
    {
        printf("Error: Could not open utility.library v50+\n");
        DropInterface((struct Interface *)IIntuition);
        CloseLibrary(IntuitionBase);
        return 20;
    }

    IUtility = (struct UtilityIFace *)GetInterface(UtilityBase, "main", 1, NULL);
    if (!IUtility)
    {
        printf("Error: Could not obtain IUtility interface\n");
        CloseLibrary(UtilityBase);
        DropInterface((struct Interface *)IIntuition);
        CloseLibrary(IntuitionBase);
        return 20;
    }

    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase)
    {
        printf("Error: Could not open muimaster.library v19+\n");
        DropInterface((struct Interface *)IUtility);
        CloseLibrary(UtilityBase);
        DropInterface((struct Interface *)IIntuition);
        CloseLibrary(IntuitionBase);
        return 20;
    }

    IMUIMaster = (struct MUIMasterIFace *)GetInterface(MUIMasterBase, "main", 1, NULL);
    if (!IMUIMaster)
    {
        printf("Error: Could not obtain IMUIMaster interface\n");
        CloseLibrary(MUIMasterBase);
        DropInterface((struct Interface *)IUtility);
        CloseLibrary(UtilityBase);
        DropInterface((struct Interface *)IIntuition);
        CloseLibrary(IntuitionBase);
        return 20;
    }

    /* Initialize Embedded MiniGL Viewport Custom Class */
    if (!InitMGLViewportClass())
    {
        DropInterface((struct Interface *)IMUIMaster);
        CloseLibrary(MUIMasterBase);
        DropInterface((struct Interface *)IUtility);
        CloseLibrary(UtilityBase);
        DropInterface((struct Interface *)IIntuition);
        CloseLibrary(IntuitionBase);
        return 20;
    }

    /*
    ** Build the MUI GUI Object Hierarchy
    */
    app = ApplicationObject,
        MUIA_Application_Title,       "CuraEngine Frontend",
        MUIA_Application_Version,     "$VER: CuraEngine-GUI 1.0 (2026.09.24)",
        MUIA_Application_Copyright,   "Open Source Amiga Community",
        MUIA_Application_Author,      "AmigaOS 4 Developer",
        MUIA_Application_Description, "MUI Frontend for 3D CuraEngine Slicer",
        MUIA_Application_Base,        "CURASLICER",

        SubWindow, win = WindowObject,
            MUIA_Window_Title,    "CuraEngine 15.6.4 Slicer - AmigaOS 4",
            MUIA_Window_ID,       MAKE_ID('C','U','R','2'),
            MUIA_Window_Width,    950,
            MUIA_Window_Height,   600,
            MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
            MUIA_Window_TopEdge,  MUIV_Window_TopEdge_Centered,

            WindowContents, HGroup,

                /* 
                ** =========================================================
                ** LEFT PANEL: Slicer Configuration Settings (Fixed Width)
                ** =========================================================
                */
                Child, VGroup,
                    MUIA_Weight, 0, /* Fixed natural width */

                    /* Title Header */
                    Child, TextObject,
                        MUIA_Text_Contents, "\33c\33bCuraEngine 3D Slicer Configuration",
                    End,

                    Child, VSpace(4),

                    /* Tabbed Register Group for Settings */
                    Child, RegisterGroup(TabTitles),
                        MUIA_Register_Frame, TRUE,

                        /* -------------------------------------------------
                        ** TAB 1: Basic Parameters
                        ** ------------------------------------------------- */
                        Child, VGroup,
                            Child, ColGroup(2),
                                Child, Label2("3D Model (STL/LWO/OBJ):"),
                                Child, str_stl = PopaslObject,
                                    MUIA_Popstring_String, StringObject,
                                        MUIA_String_Contents, "",
                                    End,
                                    MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                    ASLFR_TitleText, "Select 3D Model File (.stl, .lwo, .obj)...",
                                End,

                                Child, Label2("Output G-Code:"),
                                Child, str_gcode = PopaslObject,
                                    MUIA_Popstring_String, StringObject,
                                        MUIA_String_Contents, "",
                                    End,
                                    MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                    ASLFR_TitleText, "Select Output G-Code File...",
                                End,

                                Child, Label2("Import Units (LWO/OBJ):"),
                                Child, cyc_scale = CycleObject,
                                    MUIA_Cycle_Entries, ScalePresets,
                                End,

                                Child, Label2("Print Profile:"),
                                Child, cyc_profile = CycleObject,
                                    MUIA_Cycle_Entries, Profiles,
                                End,

                                Child, Label2("Layer Height (mm):"),
                                Child, str_layer_height = StringObject,
                                    MUIA_String_Contents, "0.20",
                                End,

                                Child, Label2("Wall Thickness (mm):"),
                                Child, str_wall_thick = StringObject,
                                    MUIA_String_Contents, "0.80",
                                End,

                                Child, Label2("Infill Density (%):"),
                                Child, sld_infill = SliderObject,
                                    MUIA_Numeric_Min,   0,
                                    MUIA_Numeric_Max,   100,
                                    MUIA_Numeric_Value, 20,
                                End,

                                Child, Label2("Print Speed (mm/s):"),
                                Child, str_speed = StringObject,
                                    MUIA_String_Contents, "50",
                                End,

                                Child, Label2("Hotend Temp (\260C):"),
                                Child, str_temp = StringObject,
                                    MUIA_String_Contents, "200",
                                End,

                                Child, Label2("Bed Temp (\260C):"),
                                Child, str_bed_temp = StringObject,
                                    MUIA_String_Contents, "60",
                                End,

                                Child, Label2("Support Type:"),
                                Child, cyc_support = CycleObject,
                                    MUIA_Cycle_Entries, Supports,
                                End,

                                Child, Label2("Platform Adhesion:"),
                                Child, cyc_adhesion = CycleObject,
                                    MUIA_Cycle_Entries, Adhesions,
                                End,
                            End,
                        End,

                        /* -------------------------------------------------
                        ** TAB 2: Advanced Parameters
                        ** ------------------------------------------------- */
                        Child, VGroup,
                            Child, ColGroup(2),
                                Child, Label2("Travel Speed (mm/s):"),
                                Child, str_travel_speed = StringObject,
                                    MUIA_String_Contents, "120",
                                End,

                                Child, Label2("Retract Speed (mm/s):"),
                                Child, str_retract_speed = StringObject,
                                    MUIA_String_Contents, "40",
                                End,

                                Child, Label2("Retract Distance (mm):"),
                                Child, str_retract_len = StringObject,
                                    MUIA_String_Contents, "4.50",
                                End,

                                Child, Label2("Filament Dia. (mm):"),
                                Child, str_filament_dia = StringObject,
                                    MUIA_String_Contents, "1.75",
                                End,

                                Child, Label2("Nozzle Dia. (mm):"),
                                Child, str_nozzle_dia = StringObject,
                                    MUIA_String_Contents, "0.40",
                                End,

                                Child, Label2("Fan Speed (%):"),
                                Child, sld_fan_speed = SliderObject,
                                    MUIA_Numeric_Min,   0,
                                    MUIA_Numeric_Max,   100,
                                    MUIA_Numeric_Value, 100,
                                End,

                                Child, Label2("Extra Cura CLI Args:"),
                                Child, str_extra_args = StringObject,
                                    MUIA_String_Contents, "",
                                End,
                            End,
                        End,
                    End,

                    Child, VSpace(4),

                    /* Slice Action & Settings Buttons */
                    Child, HGroup,
                        Child, btn_slice = SimpleButton("Slice Model (Generate G-Code)"),
                        Child, btn_settings = SimpleButton("Printer Preferences..."),
                    End,

                End, /* End Left VGroup */

                /* 
                ** =========================================================
                ** RIGHT PANEL: 3D Viewport Placeholder (Fills All Extra Space)
                ** =========================================================
                */
                Child, VGroup,
                    MUIA_Weight, 100, /* Fills remaining window space */
                    MUIA_Frame,  MUIV_Frame_Group,
                    MUIA_FrameTitle, "3D Viewport (Preview)",

                    Child, area_3d_viewport = NewObject(GetMGLViewportClass()->mcc_Class, NULL,
                        MUIA_InputMode, MUIV_InputMode_None,
                        MUIA_FillArea, FALSE,
                        MUIA_Frame, MUIV_Frame_InputList,
                        TAG_END),
                    Child, g_controls.group = VGroup,
                        MUIA_Weight, 0,
                        MUIA_Frame, MUIV_Frame_Group,
                        MUIA_FrameTitle, "Model Transform",
                        MUIA_Disabled, TRUE,
                        Child, TextObject,
                            MUIA_Text_Contents, "XY: bed centre = 0   |   Z: bottom height",
                        End,
                        Child, ColGroup(4),
                            Child, Label2(""), Child, Label2("X"), Child, Label2("Y"), Child, Label2("Z"),
                            Child, Label2("Position (mm)"),
                            Child, g_controls.position[0] = TransformField("0"),
                            Child, g_controls.position[1] = TransformField("0"),
                            Child, g_controls.position[2] = TransformField("0"),
                            Child, Label2("Rotation (deg)"),
                            Child, g_controls.rotation[0] = TransformField("0"),
                            Child, g_controls.rotation[1] = TransformField("0"),
                            Child, g_controls.rotation[2] = TransformField("0"),
                            Child, Label2("Scale (%)"),
                            Child, g_controls.scale[0] = TransformField("100"),
                            Child, g_controls.scale[1] = TransformField("100"),
                            Child, g_controls.scale[2] = TransformField("100"),
                        End,
                        Child, HGroup,
                            Child, g_controls.lock = CheckMark(TRUE),
                            Child, Label2("Lock proportions"),
                            Child, btn_apply = SimpleButton("Apply"),
                        End,
                        Child, HGroup,
                            Child, btn_center = SimpleButton("Center XY"),
                            Child, btn_drop = SimpleButton("Drop to Bed"),
                            Child, btn_reset = SimpleButton("Reset"),
                        End,
                    End,
                    Child, g_controls.dimensions = TextObject,
                        MUIA_Text_Contents, "Size: --",
                        MUIA_Text_SetMin, FALSE,
                    End,
                    Child, g_controls.status = TextObject,
                        MUIA_Text_Contents, "Load a model to edit it.",
                        MUIA_Text_SetMin, FALSE,
                    End,
                End, /* End Right VGroup */

            End, /* End WindowContents HGroup */
        End, /* End WindowObject */
    End; /* End ApplicationObject */

    /*
    ** Verify GUI Creation Success
    */
    if (!app)
    {
        printf("Error: Failed to create MUI Application Object\n");
        DropInterface((struct Interface *)IMUIMaster);
        CloseLibrary(MUIMasterBase);
        DropInterface((struct Interface *)IUtility);
        CloseLibrary(UtilityBase);
        DropInterface((struct Interface *)IIntuition);
        CloseLibrary(IntuitionBase);
        return 20;
    }

    /*
    ** Setup Notification Events
    */

    /* Close Window -> Quit App */
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             app, 2, MUIM_Application_ReturnID, ID_QUIT);

    /* Click Slice Button -> Trigger Slice Action */
    DoMethod(btn_slice, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_SLICE);

    /* Click Printer Preferences Button -> Open Settings Dialog */
    DoMethod(btn_settings, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_SETTINGS);

    /* Change 3D Model String -> Load 3D Mesh into Viewport */
    DoMethod(str_stl, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, ID_OPEN_STL);

    /* Change import units without losing the model's placement. */
    DoMethod(cyc_scale, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, ID_IMPORT_SCALE);

    DoMethod(btn_apply, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_TRANSFORM_APPLY);
    DoMethod(btn_center, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_TRANSFORM_CENTER);
    DoMethod(btn_drop, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_TRANSFORM_DROP);
    DoMethod(btn_reset, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_TRANSFORM_RESET);
    for (int i=0; i<3; i++) {
        DoMethod(g_controls.position[i], MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
                 app, 2, MUIM_Application_ReturnID, ID_TRANSFORM_APPLY);
        DoMethod(g_controls.rotation[i], MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
                 app, 2, MUIM_Application_ReturnID, ID_TRANSFORM_APPLY);
        DoMethod(g_controls.scale[i], MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
                 app, 2, MUIM_Application_ReturnID, ID_TRANSFORM_APPLY);
    }

    /* Load Printer & Application Config */
    LoadConfig("PROGDIR:simpleslicer.cfg", &g_config);
    ResetModelTransform(&g_transform);
    RefreshTransformControls();

    /*
    ** Open Window and Enter Main Event Loop
    */
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_END);
    UpdatePreviewBed(area_3d_viewport);

    while (running)
    {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, &signals);

        switch (id)
        {
            case ID_QUIT:
            case (ULONG)MUIV_Application_ReturnID_Quit:
                running = FALSE;
                break;

            case ID_SETTINGS:
                OpenSettingsDialog(app, win, &g_config);
                UpdatePreviewBed(area_3d_viewport);
                RefreshTransformControls();
                break;

            case ID_IMPORT_SCALE:
            case ID_TRANSFORM_APPLY:
            case ID_TRANSFORM_CENTER:
            case ID_TRANSFORM_DROP:
            case ID_TRANSFORM_RESET:
                ApplyTransformControls(app, win, area_3d_viewport, cyc_scale, id);
                break;

            case ID_OPEN_STL:
                {
                    STRPTR model_file = NULL;
                    GetAttr(MUIA_String_Contents, str_stl, (ULONG *)(APTR)&model_file);
                    SetAttrs(area_3d_viewport, MUIA_MGLViewport_Mesh, 0, TAG_END);
                    FreeMesh3D(&g_active_mesh);
                    FreeMesh3D(&g_original_mesh);
                    g_mesh_loaded = FALSE;
                    g_loaded_path[0] = 0;
                    ResetModelTransform(&g_transform);
                    RefreshTransformControls();
                    if (!model_file || !*model_file) break;
                    if (strlen(model_file) >= sizeof(g_loaded_path)) {
                        MUI_Request(app, win, 0, "Model load error", "*_OK", "Model path is too long.", TAG_END);
                        break;
                    }

                    InitMesh3D(&g_original_mesh);
                    BOOL is_lwo = IsLWOFile(model_file);
                    BOOL is_obj = IsOBJFile(model_file);
                    BOOL parsed = is_lwo ? ParseLWO(model_file, &g_original_mesh) :
                        (is_obj ? ParseOBJ(model_file, &g_original_mesh) : ParseSTL(model_file, &g_original_mesh));
                    if (parsed) {
                        if (is_lwo && g_config.auto_rotate_lwo != 0) {
                            float angle = g_config.auto_rotate_lwo == 1 ? 90.0f : -90.0f;
                            RotateMeshX(&g_original_mesh, angle);
                        }
                        RecalculateMeshBounds(&g_original_mesh);
                        g_convert_units = is_lwo || is_obj;
                        g_mesh_loaded = TRUE;
                        if (UpdatePreview(area_3d_viewport, cyc_scale, &g_transform)) {
                            strcpy(g_loaded_path, model_file);
                            printf("3D Model Loaded: '%s' (%lu triangles)\n", model_file, g_active_mesh.triangle_count);
                            break;
                        }
                    }
                    g_mesh_loaded = FALSE;
                    FreeMesh3D(&g_original_mesh);
                    RefreshTransformControls();
                    MUI_Request(app, win, 0, "Model load error", "*_OK",
                        "Could not load or transform the selected model.", TAG_END);
                }
                break;

            case ID_SLICE:
                {
                    STRPTR stl_file = NULL;
                    STRPTR gcode_file = NULL;
                    STRPTR layer_height = NULL;
                    STRPTR wall_thick = NULL;
                    STRPTR print_speed = NULL;
                    STRPTR hotend_temp = NULL;
                    STRPTR bed_temp = NULL;
                    STRPTR travel_speed = NULL;
                    STRPTR retract_speed = NULL;
                    STRPTR retract_len = NULL;
                    STRPTR filament_dia = NULL;
                    STRPTR nozzle_dia = NULL;
                    STRPTR extra_args = NULL;
                    ULONG infill_val = 20;
                    ULONG fan_speed_val = 100;
                    ULONG support_idx = 0;
                    ULONG adhesion_idx = 0;
                    ULONG profile_idx = 0;

                    char auto_gcode[512] = {0};
                    char cmd_buffer[2048] = {0};
                    int ret_code = 0;

                    /* Get parameters from MUI Gadgets */
                    GetAttr(MUIA_String_Contents, str_stl, (ULONG *)(APTR)&stl_file);
                    GetAttr(MUIA_String_Contents, str_gcode, (ULONG *)(APTR)&gcode_file);
                    GetAttr(MUIA_Cycle_Active, cyc_profile, &profile_idx);
                    GetAttr(MUIA_String_Contents, str_layer_height, (ULONG *)(APTR)&layer_height);
                    GetAttr(MUIA_String_Contents, str_wall_thick, (ULONG *)(APTR)&wall_thick);
                    GetAttr(MUIA_Numeric_Value, sld_infill, &infill_val);
                    GetAttr(MUIA_String_Contents, str_speed, (ULONG *)(APTR)&print_speed);
                    GetAttr(MUIA_String_Contents, str_temp, (ULONG *)(APTR)&hotend_temp);
                    GetAttr(MUIA_String_Contents, str_bed_temp, (ULONG *)(APTR)&bed_temp);
                    GetAttr(MUIA_Cycle_Active, cyc_support, &support_idx);
                    GetAttr(MUIA_Cycle_Active, cyc_adhesion, &adhesion_idx);

                    GetAttr(MUIA_String_Contents, str_travel_speed, (ULONG *)(APTR)&travel_speed);
                    GetAttr(MUIA_String_Contents, str_retract_speed, (ULONG *)(APTR)&retract_speed);
                    GetAttr(MUIA_String_Contents, str_retract_len, (ULONG *)(APTR)&retract_len);
                    GetAttr(MUIA_String_Contents, str_filament_dia, (ULONG *)(APTR)&filament_dia);
                    GetAttr(MUIA_String_Contents, str_nozzle_dia, (ULONG *)(APTR)&nozzle_dia);
                    GetAttr(MUIA_Numeric_Value, sld_fan_speed, &fan_speed_val);
                    GetAttr(MUIA_String_Contents, str_extra_args, (ULONG *)(APTR)&extra_args);

                    char actual_stl_file[512] = {0};

                    /* Validate 3D Model Input File */
                    if (!stl_file || strlen(stl_file) == 0)
                    {
                        MUI_Request(app, win, 0, "CuraEngine Slicer Error", "*_OK", "Please select a 3D model file (.stl, .lwo, .obj) before slicing!", TAG_END);
                        break;
                    }

                    /* Apply pending edits, then export exactly the displayed geometry. */
                    if (!g_mesh_loaded || strcmp(stl_file, g_loaded_path) != 0) {
                        MUI_Request(app, win, 0, "Model not loaded", "*_OK",
                            "Load the selected model successfully before slicing.", TAG_END);
                        break;
                    }
                    if (!ApplyTransformControls(app, win, area_3d_viewport, cyc_scale, ID_TRANSFORM_APPLY)) break;
                    if (!isfinite(g_config.bed_x) || !isfinite(g_config.bed_y) || !isfinite(g_config.bed_z) ||
                        g_config.bed_x <= 0 || g_config.bed_y <= 0 || g_config.bed_z <= 0 ||
                        g_config.bed_x > 1000000 || g_config.bed_y > 1000000 || g_config.bed_z > 1000000) {
                        MUI_Request(app, win, 0, "Invalid build volume", "*_OK",
                            "Set positive, finite printer dimensions in Preferences.", TAG_END);
                        break;
                    }
                    if (!MeshFitsBuildVolume(&g_active_mesh, g_config.bed_x, g_config.bed_y, g_config.bed_z)) {
                        char warning[320];
                        snprintf(warning, sizeof(warning),
                            "Model extends outside the build volume.\n"
                            "Size: %.2f x %.2f x %.2f mm\n"
                            "Position XY: %.2f, %.2f mm; bottom Z: %.2f mm\n"
                            "Return to the preview to adjust placement or scale.",
                            g_active_mesh.max_bounds.x-g_active_mesh.min_bounds.x,
                            g_active_mesh.max_bounds.y-g_active_mesh.min_bounds.y,
                            g_active_mesh.max_bounds.z-g_active_mesh.min_bounds.z,
                            g_transform.position.x, g_transform.position.y, g_transform.position.z);
                        if (MUI_Request(app, win, 0, "Build Volume Warning", "*_Back|Slice Anyway|Cancel",
                            "%s", (ULONG)warning, TAG_END) != 2) break;
                    }
                    const char *temp_stl = "T:simpleslicer_temp.stl";
                    if (!ExportMeshToSTL(&g_active_mesh, temp_stl, 1.0f)) {
                        MUI_Request(app, win, 0, "STL Export Error", "*_OK",
                            "Failed to export transformed model for slicing.", TAG_END);
                        break;
                    }
                    strncpy(actual_stl_file, temp_stl, sizeof(actual_stl_file)-1);

                    /* Auto-generate G-Code filename if not provided */
                    if (!gcode_file || strlen(gcode_file) == 0)
                    {
                        strncpy(auto_gcode, stl_file, sizeof(auto_gcode) - 1);
                        char *dot = strrchr(auto_gcode, '.');
                        if (dot)
                        {
                            strcpy(dot, ".gcode");
                        }
                        else
                        {
                            strncat(auto_gcode, ".gcode", sizeof(auto_gcode) - strlen(auto_gcode) - 1);
                        }
                        gcode_file = auto_gcode;
                    }

                    /* Convert float strings (mm) to microns for CuraEngine parameters */
                    double lh_mm   = (layer_height && *layer_height) ? atof(layer_height) : 0.20;
                    double wt_mm   = (wall_thick && *wall_thick) ? atof(wall_thick) : 0.80;
                    double fd_mm   = (filament_dia && *filament_dia) ? atof(filament_dia) : 1.75;
                    double nd_mm   = (nozzle_dia && *nozzle_dia) ? atof(nozzle_dia) : 0.40;
                    double rlen_mm = (retract_len && *retract_len) ? atof(retract_len) : 4.50;

                    int lh_um   = (int)(lh_mm * 1000.0);
                    int fd_um   = (int)(fd_mm * 1000.0);
                    int nd_um   = (int)(nd_mm * 1000.0);
                    int rlen_um = (int)(rlen_mm * 1000.0);

                    int inset_count = (int)(wt_mm / (nd_mm > 0 ? nd_mm : 0.40) + 0.5);
                    if (inset_count < 1) inset_count = 1;

                    /* Construct CuraEngine 15.04.6 Command Line */
                    int command_length = snprintf(cmd_buffer, sizeof(cmd_buffer),
                        "C:CuraEngine -v "
                        "-s layerThickness=%d "
                        "-s initialLayerThickness=%d "
                        "-s insetCount=%d "
                        "-s infillOverlap=15 "
                        "-s filamentDiameter=%d "
                        "-s nozzleSize=%d "
                        "-s printSpeed=%s "
                        "-s moveSpeed=%s "
                        "-s retractionSpeed=%s "
                        "-s retractionAmount=%d "
                        "-s temperature=%s "
                        "%s "
                        "-s autoCenter=2 -s posx=%ld -s posy=%ld -s objectSink=0 "
                        "-o \"%s\" \"%s\"",
                        lh_um, lh_um, inset_count, fd_um, nd_um,
                        (print_speed && *print_speed) ? print_speed : "50",
                        (travel_speed && *travel_speed) ? travel_speed : "120",
                        (retract_speed && *retract_speed) ? retract_speed : "40",
                        rlen_um,
                        (hotend_temp && *hotend_temp) ? hotend_temp : "200",
                        (extra_args && *extra_args) ? extra_args : "",
                        (long)lroundf(g_config.bed_x * 500.0f),
                        (long)lroundf(g_config.bed_y * 500.0f),
                        gcode_file, actual_stl_file);

                    if (command_length < 0 || (size_t)command_length >= sizeof(cmd_buffer)) {
                        MUI_Request(app, win, 0, "Slicing error", "*_OK",
                            "Command too long. Shorten file paths or extra arguments.", TAG_END);
                        break;
                    }

                    printf("\n[SimpleSlicer] Invoking CuraEngine:\n%s\n\n", cmd_buffer);

                    /* Open dedicated AmigaOS CON: console window to display live output */
                    BPTR console_win = Open("CON:50/50/850/450/CuraEngine Slicer Console Output/AUTO/WAIT/CLOSE", MODE_NEWFILE);
                    if (console_win)
                    {
                        ret_code = SystemTags(cmd_buffer,
                            SYS_Output, console_win,
                            SYS_Input,  0,
                            TAG_END);
                        Close(console_win);
                    }
                    else
                    {
                        /* Fallback if CON: window could not be opened */
                        ret_code = SystemTags(cmd_buffer, TAG_END);
                    }

                    if (ret_code == 0)
                    {
                        MUI_Request(app, win, 0, "CuraEngine Slicer Success", "*_OK", "Slicing completed successfully!\nOutput file: %s", (ULONG)gcode_file, TAG_END);
                    }
                    else
                    {
                        MUI_Request(app, win, 0, "CuraEngine Slicer Warning", "*_OK", "CuraEngine execution finished with return code: %d\nPlease check C:CuraEngine installation.", (ULONG)ret_code, TAG_END);
                    }
                }
                break;

            default:
                break;
        }

        if (running && signals)
        {
            signals = Wait(signals);
        }
    }

    /*
    ** Clean Up and Exit
    */
    MUI_DisposeObject(app);
    if (g_mesh_loaded)
    {
        FreeMesh3D(&g_active_mesh);
        g_mesh_loaded = FALSE;
    }

    FreeMesh3D(&g_original_mesh);
    FreeMGLViewportClass();

    DropInterface((struct Interface *)IMUIMaster);
    CloseLibrary(MUIMasterBase);
    DropInterface((struct Interface *)IUtility);
    CloseLibrary(UtilityBase);
    DropInterface((struct Interface *)IIntuition);
    CloseLibrary(IntuitionBase);

    return 0;
}
