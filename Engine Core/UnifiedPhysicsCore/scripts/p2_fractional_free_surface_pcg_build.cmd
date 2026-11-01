@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
if "%~1"=="" goto audit
if /I not "%~1"=="--run" (
  echo Usage: p2_fractional_free_surface_pcg_build.cmd [--run]
  exit /b 2
)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 30
cd /d "%CORE%"
cmake -S . -B build -G Ninja > "%RUNS%\p2_fractional_surface_configure.txt" 2>&1
if errorlevel 1 exit /b 31
cmake --build build --config Release > "%RUNS%\p2_fractional_surface_build.txt" 2>&1
if errorlevel 1 exit /b 32
build\upc_p2_fractional_free_surface_pcg_tests.exe > "%RUNS%\p2_fractional_surface_native.txt" 2>&1
if errorlevel 1 exit /b 33
ctest --test-dir build -C Release --output-on-failure > "%RUNS%\p2_fractional_surface_ctest.txt" 2>&1
if errorlevel 1 exit /b 34
python scripts\phase_gate.py > "%RUNS%\p2_fractional_surface_phase_gate.txt" 2>&1
if errorlevel 1 exit /b 35
:audit
cd /d "%CORE%"
python scripts\p2_fractional_free_surface_pcg_partial_acceptance.py
if errorlevel 1 exit /b 36
exit /b 0
