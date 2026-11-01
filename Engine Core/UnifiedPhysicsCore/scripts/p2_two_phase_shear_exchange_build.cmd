@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
cd /d "%CORE%"
"%CMAKE%" -S "%CORE%" -B "%CORE%\build" -DCMAKE_BUILD_TYPE=Release > "%RUNS%\p2_two_phase_shear_exchange_build.txt" 2>&1
if errorlevel 1 exit /b 21
"%CMAKE%" --build "%CORE%\build" --config Release --target upc_p2_two_phase_shear_exchange_tests --parallel 4 >> "%RUNS%\p2_two_phase_shear_exchange_build.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_two_phase_shear_exchange_build.txt" & exit /b 22)
"%CORE%\build\upc_p2_two_phase_shear_exchange_tests.exe" > "%RUNS%\p2_two_phase_shear_exchange_native.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_two_phase_shear_exchange_native.txt" & exit /b 23)
"%CMAKE%" --build "%CORE%\build" --config Release --parallel 4 >> "%RUNS%\p2_two_phase_shear_exchange_build.txt" 2>&1
if errorlevel 1 exit /b 24
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%RUNS%\p2_two_phase_shear_exchange_ctest.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_two_phase_shear_exchange_ctest.txt" & exit /b 25)
type "%RUNS%\p2_two_phase_shear_exchange_native.txt"
powershell -NoProfile -Command "Get-Content '%RUNS%\p2_two_phase_shear_exchange_ctest.txt' -Tail 5"
exit /b 0
