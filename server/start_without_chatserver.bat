@echo off
title Chat Project Starter
echo 正在启动所有服务器...

:: --- 启动 GateServer ---
echo 启动 GateServer...
start "GateServer" /d "GateServer\x64\Debug" "GateServer.exe"

:: --- 启动 StatusServer ---
echo 启动 StatusServer...
start "StatusServer" /d "StatusServer\x64\Debug" "StatusServer.exe"

:: --- 启动 ResourceServer ---
echo 启动 ResourceServer...
start "ResourceServer" /d "ResourceServer\x64\Debug" "ResourceServer.exe"

:: --- 启动 ChatServer2 ---
echo 启动 ChatServer2...
start "ChatServer2" /d "ChatServer2\x64\Debug" "ChatServer.exe"

echo 所有服务器已尝试启动。
pause