@echo off
rem Builds QuantumOfSolace.exe (launcher + installer) from qos_launcher.py and the project files.
rem Run from this folder (installer\) in any terminal with Python on PATH.
rem
rem   build_installer.bat [SDK folder]
rem
rem The SDK folder is a ReXGlue SDK install made by ..\tools\build_native.bat (our SDK patches and
rem Skullburglar's native Vulkan renderer with this project's patches, native-plugin\patches). It's
rem zipped into the exe, so the installer needs no SDK download. Default:
rem C:\rexglue-src\rexglue-sdk\out\install\win-amd64 if it exists. The Bourne and Eat Lead ports share
rem that folder and put their own renderer patches in: run 007's tools\build_native.bat last.
setlocal
cd /d "%~dp0"

set "SDK_FOLDER=%~1"
if "%SDK_FOLDER%"=="" if exist "C:\rexglue-src\rexglue-sdk\out\install\win-amd64\lib\cmake\rexglue" set "SDK_FOLDER=C:\rexglue-src\rexglue-sdk\out\install\win-amd64"
rem Name of the bundled SDK: bump the suffix whenever the SDK or the native renderer's patches change,
rem so existing installs replace their SDK on the next install.
set "SDK_BUNDLE=rexglue-sdk-0.10.1-qos2-win-amd64.zip"

if not exist "%SDK_FOLDER%\bin\rexgpu-native*.dll" if not exist "%SDK_FOLDER%\lib\rexgpu-native*.dll" (
  echo Note: no rexgpu-native DLL found in "%SDK_FOLDER%". Quantum of Solace needs the native renderer:
  echo build the SDK with ..\tools\build_native.bat first.
)

echo Collecting project files into payload\project ...
if exist payload rmdir /s /q payload
mkdir payload\project\src payload\project\generated
for %%F in (qos_manifest.toml default_functions.toml default_functions_scan.toml CMakeLists.txt CMakePresets.json) do copy /y "..\%%F" payload\project\ >nul || goto :missing
copy /y ..\src\*.* payload\project\src\ >nul || goto :missing
copy /y ..\generated\rexglue.cmake payload\project\generated\ >nul || goto :missing
rem Read by the native renderer from beside qos.exe (the game's build copies them there): the menu hint
rem pictures, the PlayStation button pictures and their texture list, and the shader refs.
for %%D in (hints prompts pipelines) do (
  mkdir payload\project\%%D
  xcopy /e /i /q /y ..\%%D payload\project\%%D >nul || goto :missing
)
mkdir payload\licenses
copy /y ..\licenses\*.txt payload\licenses\ >nul || goto :missing

if "%SDK_FOLDER%"=="" (
  echo No SDK folder: the installer would download the stock ReXGlue SDK, which has no native renderer.
  echo Build the SDK with ..\tools\build_native.bat first.
  exit /b 1
)
if not exist "%SDK_FOLDER%\lib\cmake\rexglue" (
  echo "%SDK_FOLDER%" doesn't look like a ReXGlue SDK install.
  exit /b 1
)
echo Packing the SDK from %SDK_FOLDER% ...
mkdir payload\sdk
python -c "import os,sys,zipfile; src=sys.argv[1]; z=zipfile.ZipFile(sys.argv[2],'w',zipfile.ZIP_DEFLATED); [z.write(os.path.join(d,f), os.path.relpath(os.path.join(d,f),src)) for d,_,fs in os.walk(src) for f in fs]; z.close()" "%SDK_FOLDER%" "payload\sdk\%SDK_BUNDLE%" || goto :fail

echo Installing PyInstaller (first time only) ...
python -m pip install --upgrade pyinstaller || goto :fail

echo Building the installer ...
python -m PyInstaller --noconfirm --onefile --windowed --name QuantumOfSolace --icon icon.ico --add-data "payload;payload" --add-data "icon.ico;." qos_launcher.py || goto :fail

echo.
echo Done: %~dp0dist\QuantumOfSolace.exe
exit /b 0

:missing
echo A project file is missing. Run this from the installer folder inside the project.
exit /b 1
:fail
echo Build failed, see the messages above.
exit /b 1
