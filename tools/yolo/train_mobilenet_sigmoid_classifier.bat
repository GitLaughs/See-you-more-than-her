@echo off
setlocal
python "%~dp0train_mobilenet_sigmoid_classifier.py" %*
exit /b %ERRORLEVEL%
