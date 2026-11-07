@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b 1
call "%~dp0build-video-worker.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist build\performance mkdir build\performance
pushd build\performance
rem Licensed UI sounds are optional and prepared separately from external inputs.
set "uiSoundResources="
if exist "ui-sounds.res" set "uiSoundResources=ui-sounds.res"
cl /nologo /utf-8 /std:c++17 /EHsc /W4 /WX /O2 /MT /LD ..\..\src\performance\proxy.cpp ..\..\src\performance\native_lod.cpp ..\..\src\performance\sss_diffusion.cpp ..\..\src\performance\sss_native_recipes.cpp ..\..\src\performance\sss_skin_targets.cpp ..\..\src\performance\sss_timing.cpp ..\..\src\performance\gtao_renderer.cpp ..\..\src\performance\native_occlusion.cpp ..\..\src\performance\skin_material_catalog.cpp ..\..\src\performance\sss_capture.cpp ..\..\src\diagnostics\exports.cpp ..\..\src\performance\menu_video.cpp ..\..\src\performance\menu_presentation.cpp ..\..\src\performance\ui_vector_text.cpp ..\..\src\performance\ui_theme.cpp ..\..\src\performance\ui_art.cpp ..\..\src\performance\pause_presentation.cpp ..\..\src\performance\pause_art.cpp ..\..\src\performance\favorites_art.cpp ..\..\src\performance\recap_art.cpp ..\..\src\performance\calibration_art.cpp ..\..\src\performance\options_presentation.cpp ..\..\src\performance\options_children_presentation.cpp ..\..\src\performance\options_children_art.cpp ..\..\src\performance\dialog_presentation.cpp ..\..\src\performance\main_popup_art.cpp ..\..\src\performance\controls_art.cpp ..\..\src\performance\chapters_art.cpp ..\..\src\performance\loading_art.cpp ..\..\src\performance\episode_end_art.cpp ..\..\src\performance\ui_rich_keycaps.cpp ..\..\src\performance\inventory_ui_art.cpp ..\..\src\performance\inventory_popup_art.cpp ..\..\src\performance\general_hud_art.cpp ..\..\src\performance\interaction_ui_art.cpp ..\..\src\performance\tutorial_ui_art.cpp ..\..\src\performance\qte_ui_art.cpp ..\..\src\performance\healing_ui_art.cpp ..\..\src\performance\bleeding_ui_art.cpp ..\..\src\performance\phone_content_art.cpp ..\..\src\performance\phone_art.cpp ..\..\src\performance\phone_presentation.cpp %uiSoundResources% /link /DEF:..\..\src\diagnostics\d3d9.def /MACHINE:X86 /OUT:d3d9.dll /MAP:d3d9-component.map bcrypt.lib user32.lib gdi32.lib mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib propsys.lib shlwapi.lib dwrite.lib d2d1.lib windowscodecs.lib dsound.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /DUNICODE /D_UNICODE ..\..\src\testing\native_test_launcher.cpp "/Fe:AITD Runtime.exe" /link /SUBSYSTEM:WINDOWS user32.lib
if errorlevel 1 exit /b 1
call "%~dp0build-volume-input.cmd"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /Od ..\..\src\performance\contract_tests.cpp /Fe:gamma-contract-tests.exe /link user32.lib
if errorlevel 1 exit /b 1
gamma-contract-tests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /Od ..\..\src\performance\borderless_tests.cpp /Fe:borderless-tests.exe /link user32.lib
if errorlevel 1 exit /b 1
borderless-tests.exe
set "buildResult=%errorlevel%"
popd
popd
if not "%buildResult%"=="0" exit /b %buildResult%
call "%~dp0build-custom-launcher.cmd"
set "buildResult=%errorlevel%"
exit /b %buildResult%
