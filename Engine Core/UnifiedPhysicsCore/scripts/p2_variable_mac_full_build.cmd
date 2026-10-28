@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\build" --config Release
if errorlevel 1 exit /b 21
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\build" -C Release --output-on-failure > "D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_variable_mac_full_ctest_20261028.txt" 2>&1
if errorlevel 1 (
  type "D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_variable_mac_full_ctest_20261028.txt"
  exit /b 22
)
findstr /C:"100%% tests passed" "D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_variable_mac_full_ctest_20261028.txt"
