@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist build\volume-input mkdir build\volume-input
pushd build\volume-input
cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /LD ..\..\src\performance\volume_input.cpp ..\..\src\performance\options_native_values.cpp ..\..\src\performance\dialog_context.cpp ..\..\src\performance\dialog_text_capture.cpp ..\..\src\performance\native_ui_text.cpp ..\..\src\performance\native_ui_text_hook.cpp ..\..\src\performance\native_ui_text_mask.cpp ..\..\src\performance\ui_vector_text.cpp /link /DEF:..\..\src\input\dinput8.def /MACHINE:X86 /OUT:dinput8.dll /MAP:input-component.map bcrypt.lib user32.lib dxguid.lib dwrite.lib d2d1.lib ole32.lib
set "buildResult=%errorlevel%"
popd
popd
exit /b %buildResult%
