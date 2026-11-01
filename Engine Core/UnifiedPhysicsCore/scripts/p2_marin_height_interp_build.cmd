@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
if "%~1"=="" goto audit
if /I not "%~1"=="--run" (echo Usage: p2_marin_height_interp_build.cmd [--run] & exit /b 2)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 30
cd /d "%CORE%"
cmake -S . -B build -G Ninja > "%RUNS%\p2_marin_height_interp_configure.txt" 2>&1
if errorlevel 1 exit /b 31
cmake --build build --config Release --target upc_p2_marin_height_interp_tests > "%RUNS%\p2_marin_height_interp_build.txt" 2>&1
if errorlevel 1 exit /b 32
cl /nologo /std:c++17 /EHsc /O2 /MD /Iinclude /Fobuild\p2_marin_dambreak_heights.obj /Febuild\p2_marin_dambreak_heights.exe benchmarks\p2_marin_dambreak_heights.cpp > "%RUNS%\p2_marin_height_interp_benchmark_build.txt" 2>&1
if errorlevel 1 exit /b 33
build\p2_marin_dambreak_heights.exe 200 throughflow dense interp pcg height_interp > "%RUNS%\p2_marin_height_interp_coarse200.txt" 2> "%RUNS%\p2_marin_height_interp_native_stderr.txt"
if errorlevel 1 exit /b 34
build\upc_p2_marin_height_interp_tests.exe > "%RUNS%\p2_marin_height_interp_native.txt" 2>&1
if errorlevel 1 exit /b 35
ctest --test-dir build -C Release --output-on-failure > "%RUNS%\p2_marin_height_interp_ctest.txt" 2>&1
if errorlevel 1 exit /b 36
python tests\p2_marin_height_interp_regression.py > "%RUNS%\p2_marin_height_interp_regression.txt" 2>&1
if errorlevel 1 exit /b 37
:audit
cd /d "%CORE%"
python scripts\p2_marin_height_interp_partial_acceptance.py
if errorlevel 1 exit /b 38
exit /b 0
