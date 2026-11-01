@echo off
setlocal
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
cd /d "%CORE%"
"%CMAKE%" -S "%CORE%" -B "%CORE%\build" -DCMAKE_BUILD_TYPE=Release > "%RUNS%\p2_marin_wall_interp_compile.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_marin_wall_interp_compile.txt" & exit /b 21)
"%CMAKE%" --build "%CORE%\build" --config Release --parallel 4 >> "%RUNS%\p2_marin_wall_interp_compile.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_marin_wall_interp_compile.txt" & exit /b 22)
cl /nologo /std:c++17 /EHsc /O2 /MD /Iinclude /Fobuild\p2_marin_dambreak_heights.obj /Febuild\p2_marin_dambreak_heights.exe benchmarks\p2_marin_dambreak_heights.cpp >> "%RUNS%\p2_marin_wall_interp_compile.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_marin_wall_interp_compile.txt" & exit /b 23)
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%RUNS%\p2_marin_wall_interp_ctest.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_marin_wall_interp_ctest.txt" & exit /b 24)
type "%RUNS%\p2_marin_wall_interp_ctest.txt"
exit /b 0
