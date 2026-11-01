@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
cl /nologo /std:c++17 /EHsc /O2 /W4 /I"%CORE%\include" /Fe:"%CORE%\build\upc_p2_abc3d_analytic_tests.exe" "%CORE%\tests\p2_abc3d_analytic_tests.cpp" > "%RUNS%\p2_abc3d_compile.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_abc3d_compile.txt" & exit /b 21)
"%CORE%\build\upc_p2_abc3d_analytic_tests.exe" > "%RUNS%\p2_abc3d_native.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_abc3d_native.txt" & exit /b 22)
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%RUNS%\p2_abc3d_ctest.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_abc3d_ctest.txt" & exit /b 23)
type "%RUNS%\p2_abc3d_native.txt"
exit /b 0
