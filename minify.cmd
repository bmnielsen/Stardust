@echo off

rmdir .git /s /q
rmdir 3rdparty\openbw /s /q
rmdir 3rdparty\zstd /s /q
rmdir 3rdparty\zstdstream /s /q
rmdir docs /s /q
rmdir test /s /q
rmdir windbg /s /q
rmdir out /s /q
del .gitignore
del CMakeLists.txt
del 3rdparty\BWAPILIB\CMakeLists.txt
del 3rdparty\BWEM\CMakeLists.txt
del 3rdparty\FAP\CMakeLists.txt
del src\CMakeLists.txt
del minify.cmd
