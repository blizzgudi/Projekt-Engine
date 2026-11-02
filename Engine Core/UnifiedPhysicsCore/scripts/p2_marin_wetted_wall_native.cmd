@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
"%CMAKE%" -S "%CORE%" -B "%CORE%\build" -DCMAKE_BUILD_TYPE=Release > "%RUNS%\p2_marin_wetted_build_20261102.log" 2>&1
if errorlevel 1 exit /b 21
"%CMAKE%" --build "%CORE%\build" --config Release --target upc_p2_marin_wetted_wall_traction_tests --parallel 4 >> "%RUNS%\p2_marin_wetted_build_20261102.log" 2>&1
if errorlevel 1 exit /b 22
"%CORE%\build\upc_p2_marin_wetted_wall_traction_tests.exe" > "%RUNS%\p2_marin_wetted_native_20261102.log" 2>&1
if errorlevel 1 exit /b 23
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%RUNS%\p2_marin_wetted_ctest_20261102.log" 2>&1
if errorlevel 1 exit /b 24
type "%RUNS%\p2_marin_wetted_native_20261102.log"
powershell -NoProfile -Command "Get-Content '%RUNS%\p2_marin_wetted_ctest_20261102.log' -Tail 4"
exit /b 0
