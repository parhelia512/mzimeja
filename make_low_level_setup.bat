set VERSION=0.9.9.2
set DEST_DIR=mzimeja-%VERSION%-low-level-setup
set OUTPUT=mzimeja-%VERSION%-low-level-setup.exe
if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"
if exist archive.7z del archive.7z
for %%F in (READMEJP.txt LICENSE.txt HISTORY.txt res\basic.dic res\name.dic res\kanji.dat res\radical.dat res\postal.dat build\Release\mzimeja.ime build\Release\ime_setup.exe build\Release\imepad.exe build\Release\dict_compile.exe build\Release\verinfo.exe) do copy %%F "%DEST_DIR%"
C:\7z2409-extra\7za.exe a archive.7z "%DEST_DIR%\*.*"
copy /b "C:\Program Files\7-Zip\7z.sfx" + archive.7z "%OUTPUT%"
del archive.7z
pause
