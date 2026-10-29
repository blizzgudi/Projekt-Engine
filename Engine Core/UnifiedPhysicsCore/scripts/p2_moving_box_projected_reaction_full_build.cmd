@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "LOG=D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_moving_box_projected_reaction_full_ctest_20261029.txt"
"%CMAKE%" -S "%CORE%" -B "%CORE%\build"
if errorlevel 1 exit /b 21
"%CMAKE%" --build "%CORE%\build" --config Release --parallel 4
if errorlevel 1 exit /b 22
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%LOG%" 2>&1
if errorlevel 1 (
  type "%LOG%"
  exit /b 23
)
findstr /C:"100%% tests passed, 0 tests failed out of 80" "%LOG%"
if errorlevel 1 exit /b 24
