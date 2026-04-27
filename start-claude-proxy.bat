@echo off
chcp 65001 > nul
echo ==================================
echo   Claude Code Startup (Proxy Mode)
echo ==================================
echo.

REM Proxy configuration - Change port if needed
set HTTP_PROXY=http://127.0.0.1:7890
set HTTPS_PROXY=http://127.0.0.1:7890
set NO_PROXY=127.0.0.1,localhost,::1

REM Claude Code path
set CLAUDE_PATH=C:\Users\sun\.trae-cn\binaries\node\versions\24.11.1\claude.cmd

echo Proxy: %HTTP_PROXY%
echo.

REM Set proxy environment variables
set http_proxy=%HTTP_PROXY%
set https_proxy=%HTTPS_PROXY%
set no_proxy=%NO_PROXY%

REM Launch Claude Code
"%CLAUDE_PATH%"
pause