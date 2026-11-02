@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
set "TMP=%CORE%\build\p2_capillary_temp"
set "TEMP=%TMP%"
if not exist "%TMP%" mkdir "%TMP%"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
"%CMAKE%" -S "%CORE%" -B "%CORE%\build" -DCMAKE_BUILD_TYPE=Release > "%RUNS%\p2_two_phase_capillary_build.txt" 2>&1
if errorlevel 1 exit /b 21
"%CMAKE%" --build "%CORE%\build" --config Release --target upc_p2_two_phase_capillary_mac_vof_tests --parallel 4 >> "%RUNS%\p2_two_phase_capillary_build.txt" 2>&1
if errorlevel 1 exit /b 22
"%CORE%\build\upc_p2_two_phase_capillary_mac_vof_tests.exe" > "%RUNS%\p2_two_phase_capillary_native.txt" 2>&1
if errorlevel 1 exit /b 23
"%CTEST%" --test-dir "%CORE%\build" -C Release -R "physics_p2_two_phase_capillary_mac_vof_partial" --output-on-failure > "%RUNS%\p2_two_phase_capillary_target_ctest.txt" 2>&1
if errorlevel 1 exit /b 24
type "%RUNS%\p2_two_phase_capillary_native.txt"
type "%RUNS%\p2_two_phase_capillary_target_ctest.txt"
exit /b 0
