@echo off
for /R %%f in (*.exe *.dll *.ime) do (
	echo "%%~ff" | findstr /I /C:"\CMakeFiles\" /C:"\.git\" /C:"\.vs\" >NUL
	if errorlevel 1 (
		call ..\do_sign.bat "%%~ff"
	)
)
