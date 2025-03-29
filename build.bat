@echo off

cl

if %ERRORLEVEL% neq 0 (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)

cls

rmdir /S /Q .\build
mkdir build
pushd .\build

cl /nologo /O2 /diagnostics:caret /sdl /Wall /WX /W4 /wd4189 /wd4996 /wd4100 /wd4244 /wd4255 /wd5045 /wd4711 /wd4710 /Wv:18 ..\main.cpp

if %ERRORLEVEL% equ 0 (
  popd
  build\main
) else (
  popd
  echo "Error"
)
