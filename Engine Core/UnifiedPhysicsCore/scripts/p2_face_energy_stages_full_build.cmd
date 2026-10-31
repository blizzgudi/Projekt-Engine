@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
set "RUNS=D:\Projekt Engine\ENGINE TEST STUBE\runs"
"%CMAKE%" --build "%CORE%\build" --config Release --parallel 4 > "%RUNS%\p2_face_energy_stages_build_20261031.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_face_energy_stages_build_20261031.txt" & exit /b 21)
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%RUNS%\p2_face_energy_stages_ctest_20261031.txt" 2>&1
if errorlevel 1 (type "%RUNS%\p2_face_energy_stages_ctest_20261031.txt" & exit /b 22)
findstr /C:"100%% tests passed, 0 tests failed out of 93" "%RUNS%\p2_face_energy_stages_ctest_20261031.txt"
if errorlevel 1 exit /b 23
"%CORE%\build\upc_p2_face_energy_stages_tests.exe"
exit /b %ERRORLEVEL%
