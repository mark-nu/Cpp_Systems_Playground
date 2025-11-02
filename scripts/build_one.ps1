param(
  [Parameter(Mandatory=$true)][ValidateSet("aos_soa","false_sharing","sizes","spsc","pool_probe","vector_moves")] [string]$target,
  [switch]$debug,
  [switch]$clean,
  [switch]$run,
  [Parameter(ValueFromRemainingArguments=$true)] [string[]]$args
)

# build_one.ps1 — build (and optionally run) a single demo in this repo.
# Usage:
#   scripts\build_one.ps1 -target spsc [-debug] [-clean] [-run -- 65536 20000000]

$ErrorActionPreference = "Stop"

switch ($target) {
  "aos_soa"        { $src="AoS_vs_SoA_Traversal"; $exe="aos_soa" }
  "false_sharing"  { $src="False_Sharing_Demo"; $exe="false_sharing" }
  "sizes"          { $src="LP64_vs_LLP64"; $exe="sizes" }
  "spsc"           { $src="Lock_Free_Ring_Buffer"; $exe="spsc" }
  "pool_probe"     { $src="Pool_Allocator_w_Placement_New"; $exe="pool_probe" }
  "vector_moves"   { $src="Vector_Reallocation_&_noexcept_Move"; $exe="vector_moves" }
}

if (!(Test-Path $src)) { Write-Error "Expected folder '$src' not found. Run this from the repo root." }

$buildType = if ($debug) { "Debug" } else { "Release" }
$buildDir = Join-Path $src ("build-" + $buildType)

if ($clean -and (Test-Path $buildDir)) {
  Write-Host "Cleaning $buildDir ..."
  Remove-Item -Recurse -Force $buildDir
}

Write-Host "Configuring $target ($buildType) ..."
cmake -S $src -B $buildDir -DCMAKE_BUILD_TYPE=$buildType

Write-Host "Building $target ..."
cmake --build $buildDir --parallel

$exePath = Join-Path $buildDir ($exe + ".exe")
$altExePath = Join-Path (Join-Path $buildDir $buildType) ($exe + ".exe")

if (!(Test-Path $exePath)) {
  if (Test-Path $altExePath) {
    $exePath = $altExePath
  } else {
    Write-Error "Built executable not found at $exePath or $altExePath"
  }
}

Write-Host "Built: $exePath"
if ($run) {
  Write-Host "Running: $exePath $args"
  & $exePath @args
}
