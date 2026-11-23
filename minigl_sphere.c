/*
** =========================================================================
** minigl_sphere.c - Standalone MiniGL 3D Sphere Viewer for AmigaOS 4
** Verifies MiniGL / Warp3D hardware acceleration and 3D rendering pipeline.
** =========================================================================
*/

#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <proto/exec.h>

/* Minimum stack space for AmigaOS */
static USED const char *stack = "$STACK:65535";

static float rot_angle = 0.0f;

/*
** Set up OpenGL 3D Lighting and Material Properties
*/
static void InitLighting(void)
{
    GLfloat ambient_light[]  = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat diffuse_light[]  = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat specular_light[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light_position[] = { 3.0f, 4.0f, 5.0f, 1.0f };

    GLfloat mat_ambient[]   = { 0.1f, 0.3f, 0.5f, 1.0f };
    GLfloat mat_diffuse[]   = { 0.2f, 0.6f, 0.9f, 1.0f };
    GLfloat mat_specular[]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat mat_shininess[] = { 50.0f };

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient_light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse_light);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular_light);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

    glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
}

/*
** Display Callback: Renders Passive 3D Sphere
*/
static void DisplayCallback(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0.0, 0.0, 4.5,   /* Eye Position */
              0.0, 0.0, 0.0,   /* Look-At Target */
              0.0, 1.0, 0.0);  /* Up Vector */

    glPushMatrix();
    glRotatef(25.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(rot_angle, 0.0f, 1.0f, 0.0f);

    /* Render 3D Lit Solid Sphere */
    glutSolidSphere(1.3, 36, 36);

    glPopMatrix();

    glutSwapBuffers();
}

/*
** Reshape Callback: Updates Viewport Aspect Ratio
*/
static void ReshapeCallback(int width, int height)
{
    if (height == 0) height = 1;
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (GLfloat)width / (GLfloat)height, 0.1, 100.0);
}

/*
** Idle Callback: Smooth Sphere Rotation Animation
*/
static void IdleCallback(void)
{
    rot_angle += 0.5f;
    if (rot_angle >= 360.0f) rot_angle -= 360.0f;
    glutPostRedisplay();
}

/*
** Keyboard Callback
*/
static void KeyboardCallback(unsigned char key, int x, int y)
{
    (void)x; (void)y;
    if (key == 27 || key == 'q' || key == 'Q')
    {
        exit(0);
    }
}

int main(int argc, char **argv)
{
    printf("=====================================================\n");
    printf(" SimpleSlicer MiniGL 3D Sphere Test (AmigaOS 4)\n");
    printf(" Press 'Q' or ESC to exit.\n");
    printf("=====================================================\n");

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(600, 500);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("SimpleSlicer MiniGL 3D Sphere Viewport Test");

    InitLighting();

    glutDisplayFunc(DisplayCallback);
    glutReshapeFunc(ReshapeCallback);
    glutIdleFunc(IdleCallback);
    glutKeyboardFunc(KeyboardCallback);

    glutMainLoop();
    return 0;
}
