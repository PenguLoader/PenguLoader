@echo off

mkdir obj\clang

for /r .\src %%i in (*.cc) do (
  clang++ -c -std=c++17 -O3 -DNDEBUG -DUNICODE -Wno-address-of-temporary ^
    -I ./src -I ./ -o ./obj/clang/%%~ni.obj "%%i"
)

clang++ -shared -o ../bin/core.dll ./obj/clang/*.obj ^
  -Wl,/DEF:res/module.def,/DELAYLOAD:libcef.dll libcef.lib -luser32 -ldelayimp

pause