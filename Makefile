# =========================================================================
# Makefile for SimpleSlicer (AmigaOS 4)
# =========================================================================

CC      = ppc-amigaos-gcc
CRT     = newlib

# DoMethod / DoSuperMethod / CoerceMethod must be mapped onto the
# Intuition interfaces for AmigaOS 4.
REDEFINE = -DCoerceMethod=ICoerceMethod -DDoMethod=IDoMethod \
           -DDoSuperMethod=IDoSuperMethod -DDoSuperMethodA=IDoSuperMethodA

WARN    = -Wall -Wextra
OPT     = -O2

CFLAGS  = $(OPT) $(WARN) -mcrt=$(CRT) -D__USE_INLINE__ $(REDEFINE)
LDFLAGS = -mcrt=$(CRT) -Wl,--allow-multiple-definition
LIBS    = -lglut -lGL -lGLU -lauto -lm

TARGET  = SimpleSlicer
TARGET_MGL = minigl_sphere
OBJS    = main.o lwo_parser.o settings_win.o mgl_viewport.o model_transform.o

all: $(TARGET) $(TARGET_MGL)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

$(TARGET_MGL): minigl_sphere.c
	$(CC) $(CFLAGS) minigl_sphere.c -o $(TARGET_MGL) -lglut -lGL -lGLU -lauto -lm

main.o: main.c lwo_parser.h settings_win.h mgl_viewport.h model_transform.h
	$(CC) $(CFLAGS) -c main.c -o main.o

lwo_parser.o: lwo_parser.c lwo_parser.h
	$(CC) $(CFLAGS) -c lwo_parser.c -o lwo_parser.o

settings_win.o: settings_win.c settings_win.h
	$(CC) $(CFLAGS) -c settings_win.c -o settings_win.o

mgl_viewport.o: mgl_viewport.c mgl_viewport.h lwo_parser.h settings_win.h
	$(CC) $(CFLAGS) -c mgl_viewport.c -o mgl_viewport.o

model_transform.o: model_transform.c model_transform.h lwo_parser.h
	$(CC) $(CFLAGS) -c model_transform.c -o model_transform.o

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_MGL) errore_di_compilazione

.PHONY: all clean
