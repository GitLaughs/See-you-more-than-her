@echo off
chcp 65001 >nul
echo ========================================
echo  Codex 会话导出工具
echo  正在导出到 output\conversationsw\ ...
echo ========================================
echo.

cd /d "%~dp0"
node "%~dp0export-codex-sessions.mjs"

echo.
if %errorlevel% equ 0 (
    echo 导出完成！
) else (
    echo 导出失败，请确保已安装 Node.js (v18+)
)
echo.
pause
