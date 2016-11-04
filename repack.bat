set zip="7z.exe"
set rs1="file.7z"
set rs=%rs1%
del %rs1% /Q

Bake\Bake.exe
IF "%errorlevel%" NEQ "0" goto failed

cd baked
%zip% a ..\%rs% "*.*" -r
cd ..
IF "%errorlevel%" NEQ "0" goto failed

exit /b 0

:failed
echo Repack failure
pause 
exit /b %errorlevel%
