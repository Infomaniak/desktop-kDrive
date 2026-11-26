@echo off
if defined ProgramW6432 (
    start "" "%ProgramW6432%\kDrive\kDrive.exe" --synthesis
) else (
    start "" "%ProgramFiles%\kDrive\kDrive.exe" --synthesis
)
