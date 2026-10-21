@echo off
setlocal
pushd "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake 3.25+ is required. Install the Visual Studio C++ and CMake tools.
    popd
    exit /b 1
)
cmake --preset windows
if errorlevel 1 goto failed
cmake --build --preset release --parallel
if errorlevel 1 goto failed
ctest --preset release
if errorlevel 1 goto failed
echo Ready: %CD%\out\build\windows\Release\QIS-X.exe
popd
exit /b 0
:failed
echo Build or validation failed. See the errors above.
popd
exit /b 1
