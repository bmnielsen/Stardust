$ErrorActionPreference="Stop"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    New-Alias -Name cmake -Value "$Env:ProgramFiles\CMake\bin\cmake.exe"
}

New-Item -Name build -ItemType Directory
Push-Location build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_GENERATOR_PLATFORM=Win32 ..
cmake --build . --config RelWithDebInfo
Pop-Location
