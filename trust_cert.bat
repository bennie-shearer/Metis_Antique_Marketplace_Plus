@echo off
echo Removing old Metis localhost certificate...
certutil -delstore Root localhost >nul 2>&1
echo Installing new certificate with SAN...
certutil -addstore Root "%~dp0certs\server.crt"
echo.
echo Done. Close ALL Edge/Chrome windows completely then reopen.
pause
