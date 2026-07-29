@echo off
del /q Objects\*.o Objects\*.d Objects\*.crf Objects\*.htm Objects\*.lnp Objects\*.dep 2>nul
del /q Listings\*.map Listings\*.lst 2>nul
echo Clean complete.
