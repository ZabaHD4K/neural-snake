@echo off
set "MSVC_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
set "WINSDK_DIR=C:\Program Files (x86)\Windows Kits\10"
set "WINSDK_VER=10.0.26100.0"
set "PATH=%MSVC_DIR%\bin\HostX64\x64;%WINSDK_DIR%\bin\%WINSDK_VER%\x64;C:\Program Files\CMake\bin;C:\Users\aleja\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
set "INCLUDE=%MSVC_DIR%\include;%WINSDK_DIR%\Include\%WINSDK_VER%\ucrt;%WINSDK_DIR%\Include\%WINSDK_VER%\shared;%WINSDK_DIR%\Include\%WINSDK_VER%\winrt;%WINSDK_DIR%\Include\%WINSDK_VER%\um"
set "LIB=%MSVC_DIR%\lib\x64;%WINSDK_DIR%\Lib\%WINSDK_VER%\ucrt\x64;%WINSDK_DIR%\Lib\%WINSDK_VER%\um\x64"
cd /d C:\Users\aleja\Desktop\proyectospersonales\neural-snake\build
cmake --build . > C:\Users\aleja\Desktop\proyectospersonales\neural-snake\build_log.txt 2>&1
echo %errorlevel% > C:\Users\aleja\Desktop\proyectospersonales\neural-snake\build_result.txt
