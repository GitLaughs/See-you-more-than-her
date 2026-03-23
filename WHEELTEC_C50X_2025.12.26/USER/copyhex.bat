@echo off
cd..
for /r %%f in (*.hex) do (
    copy "%%f" . >nul
)
