/*
** =========================================================================
** mgl_viewport.h - Embedded MiniGL 3D Viewport MUI Custom Class for AmigaOS 4
** =========================================================================
*/

#ifndef MGL_VIEWPORT_H
#define MGL_VIEWPORT_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/intuition.h>

#include "lwo_parser.h"
#include "settings_win.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Custom MUI Attributes for MGL Viewport */
#define MUIA_MGLViewport_Mesh      (TAG_USER + 0x1000)
#define MUIA_MGLViewport_BedX      (TAG_USER + 0x1001)
#define MUIA_MGLViewport_BedY      (TAG_USER + 0x1002)
#define MUIA_MGLViewport_RotX      (TAG_USER + 0x1003)
#define MUIA_MGLViewport_RotY      (TAG_USER + 0x1004)
#define MUIA_MGLViewport_Zoom      (TAG_USER + 0x1005)
#define MUIA_MGLViewport_PanX      (TAG_USER + 0x1006)
#define MUIA_MGLViewport_PanY      (TAG_USER + 0x1007)

/* Custom Class Creation & Cleanup Prototypes */
BOOL InitMGLViewportClass(void);
void FreeMGLViewportClass(void);
struct MUI_CustomClass *GetMGLViewportClass(void);

#ifdef __cplusplus
}
#endif

#endif /* MGL_VIEWPORT_H */
