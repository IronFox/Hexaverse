set zip="7z.exe"
set rs1="game.7z"
set rs2="server.7z"


del %rs1% /Q
del %rs2% /Q

Bake\Bake.exe mainBakeRecipe baked
IF "%errorlevel%" NEQ "0" goto failed

cd baked
%zip% a ..\%rs1% "*" -r
cd ..
IF "%errorlevel%" NEQ "0" goto failed


Bake\Bake.exe serverBakeRecipe baked
IF "%errorlevel%" NEQ "0" goto failed

cd baked
%zip% a ..\%rs2% "*" -r
cd ..
IF "%errorlevel%" NEQ "0" goto failed

exit /b 0

:failed
echo Repack failure
pause 
exit /b %errorlevel%
