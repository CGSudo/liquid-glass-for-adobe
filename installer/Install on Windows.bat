@echo off
rem One-click install of Sudo Liquid Glass into every Photoshop and Illustrator on this PC.
rem Photoshop: Adobe's own plugin installer (Creative Cloud), as you. Illustrator: copies LiquidGlass.aip into
rem Illustrator's Plug-ins folder, which needs admin rights, so Windows asks once.
if "%~1"=="illustrator" goto copy_aip
if not exist "%~dp0SudoLiquidGlass.ccx" goto missing
if not exist "%~dp0LiquidGlass.aip" goto missing

echo Photoshop:
set "PS="
for /d %%A in ("%ProgramFiles%\Adobe\Adobe Photoshop *") do set "PS=1"
set "UPIA=%CommonProgramFiles%\Adobe\Adobe Desktop Common\RemoteComponents\UPI\UnifiedPluginInstallerAgent\UnifiedPluginInstallerAgent.exe"
if not defined PS (
  echo   Not found, skipped.
) else if not exist "%UPIA%" (
  echo   The Creative Cloud desktop app was not found. Install it, or double-click SudoLiquidGlass.ccx instead.
) else (
  rem Remove any older copy first: the installer silently keeps one with the same version.
  "%UPIA%" /remove "Sudo Liquid Glass" >nul 2>&1
  "%UPIA%" /install "%~dp0SudoLiquidGlass.ccx" >nul && echo   Installed. Restart Photoshop and open Plugins ^> Sudo Liquid Glass. || echo   Adobe's installer reported a problem.
)

echo.
echo Illustrator:
set "AI="
for /d %%A in ("%ProgramFiles%\Adobe\Adobe Illustrator *") do set "AI=1"
if not defined AI (
  echo   Not found, skipped.
  goto done
)
tasklist /fi "imagename eq Illustrator.exe" | "%SystemRoot%\System32\find.exe" /i "Illustrator.exe" >nul
if not errorlevel 1 (
  echo   Illustrator is open. Close it, then run this again.
  goto done
)
rem The copy runs in a second, elevated copy of this file; this window waits and then checks the result.
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -ArgumentList illustrator -Verb RunAs -WindowStyle Hidden -Wait"
for /d %%A in ("%ProgramFiles%\Adobe\Adobe Illustrator *") do (
  rem Byte compare, so an older copy left in place never counts as installed.
  fc /b "%~dp0LiquidGlass.aip" "%%A\Plug-ins\Liquid Glass\LiquidGlass.aip" >nul 2>&1 && (echo   Installed into %%A) || (echo   Not installed into %%A. Was the admin prompt declined?)
)
echo   Start Illustrator, select a shape and choose Effect ^> Stylize ^> Liquid Glass...
goto done

:copy_aip
for /d %%A in ("%ProgramFiles%\Adobe\Adobe Illustrator *") do (
  mkdir "%%A\Plug-ins\Liquid Glass" 2>nul
  copy /y "%~dp0LiquidGlass.aip" "%%A\Plug-ins\Liquid Glass\" >nul
)
exit /b

:missing
echo SudoLiquidGlass.ccx or LiquidGlass.aip was not found next to this file. Unzip the whole download first.

:done
echo.
pause
