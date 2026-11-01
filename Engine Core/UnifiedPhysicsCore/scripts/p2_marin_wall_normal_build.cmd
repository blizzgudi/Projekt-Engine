@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
if "%~1"=="" goto audit
if /I not "%~1"=="--run" (echo Usage: p2_marin_wall_normal_build.cmd [--run] & exit /b 2)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 30
cd /d "%CORE%"
cmake -S . -B build -G Ninja > "%RUNS%\p2_marin_wall_normal_configure.txt" 2>&1
if errorlevel 1 exit /b 31
cmake --build build --config Release > "%RUNS%\p2_marin_wall_normal_build.txt" 2>&1
if errorlevel 1 exit /b 32
cl /nologo /std:c++17 /EHsc /O2 /MD /Iinclude /Fobuild\p2_marin_wall_normal_benchmark.obj /Febuild\p2_marin_wall_normal_benchmark.exe benchmarks\p2_marin_dambreak_heights.cpp > "%RUNS%\p2_marin_wall_normal_benchmark_build.txt" 2>&1
if errorlevel 1 exit /b 33
build\upc_p2_marin_wall_normal_tests.exe > "%RUNS%\p2_marin_wall_normal_native.txt" 2>&1
if errorlevel 1 exit /b 34
build\p2_marin_wall_normal_benchmark.exe 200 throughflow dense interp pcg normal_extrap > "%RUNS%\p2_marin_wall_normal_coarse200.txt" 2> "%RUNS%\p2_marin_wall_normal_stderr.txt"
if errorlevel 1 exit /b 35
ctest --test-dir build -C Release --output-on-failure > "%RUNS%\p2_marin_wall_normal_ctest.txt" 2>&1
if errorlevel 1 exit /b 36
:audit
cd /d "%CORE%"
python scripts\p2_marin_wall_normal_partial_acceptance.py
if errorlevel 1 exit /b 37
exit /b 0
