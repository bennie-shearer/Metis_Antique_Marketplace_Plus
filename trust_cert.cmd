@echo off
echo Metis Antique Marketplace Plus -- TLS Certificate Trust
echo.
echo This script must be run as Administrator.
echo.
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: Please right-click this file and choose "Run as administrator"
    pause
    exit /b 1
)

echo Removing any existing localhost certificates from Trusted Root store...
certutil -delstore Root "localhost" >nul 2>&1

echo Installing new certificate with Subject Alternative Name...
certutil -addstore Root "%~dp0certs\server.crt"

if %errorLevel% equ 0 (
    echo.
    echo SUCCESS - Certificate trusted.
    echo.
    echo Please close ALL Edge/Chrome windows completely, then reopen.
    echo Navigate to https://localhost:8481
    echo.
) else (
    echo.
    echo ERROR: Certificate import failed.
)
pause
