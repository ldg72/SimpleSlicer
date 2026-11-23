"""Run the actual geometry and STL code on macOS, without an Amiga runtime."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="examplemac-transform-") as directory:
    temp = Path(directory)
    (temp / "exec").mkdir()
    (temp / "GL").mkdir()
    # Only platform types/header locations are adapted. No geometry is mocked.
    (temp / "exec/types.h").write_text(
        "#pragma once\ntypedef unsigned long ULONG; typedef int BOOL;\n"
        "#define TRUE 1\n#define FALSE 0\n"
    )
    for name, target in (("gl", "OpenGL/gl.h"), ("glu", "OpenGL/glu.h"), ("glut", "GLUT/glut.h")):
        (temp / f"GL/{name}.h").write_text(f"#include <{target}>\n")
    binary = temp / "transform_test"
    subprocess.run([
        "clang", "-std=c99", "-Wall", "-Wextra", "-Werror",
        "-Wno-deprecated-declarations", "-Wno-unused-variable",
        "-fsanitize=undefined",
        "-Wl,-dead_strip", "-Wl,-dead_strip_dylibs",
        "-I", str(temp), "-I", str(root),
        str(root / "model_transform.c"), str(root / "lwo_parser.c"),
        str(root / "tests/model_transform_test.c"),
        "-framework", "OpenGL", "-framework", "GLUT", "-o", str(binary)
    ], check=True, timeout=60)
    subprocess.run([str(binary), str(temp / "roundtrip.stl")], check=True, timeout=30)
