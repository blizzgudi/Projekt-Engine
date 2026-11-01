@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 30
cd /d "%CORE%"
cl /nologo /std:c++17 /EHsc /O2 /MD /Iinclude ^
 /Fobuild\p2_marin_dambreak_heights.obj ^
 /Febuild\p2_marin_dambreak_heights.exe ^
 benchmarks\p2_marin_dambreak_heights.cpp > "%RUNS%\p2_marin_pcg_native_build.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_marin_pcg_native_build.txt" & exit /b 31)
type "%RUNS%\p2_marin_pcg_native_build.txt"
exit /b 0
