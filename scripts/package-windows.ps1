# =============================================================================
#  scripts/package-windows.ps1 —— Windows 构建 + Inno Setup 安装程序
#
#  前置：
#    · Visual Studio 2022（含「使用 C++ 的桌面开发」工作负载）
#    · CMake（3.21+，安装时勾选加入 PATH）
#    · Inno Setup 6（可选；未安装则只产出 zip）
#
#  用法（仓库根目录，PowerShell）：
#    powershell -ExecutionPolicy Bypass -File scripts\package-windows.ps1
#    powershell -ExecutionPolicy Bypass -File scripts\package-windows.ps1 -NoBuild
#
#  产物：
#    dist\Cjing-<版本>-windows-setup.exe   （Inno Setup 安装程序）
#    dist\Cjing-<版本>-windows-x64.zip     （回退：便携版）
# =============================================================================
param([switch]$NoBuild)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "build-windows"
$Dist = Join-Path $Root "dist"
$Stage = Join-Path $Root "packaging\windows\stage"

# 版本号：从 CMakeLists.txt 的 project(... VERSION x.y.z) 读取
$Version = "0.2.0"
$m = Select-String -Path (Join-Path $Root "CMakeLists.txt") -Pattern 'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' |
     Select-Object -First 1
if ($m) { $Version = $m.Matches[0].Groups[1].Value }

New-Item -ItemType Directory -Force -Path $Dist | Out-Null

if (-not $NoBuild) {
    Write-Host "  [1/3] CMake 配置（静态链接 SDL）"
    cmake -S $Root -B $Build `
        -DCJING_STATIC=ON -DCJING_BUILD_TESTS=OFF -DCJING_BUILD_EXAMPLES=OFF
    if ($LASTEXITCODE -ne 0) { throw "cmake configure 失败" }

    Write-Host "  [2/3] 编译 Release"
    cmake --build $Build --config Release --parallel 8
    if ($LASTEXITCODE -ne 0) { throw "编译失败" }
}

$Bin = Get-ChildItem -Path $Build -Filter "CjingApp.exe" -Recurse |
       Where-Object { $_.FullName -match "Release" } | Select-Object -First 1
if (-not $Bin) { $Bin = Get-ChildItem -Path $Build -Filter "CjingApp.exe" -Recurse | Select-Object -First 1 }
if (-not $Bin) { throw "找不到 CjingApp.exe" }
Write-Host "  可执行文件: $($Bin.FullName)"

Write-Host "  [3/3] 组装分发包"
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path (Join-Path $Stage "assets") | Out-Null
Copy-Item $Bin.FullName $Stage
Copy-Item (Join-Path $Root "assets\*") (Join-Path $Stage "assets") -Recurse -Force

@"
井字棋 Cjing $Version（Windows x64，静态链接 SDL3）

运行 CjingApp.exe 即可；assets 目录需与其同级。
本程序未做商业代码签名，SmartScreen 提示时选择「更多信息 → 仍要运行」。
"@ | Set-Content -Path (Join-Path $Stage "README.txt") -Encoding UTF8

$IsccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
)
$Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($Iscc) {
    Write-Host "  使用 Inno Setup: $Iscc"
    & $Iscc (Join-Path $Root "packaging\windows\Cjing.iss")
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup 编译失败" }
    Write-Host "  产物: $Dist\Cjing-$Version-windows-setup.exe"
} else {
    Write-Host "  未找到 Inno Setup，改为生成便携版 zip"
    $zip = Join-Path $Dist "Cjing-$Version-windows-x64.zip"
    Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $zip -Force
    Write-Host "  产物: $zip"
}
