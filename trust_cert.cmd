@echo off
echo Trusting Metis Antique Marketplace Plus TLS certificate...
certutil -delstore Root localhost
certutil -addstore Root "%~dp0cmake-build-release\server\certs\server.crt"
echo Done. Restart Edge and go to https://localhost:8481
pause
