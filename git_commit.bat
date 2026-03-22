@echo off
set /p msg="Enter commit message: "
git add .
git commit -m "%msg%"
echo.
echo Commit successful!
pause