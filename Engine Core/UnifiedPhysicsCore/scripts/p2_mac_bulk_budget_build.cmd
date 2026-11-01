@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
cl /nologo /std:c++17 /EHsc /O2 /W4 /I"%CORE%\include" /Fo:"%CORE%\build\upc_p2_mac_bulk_budget_tests.obj" /Fe:"%CORE%\build\upc_p2_mac_bulk_budget_tests.exe" "%CORE%\tests\p2_mac_bulk_budget3d_tests.cpp" > "%RUNS%\p2_mac_bulk_budget_compile_20261101.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_mac_bulk_budget_compile_20261101.txt" & exit /b 21)
"%CORE%\build\upc_p2_mac_bulk_budget_tests.exe" > "%RUNS%\p2_mac_bulk_budget_native_20261101.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_mac_bulk_budget_native_20261101.txt" & exit /b 22)
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%RUNS%\p2_mac_bulk_budget_ctest_20261101.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_mac_bulk_budget_ctest_20261101.txt" & exit /b 23)
type "%RUNS%\p2_mac_bulk_budget_native_20261101.txt"
exit /b 0
