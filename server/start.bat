@echo off
title Chat Project Starter
chcp 65001 >nul
echo 正在启动所有服务器...

:: ==========================
:: 启动 Redis
:: ==========================
echo.
echo [1/8] 启动 Redis...
cd /d "./redis-windows"
start "Redis Server" .\redis-server.exe .\redis.windows.conf
timeout /t 3 /nobreak >nul
cd ..

:: ==========================
:: 启动 C++ 服务
:: ==========================
echo.
echo [2/8] 启动 GateServer...
start "GateServer" /d "./GateServer/x64/Debug" GateServer.exe

echo [3/8] 启动 StatusServer...
start "StatusServer" /d "./StatusServer/x64/Debug" StatusServer.exe

echo [4/8] 启动 ResourceServer...
start "ResourceServer" /d "./ResourceServer/x64/Debug" ResourceServer.exe

echo [5/8] 启动 ChatServer...
start "ChatServer" /d "./ChatServer/x64/Debug" ChatServer.exe

echo [6/8] 启动 ChatServer2...
start "ChatServer2" /d "./ChatServer2/x64/Debug" ChatServer.exe

:: ==========================
:: 启动 Python AI 服务
:: ==========================
echo.
echo [7/8] 启动 AIServer...
start "AIServer" /d "./AiServer" "E:/Library/Python313/python.exe" app/main.py

:: ==========================
:: 新增 FaceServer
:: ==========================
echo [8/8] 启动 FaceServer...
start "FaceServer" /d "./FaceServer" "E:/Library/Python313/python.exe" main.py

echo.
echo ✅ 所有服务器启动完成！
echo.
pause