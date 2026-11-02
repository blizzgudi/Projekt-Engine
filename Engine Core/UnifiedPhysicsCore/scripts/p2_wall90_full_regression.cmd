@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
set "TMP=%CORE%\build\p2_wall90_tmp"
set "TEMP=%TMP%"
if not exist "%TMP%" mkdir "%TMP%"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
cd /d "%CORE%"
"%CMAKE%" --build "%CORE%\build" --config Release --parallel 4 > "%RUNS%\p2_wall90_full_build_20261102.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_wall90_full_build_20261102.txt" & exit /b 21)
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 6 --output-on-failure > "%RUNS%\p2_wall90_full_ctest_20261102.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_wall90_full_ctest_20261102.txt" & exit /b 22)
echo Full CTest log: "%RUNS%\p2_wall90_full_ctest_20261102.txt"
exit /b 0
