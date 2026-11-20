@echo off
title hdrchain
mode con: cols=100 lines=28
color 0F
cd /d C:\Project\portfolio\hdrchain\build\Release
C:\Users\Bitcoin\AppData\Local\Programs\Python\Python312\python.exe C:\Project\portfolio\hdrchain\scripts\demo_gif.py
echo.
pause
