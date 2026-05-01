@echo off
setlocal
python "%~dp0export_mobilenet_sigmoid_onnx.py" %*
exit /b %ERRORLEVEL%
