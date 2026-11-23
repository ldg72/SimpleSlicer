# SimpleSlicer

SimpleSlicer is a MUI graphical frontend for CuraEngine on AmigaOS 4.
It loads STL, OBJ and LightWave LWO models, provides a MiniGL 3D preview
with position, rotation and scale controls, and invokes an external
CuraEngine executable to generate G-code.

## Runtime dependencies

- AmigaOS 4.1 Final Edition.
- MUI with `muimaster.library` version 19 or later.
- MiniGL, its GLUT support, and a compatible graphics backend/driver.
- CuraEngine 15.04.6 for AmigaOS 4.1 FE, installed as `C:CuraEngine`.

## Get CuraEngine

Use the AmigaOS port maintained by Luca De Giovanni:

- [GitHub: CuraEngine 15.04.6 for AmigaOS 4.1 FE](https://github.com/ldg72/CuraEngine-15.04.6-for-AmigaOS-4.1FE)
  contains the source code and port documentation.
- [OS4Depot: CuraEngine](https://os4depot.net/?function=showfile&file=graphics/edit/curaengine.lha)
  provides the downloadable AmigaOS executable (currently 15.04.6-r1).

After extracting the archive, open a Shell in the directory containing
the `CuraEngine` executable and install it with:

```text
Copy CuraEngine C:
```

Check that it can run:

```text
C:CuraEngine -h
```

SimpleSlicer currently uses the fixed path `C:CuraEngine` and the legacy
15.04 command-line interface. A newer CuraEngine release is not a drop-in
replacement. CuraEngine is installed separately and is not bundled here.

## Build dependencies

- AmigaOS 4 SDK with the newlib development files.
- MUI development headers.
- MiniGL, GLUT and GLU development headers and link libraries.
- A GCC toolchain targeting AmigaOS 4 and `make`.

The Makefile defaults to `ppc-amigaos-gcc` and links with
`-lglut -lGL -lGLU -lauto -lm`. Configure the toolchain so it can find the
SDK headers and libraries. The SDK is not distributed in this repository;
a local `SDK/` directory is ignored by Git.

## Build and run

Build the application:

```sh
make SimpleSlicer
```

If your AmigaOS compiler command is `gcc`, use `make CC=gcc SimpleSlicer`.
Running `make` builds both `SimpleSlicer` and the optional `minigl_sphere`
graphics test. `make clean` removes the generated executables and objects.

Run `SimpleSlicer` on AmigaOS. Keep `simpleslicer.cfg` alongside the
executable to use the supplied configuration, or configure the printer
dimensions in Preferences. Select a model and output G-code file, adjust
the slicing settings and model transforms, then click Slice.

See [Model transforms](MODEL_TRANSFORMS.md) for positioning, units,
rotation, scaling and build-volume behavior.

## Geometry tests on macOS

With Python 3, Clang and the macOS OpenGL/GLUT frameworks available:

```sh
python3 tests/run_host_tests.py
```

These tests cover the geometry and STL code; they do not run the AmigaOS
interface or CuraEngine.

## License

SimpleSlicer is distributed under the [MIT License](LICENSE).
Copyright (c) 2026 Luca De Giovanni.

External dependencies retain their own licenses; see
[Third-party notices](THIRD_PARTY_NOTICES). CuraEngine is licensed
separately under AGPLv3.
