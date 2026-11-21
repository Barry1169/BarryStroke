@echo off
setlocal
rem Adjust PROJ to where this BarryStroke checkout lives on your machine.
set PROJ=D:\DavinciCoding\OpenfxProject\BarryStroke
set DEST=C:\Program Files\Common Files\OFX\Plugins\BarryStroke.ofx.bundle

echo [1/4] Building Release...
cmake --build "%PROJ%\build" --config Release
if errorlevel 1 goto fail

echo [2/4] Staging into the bundle...
copy /Y "%PROJ%\build\Release\BarryStroke.ofx" "%PROJ%\bundle\BarryStroke.ofx.bundle\Contents\Win64\" >nul
if errorlevel 1 goto fail

echo [3/4] Installing into Program Files...
xcopy /E /I /Y "%PROJ%\bundle\BarryStroke.ofx.bundle" "%DEST%"
if errorlevel 1 goto fail

echo [4/4] Clearing the old log...
if exist "%PROJ%\log.txt" del "%PROJ%\log.txt"

echo.
echo DONE: built, staged and installed.
exit /b 0

:fail
echo.
echo FAILED. Read the message above. Later steps were not run.
exit /b 1
