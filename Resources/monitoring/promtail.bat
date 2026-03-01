@echo off
cd /d "C:\Users\joeun\Desktop\monitoring"
promtail-windows-amd64.exe --config.file=promtail-config.yaml
