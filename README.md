# Yixin-Board

**This project is a modified version of [Yixin-Board](https://github.com/accreator/Yixin-Board) by [accreator](https://github.com/accreator).**

**It is modified to have better support for [Rapfi](https://github.com/dhbloo/Rapfi)'s features.**

**The code has been migrated to GTK4, with support for building Windows/MacOS/Linux App.**

Specially designed GUI for [**Yixin**](http://www.aiexp.info/pages/yixin.html)

Features:

- Written by GTK+ which is cross-platform, supporting Windows/Mac/Linux/BSD.
- Open source code (Simplified BSD License).

- Although Yixin Board is designed for Yixin, it also supports engines which are compatible with Yixin's protocol such as Tito.

Because Gomocup protocol has some limitations, Yixin-Board uses a new protocol which modifies and extends Gomocup protocol. The new protocol is described in https://github.com/accreator/Yixin-protocol .

For more information, please visit www.aiexp.info and send me e-mail: sunkaicn@gmail.com

## Running the tests

Unit tests cover the `src/model/` and `src/engine/` layers (no GTK, no display server required).
They use [doctest](https://github.com/doctest/doctest) (vendored, single header, at
`tests/vendor/doctest.h`) and are wired into CMake as the `ranls-gui-tests` target.

From a build directory configured with the top-level `CMakeLists.txt`:

```sh
cmake -S . -B build
cmake --build build --target ranls-gui-tests
ctest --test-dir build --output-on-failure
```

Or, to build the app and run the tests in one step:

```sh
RUN_TESTS=1 ./build.sh          # Linux/macOS
RUN_TESTS=1 ./build_msys2.sh    # MSYS2 (MINGW64/UCRT64 shell)
```

Set `-DRANLS_GUI_BUILD_TESTS=OFF` when configuring CMake to skip building the test target
entirely (e.g. for a packaging build that shouldn't need `sigc++` dev headers standalone).

The test suite no longer depends on POSIX `/bin/cat` / `/bin/true` — CMake builds two tiny
portable stand-in engines (`tests/mock_engine/`, targets `mock_engine` and `mock_engine_quit`)
and passes their paths to the test binaries as compile definitions (PORT-03). They build on
any toolchain.

## Building on Windows

**MSYS2 MINGW64 / UCRT64** (recommended, GCC + pkg-config):

```sh
pacman -S mingw-w64-x86_64-gtkmm4 mingw-w64-x86_64-zlib mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc
./build_msys2.sh
```

**Native MSVC** (PORT-03): the top-level `CMakeLists.txt` has an `if(MSVC)` branch that swaps
the GCC/Clang `-Wall -Wextra -Wpedantic` flags for `/W4 /permissive-`, and `ranls-gui` is built
with the `WIN32` subsystem so no `cmd.exe` console window appears on launch. MSVC has no
`pkg-config` on `PATH`, so provide gtkmm-4.0 + zlib through a
[vcpkg](https://vcpkg.io) toolchain file (vcpkg ships a `pkg-config` shim the CMake logic uses):

```bat
vcpkg install gtkmm zlib
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

The Win32 Job Object in `src/engine/engine_process.cpp` (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`)
ensures the engine subprocess is terminated if the GUI process dies — the Windows analogue of
the Linux `PR_SET_PDEATHSIG` guard. macOS has no clean equivalent and keeps the plain
`Gio::Subprocess` fallback.

# 弈心界面程序

**本项目是[Yixin-Board](https://github.com/accreator/Yixin-Board)的修改版本，原代码由[accreator](https://github.com/accreator)开发。**

**它被修改为更好地支持[Rapfi](https://github.com/dhbloo/Rapfi)的功能。**

**代码已迁移到 GTK4，支持构建 Windows/MacOS/Linux 应用程序。**

为[**弈心**](http://www.aiexp.info/pages/yixin-cn.html)引擎设计的图形用户界面

特性：

- 基于 GTK+的设计使得本程序可跨多种平台，支持 Windows/Mac/Linux/BSD。

- 开源（基于 Simplified BSD 协议）

- 尽管本程序最初是为弈心设计的，它也支持与弈心协议兼容的引擎，如你可以在无禁规则下使用 Tito 等引擎。

因为 Gomocup 协议有部分局限，弈心界面程序采用了一套修改并扩充于 Gomocup 协议的新协议。新协议的描述可见于 https://github.com/accreator/Yixin-protocol 。

更多信息，请访问 www.aiexp.info 或通过 email 联系我： sunkaicn@gmail.com
