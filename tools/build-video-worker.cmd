@echo off
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist build\video-worker mkdir build\video-worker
pushd build\video-worker
cl /nologo /utf-8 /std:c++17 /EHsc /W4 /WX /O2 /MT /DUNICODE /D_UNICODE ..\..\src\testing\menu_video_worker.cpp ..\..\src\performance\menu_video.cpp /link /SUBSYSTEM:WINDOWS /MACHINE:X64 /OUT:"AITD Video Worker.exe" mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib propsys.lib shlwapi.lib shell32.lib
set "buildResult=%errorlevel%"
popd
popd
exit /b %buildResult%
