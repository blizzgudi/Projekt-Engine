@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 20
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
"%CMAKE%" -S "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore" -B "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\build"
if errorlevel 1 exit /b 21
"%CMAKE%" --build "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\build" --config Release
if errorlevel 1 exit /b 22
"%CTEST%" --test-dir "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\build" -C Release --output-on-failure > "D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_periodic_mac_vof_energy_full_ctest_20261028.txt" 2>&1
if errorlevel 1 (
  type "D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_periodic_mac_vof_energy_full_ctest_20261028.txt"
  exit /b 23
)
findstr /C:"100%% tests passed" "D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_periodic_mac_vof_energy_full_ctest_20261028.txt"
