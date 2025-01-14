@echo off
setlocal EnableDelayedExpansion

if "%CRYSIS_INSTALL_DIR%"=="" (goto err_missing_env)

set SOURCE_DIR=%~dp0
set VRMOD_INSTALL_DIR=%CRYSIS_INSTALL_DIR%\Mods\VRMod
if not exist %VRMOD_INSTALL_DIR%\Bin64 (mkdir %VRMOD_INSTALL_DIR%\Bin64)

cd %SOURCE_DIR%\Game\Localized
7z a english.zip .\Languages
rename english.zip english.pak

xcopy /i /y %SOURCE_DIR%\*.xml %VRMOD_INSTALL_DIR%
xcopy /y %SOURCE_DIR%\Bin64\VRMod.dll %VRMOD_INSTALL_DIR%\Bin64
xcopy /y %SOURCE_DIR%\Bin64\CrysisVR.exe %CRYSIS_INSTALL_DIR%\Bin64
xcopy /i /s /y %SOURCE_DIR%\Game %VRMOD_INSTALL_DIR%\Game
xcopy /y %SOURCE_DIR%\system.vrsample.cfg %CRYSIS_INSTALL_DIR%
xcopy /y %SOURCE_DIR%\README.md %VRMOD_INSTALL_DIR%
xcopy /y %SOURCE_DIR%\LICENSE.txt %VRMOD_INSTALL_DIR%

rmdir /s /q %VRMOD_INSTALL_DIR%\Game\Localized\Languages
del /q %SOURCE_DIR%\Game\Localized\english.pak

goto eof

:err_missing_env
echo CRYSIS_INSTALL_DIR was not set, aborting
exit 1

:eof
