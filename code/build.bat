@echo off

cd ..
IF NOT EXIST build mkdir build
pushd build
cl -DHANDMADE_WIN32=1 -Zi ..\code\win32_handmade.cpp user32.lib Gdi32.lib
popd
