@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
cl /nologo /std:c++17 /EHsc /O2 /W4 /I"%CORE%\include" /Fe:"%CORE%\build\upc_p2_projected_plic_schedule_tests.exe" "%CORE%\tests\p2_projected_plic_schedule3d_tests.cpp" > "%RUNS%\p2_projected_plic_compile_20261031.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_projected_plic_compile_20261031.txt" & exit /b 21)
"%CORE%\build\upc_p2_projected_plic_schedule_tests.exe" > "%RUNS%\p2_projected_plic_native_20261031.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_projected_plic_native_20261031.txt" & exit /b 22)
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%RUNS%\p2_projected_plic_ctest_20261031.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_projected_plic_ctest_20261031.txt" & exit /b 23)
type "%RUNS%\p2_projected_plic_native_20261031.txt"
exit /b 0
