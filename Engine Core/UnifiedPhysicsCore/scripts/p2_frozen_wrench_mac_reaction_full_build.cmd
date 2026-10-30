@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
set "CORE=D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
set "LOG=D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_frozen_wrench_mac_reaction_full_ctest_20261030.txt"
pushd "%CORE%\build"
cl.exe /nologo /EHsc /std:c++17 /O2 /MD /I"%CORE%\include" "%CORE%\tests\p2_frozen_wrench_mac_reaction3d_tests.cpp" /Fe:"%CORE%\build\upc_p2_frozen_wrench_mac_reaction_tests.exe"
if errorlevel 1 (popd & exit /b 21)
popd
"%CMAKE%" --build "%CORE%\build" --config Release --parallel 4
if errorlevel 1 exit /b 22
"%CTEST%" --test-dir "%CORE%\build" -C Release --parallel 4 --output-on-failure > "%LOG%" 2>&1
if errorlevel 1 (type "%LOG%" & exit /b 23)
findstr /C:"100%% tests passed, 0 tests failed out of 87" "%LOG%"
if errorlevel 1 exit /b 24
"%CORE%\build\upc_p2_frozen_wrench_mac_reaction_tests.exe"
exit /b %ERRORLEVEL%
