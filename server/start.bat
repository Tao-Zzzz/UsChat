@echo off
title Chat Project Starter
echo 正在启动所有服务器...

:: --- 先启动 Redis（很重要，很多服务依赖它） ---
echo 启动 Redis...
cd /d "D:\Code\Project\Cpp_\llfcchat-Season_2\server\redis-windows"
start "Redis" .\redis-server.exe .\redis.windows.conf

:: 稍微等待 Redis 启动（可选，防止其他服务太早连不上）
timeout /t 3 /nobreak >nul


cd /d "D:\Code\Project\Cpp_\llfcchat-Season_2\server" 

:: --- 启动 GateServer ---
echo 启动 GateServer...
start "GateServer" /d "GateServer\x64\Debug" "GateServer.exe"

:: --- 启动 StatusServer ---
echo 启动 StatusServer...
start "StatusServer" /d "StatusServer\x64\Debug" "StatusServer.exe"

:: --- 启动 ResourceServer ---
echo 启动 ResourceServer...
start "ResourceServer" /d "ResourceServer\x64\Debug" "ResourceServer.exe"

:: --- 启动 ChatServer ---
echo 启动 ChatServer...
start "ChatServer" /d "ChatServer\x64\Debug" "ChatServer.exe"

:: --- 启动 ChatServer2 ---
echo 启动 ChatServer2...
start "ChatServer2" /d "ChatServer2\x64\Debug" "ChatServer.exe"

:: --- 启动 AIServer (Python/FastAPI) ---
echo 启动 AIServer...
:: 切换到你的 Python 项目代码所在的实际目录 (请修改下方路径)
cd /d "D:\Code\Project\Cpp_\llfcchat-Season_2\server\AiServer" 
start "AIServer" "E:/Library/Python313/python.exe" -m uvicorn main:app --reload --port 8070

echo 所有服务器已尝试启动。
pause