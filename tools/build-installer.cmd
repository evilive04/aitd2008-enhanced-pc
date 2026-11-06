@echo off
setlocal
pushd "%~dp0.."
if not exist "src\launcher\assets\launcher.ico" (
 echo Missing launcher letter-A icon: src\launcher\assets\launcher.ico
 popd
 exit /b 1
)
if not exist "build\installer" mkdir "build\installer"
set "setupCompiler=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%setupCompiler%" set "setupCompiler=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%setupCompiler%" (
 echo The .NET Framework 4.x compiler was not found.
 popd
 exit /b 1
)
"%setupCompiler%" /nologo /target:winexe /platform:anycpu /optimize+ /warn:4 /warnaserror+ /codepage:65001 /out:"build\installer\AITD Enhanced Setup.exe" /win32manifest:src\launcher\launcher.manifest /win32icon:src\launcher\assets\launcher.ico /reference:System.dll /reference:System.Core.dll /reference:System.Drawing.dll /reference:System.Windows.Forms.dll /reference:System.Web.Extensions.dll src\installer\Setup.cs src\installer\Dependencies.cs src\installer\Uninstall.cs
set "setupResult=%errorlevel%"
popd
exit /b %setupResult%
