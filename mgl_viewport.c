/*
** =========================================================================
** mgl_viewport.c - Embedded MiniGL 3D Viewport MUI Custom Class for AmigaOS 4
** =========================================================================
*/

#include "mgl_viewport.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/minigl.h>

#include <inline4/exec.h>
#include <inline4/graphics.h>
#include <inline4/intuition.h>
#include <inline4/muimaster.h>
#include <inline4/minigl.h>

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <mgl/gl.h>
#include <mgl/minigl.h>

#include <stdio.h>
#include <stdlib.h>

extern struct MUIMasterIFace *IMUIMaster;
extern struct IntuitionIFace *IIntuition;

#ifndef SAVEDS
#define SAVEDS
#endif

#ifndef ENTRY
#define ENTRY(func) (APTR)func
#endif

/*
** Custom Class Instance Data
*/
struct MGLViewportData
{
    const Mesh3D *mesh;
    float bed_x;
    float bed_y;
    float rot_x;
    float rot_y;
    float zoom;

    BOOL dragging;
    struct MUI_EventHandlerNode event_handler;
    LONG last_mouse_x;
    LONG last_mouse_y;

    void *gl_context;
    struct BitMap *bitmap;
    GLsizei last_w;
    GLsizei last_h;
};

static struct MUI_CustomClass *MGLViewportClass = NULL;

/* Let MUI manage the window's mouse reporting and event subscriptions. */
static void SetViewportDragging(Object *obj, struct MGLViewportData *data, BOOL dragging)
{
    if (data->dragging == dragging) return;

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->event_handler);
    data->dragging = dragging;
    if (dragging)
        data->event_handler.ehn_Events |= IDCMP_MOUSEMOVE;
    else
        data->event_handler.ehn_Events &= ~IDCMP_MOUSEMOVE;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->event_handler);
}

/*
** MUI Custom Class Dispatcher
*/
static ULONG SAVEDS MGLViewportDispatcher(struct IClass *cl, Object *obj, Msg msg)
{
    struct MGLViewportData *data = INST_DATA(cl, obj);

    switch (msg->MethodID)
    {
        case OM_NEW:
            {
                Object *new_obj = (Object *)DoSuperMethodA(cl, obj, msg);
                if (new_obj)
                {
                    SetAttrs(new_obj, MUIA_FillArea, FALSE, TAG_END);
                    struct MGLViewportData *d = INST_DATA(cl, new_obj);
                    d->mesh = NULL;
                    d->bed_x = 220.0f;
                    d->bed_y = 220.0f;
                    d->rot_x = 25.0f;
                    d->rot_y = 35.0f;
                    d->zoom = 1.0f;
                    d->dragging = FALSE;
                    d->last_mouse_x = 0;
                    d->last_mouse_y = 0;
                    d->gl_context = NULL;
                    d->bitmap = NULL;
                    d->last_w = 0;
                    d->last_h = 0;
                }
                return (ULONG)new_obj;
            }

        case OM_DISPOSE:
            {
                if (data->gl_context)
                {
                    mglMakeCurrent(data->gl_context);
                    mglDeleteContext();
                    data->gl_context = NULL;
                }
                if (data->bitmap)
                {
                    FreeBitMap(data->bitmap);
                    data->bitmap = NULL;
                }
                return DoSuperMethodA(cl, obj, msg);
            }

        case MUIM_Setup:
            {
                if (!DoSuperMethodA(cl, obj, msg)) return FALSE;
                data->dragging = FALSE;
                data->event_handler.ehn_Object = obj;
                data->event_handler.ehn_Class = cl;
                data->event_handler.ehn_Priority = 0;
                data->event_handler.ehn_Flags = MUI_EHF_GUIMODE;
                data->event_handler.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY |
                    IDCMP_EXTENDEDMOUSE | IDCMP_INACTIVEWINDOW;
                DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->event_handler);
                return TRUE;
            }

        case MUIM_Cleanup:
            {
                DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->event_handler);
                data->dragging = FALSE;
                return DoSuperMethodA(cl, obj, msg);
            }

        case MUIM_Hide:
            {
                SetViewportDragging(obj, data, FALSE);
                return DoSuperMethodA(cl, obj, msg);
            }

        case OM_SET:
            {
                struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList;
                struct TagItem *tag;

                while ((tag = NextTagItem(&tags)))
                {
                    union { ULONG u; float f; } val;
                    val.u = tag->ti_Data;

                    switch (tag->ti_Tag)
                    {
                        case MUIA_MGLViewport_Mesh:
                            data->mesh = (const Mesh3D *)tag->ti_Data;
                            DoMethod(obj, MUIM_Draw, MADF_DRAWOBJECT);
                            break;
                        case MUIA_MGLViewport_BedX:
                            data->bed_x = val.f;
                            break;
                        case MUIA_MGLViewport_BedY:
                            data->bed_y = val.f;
                            break;
                        case MUIA_MGLViewport_RotX:
                            data->rot_x = val.f;
                            DoMethod(obj, MUIM_Draw, MADF_DRAWOBJECT);
                            break;
                        case MUIA_MGLViewport_RotY:
                            data->rot_y = val.f;
                            DoMethod(obj, MUIM_Draw, MADF_DRAWOBJECT);
                            break;
                        case MUIA_MGLViewport_Zoom:
                            data->zoom = val.f;
                            DoMethod(obj, MUIM_Draw, MADF_DRAWOBJECT);
                            break;
                    }
                }
                return DoSuperMethodA(cl, obj, msg);
            }

        case MUIM_AskMinMax:
            {
                struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
                DoSuperMethodA(cl, obj, msg);

                m->MinMaxInfo->MinWidth  += 250;
                m->MinMaxInfo->MinHeight += 250;
                m->MinMaxInfo->DefWidth  += 450;
                m->MinMaxInfo->DefHeight += 450;
                m->MinMaxInfo->MaxWidth  += MUI_MAXMAX;
                m->MinMaxInfo->MaxHeight += MUI_MAXMAX;
                return 0;
            }

        case MUIM_Draw:
            {
                /* DO NOT call DoSuperMethodA here - prevents MUI background erasing flicker */
                LONG x = _mleft(obj);
                LONG y = _mtop(obj);
                LONG w = _mwidth(obj);
                LONG h = _mheight(obj);

                if (w <= 10 || h <= 10) return 0;
                if (!_window(obj) || !_rp(obj)) return 0;

                if (!data->gl_context || !data->bitmap || data->last_w != w || data->last_h != h)
                {
                    if (data->gl_context)
                    {
                        mglMakeCurrent(data->gl_context);
                        mglDeleteContext();
                        data->gl_context = NULL;
                    }
                    if (data->bitmap)
                    {
                        FreeBitMap(data->bitmap);
                        data->bitmap = NULL;
                    }

                    struct BitMap *friend_bm = _window(obj)->RPort->BitMap;
                    data->bitmap = AllocBitMap(w, h, 32, BMF_MINPLANES | BMF_DISPLAYABLE, friend_bm);

                    if (data->bitmap)
                    {
                        data->gl_context = CreateContextTags(
                            MGLCC_Bitmap, (ULONG)data->bitmap,
                            TAG_END);
                    }

                    data->last_w = w;
                    data->last_h = h;
                }

                if (data->gl_context && data->bitmap)
                {
                    mglMakeCurrent(data->gl_context);
                    glViewport(0, 0, w, h);

                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    gluPerspective(45.0, (GLfloat)w / (GLfloat)h, 1.0, 5000.0);

                    if (data->mesh && data->mesh->triangle_count > 0)
                    {
                        RenderMeshDisplayList(0, data->mesh, data->rot_x, data->rot_y, 0.0f, 0.0f, data->zoom, data->bed_x, data->bed_y);
                    }
                    else
                    {
                        /* Render Empty Printer Bed Grid with Frontal Light */
                        glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                        glMatrixMode(GL_MODELVIEW);
                        glLoadIdentity();
                        gluLookAt(0.0, -350.0 * (data->zoom > 0.1f ? data->zoom : 1.0f), 200.0 * (data->zoom > 0.1f ? data->zoom : 1.0f), 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);

                        glPushMatrix();
                        glRotatef(data->rot_x, 1.0f, 0.0f, 0.0f);
                        glRotatef(data->rot_y, 0.0f, 0.0f, 1.0f);

                        glDisable(GL_LIGHTING);
                        glColor3f(0.3f, 0.35f, 0.4f);
                        glLineWidth(1.0f);
                        glBegin(GL_LINES);
                        float bed_hx = (data->bed_x > 10.0f ? data->bed_x : 220.0f) * 0.5f;
                        float bed_hy = (data->bed_y > 10.0f ? data->bed_y : 220.0f) * 0.5f;
                        for (float gx = -bed_hx; gx <= bed_hx; gx += 20.0f)
                        {
                            glVertex3f(gx, -bed_hy, 0.0f);
                            glVertex3f(gx, bed_hy, 0.0f);
                        }
                        for (float gy = -bed_hy; gy <= bed_hy; gy += 20.0f)
                        {
                            glVertex3f(-bed_hx, gy, 0.0f);
                            glVertex3f(bed_hx, gy, 0.0f);
                        }
                        glEnd();
                        glPopMatrix();
                    }

                    glFlush();

                    BltBitMapRastPort(data->bitmap, 0, 0, _rp(obj), x, y, w, h, 0xC0);
                }
                return 0;
            }

        case MUIM_HandleEvent:
            {
                struct MUIP_HandleEvent *m = (struct MUIP_HandleEvent *)msg;
                if (!m->imsg) return 0;

                LONG mx = m->imsg->MouseX;
                LONG my = m->imsg->MouseY;
                BOOL inside = mx >= _mleft(obj) && mx <= _mright(obj) &&
                              my >= _mtop(obj) && my <= _mbottom(obj);

                if (m->imsg->Class == IDCMP_INACTIVEWINDOW)
                {
                    SetViewportDragging(obj, data, FALSE);
                    return 0;
                }

                if (m->imsg->Class == IDCMP_MOUSEBUTTONS)
                {
                    if (m->imsg->Code == SELECTDOWN && inside)
                    {
                        data->last_mouse_x = mx;
                        data->last_mouse_y = my;
                        SetViewportDragging(obj, data, TRUE);
                        return MUI_EventHandlerRC_Eat;
                    }
                    else if (m->imsg->Code == SELECTUP && data->dragging)
                    {
                        SetViewportDragging(obj, data, FALSE);
                        return MUI_EventHandlerRC_Eat;
                    }
                }

                /* Legacy wheel messages are raw keys, not mouse buttons. */
                if (m->imsg->Class == IDCMP_RAWKEY && inside)
                {
                    if (m->imsg->Code == 0x7A) /* Mouse Wheel Up -> Zoom In */
                    {
                        data->zoom *= 0.90f;
                        if (data->zoom < 0.2f) data->zoom = 0.2f;
                        MUI_Redraw(obj, MADF_DRAWOBJECT);
                        return MUI_EventHandlerRC_Eat;
                    }
                    else if (m->imsg->Code == 0x7B) /* Mouse Wheel Down -> Zoom Out */
                    {
                        data->zoom *= 1.10f;
                        if (data->zoom > 5.0f) data->zoom = 5.0f;
                        MUI_Redraw(obj, MADF_DRAWOBJECT);
                        return MUI_EventHandlerRC_Eat;
                    }
                }
                else if (m->imsg->Class == IDCMP_EXTENDEDMOUSE && inside)
                {
                    if (m->imsg->Code == IMSGCODE_INTUIWHEELDATA)
                    {
                        struct IntuiWheelData *wdata = (struct IntuiWheelData *)m->imsg->IAddress;
                        if (wdata && wdata->WheelY != 0)
                        {
                            if (wdata->WheelY < 0)
                            {
                                /* Scroll Up -> Zoom In */
                                data->zoom *= 0.90f;
                                if (data->zoom < 0.2f) data->zoom = 0.2f;
                            }
                            else if (wdata->WheelY > 0)
                            {
                                /* Scroll Down -> Zoom Out */
                                data->zoom *= 1.10f;
                                if (data->zoom > 5.0f) data->zoom = 5.0f;
                            }
                            MUI_Redraw(obj, MADF_DRAWOBJECT);
                            return MUI_EventHandlerRC_Eat;
                        }
                    }
                }
                else if (m->imsg->Class == IDCMP_MOUSEMOVE && data->dragging)
                {
                    LONG mx = m->imsg->MouseX;
                    LONG my = m->imsg->MouseY;
                    LONG dx = mx - data->last_mouse_x;
                    LONG dy = my - data->last_mouse_y;

                    if (dx != 0 || dy != 0)
                    {
                        data->rot_y += (float)dx * 0.5f;
                        data->rot_x += (float)dy * 0.5f;

                        data->last_mouse_x = mx;
                        data->last_mouse_y = my;

                        MUI_Redraw(obj, MADF_DRAWOBJECT);
                    }
                    return MUI_EventHandlerRC_Eat;
                }
                return 0;
            }

        default:
            return DoSuperMethodA(cl, obj, msg);
    }
}

/*
** Create and Register Custom MUI MGL Viewport Class
*/
BOOL InitMGLViewportClass(void)
{
    if (MGLViewportClass) return TRUE;

    MGLViewportClass = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct MGLViewportData), ENTRY(MGLViewportDispatcher));
    if (!MGLViewportClass)
    {
        printf("Error: Could not create MGL Viewport MUI Custom Class\n");
        return FALSE;
    }
    return TRUE;
}

void FreeMGLViewportClass(void)
{
    if (MGLViewportClass)
    {
        MUI_DeleteCustomClass(MGLViewportClass);
        MGLViewportClass = NULL;
    }
}

struct MUI_CustomClass *GetMGLViewportClass(void)
{
    return MGLViewportClass;
}
