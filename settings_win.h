/*
** =========================================================================
** settings_win.h - SimpleSlicer Configuration & Preferences Window for AmigaOS
** =========================================================================
*/

#ifndef SETTINGS_WIN_H
#define SETTINGS_WIN_H

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG)(a)<<24 | (ULONG)(b)<<16 | (ULONG)(c)<<8 | (ULONG)(d))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float bed_x;          /* Printer bed width X in mm (default: 220.0) */
    float bed_y;          /* Printer bed depth Y in mm (default: 220.0) */
    float bed_z;          /* Printer bed height Z in mm (default: 250.0) */
    int auto_rotate_lwo;  /* 0=Disabled, 1=+90deg X (Right Side Up), 2=-90deg X */
    BOOL smart_downscale; /* Downscale ONLY if model exceeds bed size */
} AppConfig;

/* Global application configuration instance */
extern AppConfig g_config;

/* Initialize configuration with default values */
void InitDefaultConfig(AppConfig *cfg);

/* Load configuration from file (PROGDIR:simpleslicer.cfg) */
BOOL LoadConfig(const char *filename, AppConfig *cfg);

/* Save configuration to file (PROGDIR:simpleslicer.cfg) */
BOOL SaveConfig(const char *filename, const AppConfig *cfg);

/* Open MUI Preferences Dialog Window */
BOOL OpenSettingsDialog(Object *app, Object *parent_win, AppConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_WIN_H */
