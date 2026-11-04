@echo off
setlocal
pushd "%~dp0.."
if not exist build\launcher mkdir build\launcher
if not exist "build\performance\AITD Runtime.exe" (
 echo Missing native backend: build\performance\AITD Runtime.exe
 popd
 exit /b 1
)
set "framework=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319"
"%framework%\csc.exe" /nologo /target:winexe /platform:x86 /optimize+ /warn:4 /warnaserror+ /codepage:65001 /out:"build\launcher\Spustit hru.exe" /win32manifest:src\launcher\launcher.manifest /win32icon:src\launcher\assets\launcher.ico /reference:System.dll /reference:System.Core.dll /reference:"%framework%\System.Xaml.dll" /reference:"%framework%\WPF\WindowsBase.dll" /reference:"%framework%\WPF\PresentationCore.dll" /reference:"%framework%\WPF\PresentationFramework.dll" /resource:src\launcher\assets\background.jpg,Launcher.Background /resource:src\launcher\assets\logo.png,Launcher.Logo /resource:src\launcher\Styles.xaml,Launcher.Styles /resource:"build\performance\AITD Runtime.exe",Launcher.Runtime src\launcher\LauncherModel.cs src\launcher\LauncherUi.cs src\launcher\LauncherPages.cs src\launcher\LauncherTests.cs
set "result=%errorlevel%"
popd
exit /b %result%
