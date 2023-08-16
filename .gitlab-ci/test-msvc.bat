@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

:: FIXME: make warnings fatal
pip3 install --upgrade --user meson~=0.64 || goto :error
:: FIXME: we should not need fontconfig=disabled
meson setup --prefix="C:\gtk4" -Ddebug=false -Dmedia-gstreamer=disabled _build -Dcairo:fontconfig=disabled -Dpango:fontconfig=disabled || goto :error
meson compile -C _build || goto :error
meson install --destdir=%CI_PROJECT_DIR%\destdir -C _build || goto :error

goto :EOF
:error
exit /b 1
