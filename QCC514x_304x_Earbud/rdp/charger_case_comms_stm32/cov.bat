@echo off
if "%1" == "" (
    call ceedling project:CB clobber gcov:all utils:gcov
    rmdir /s /q build\CB\gcov
    rmdir /s /q build\CB\test     
) else if "%2" == "" (
    call ceedling project:CB clobber gcov:%1 utils:gcov
    rmdir /s /q build\CB\gcov
    rmdir /s /q build\CB\test    
) else (
    call ceedling project:%2 clobber gcov:%1 utils:gcov
    rmdir /s /q build\%2\gcov
    rmdir /s /q build\%2\test
)