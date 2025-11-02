#!/usr/bin/env bash
set -euo pipefail

# build_one.sh — build (and optionally run) a single demo in this repo.
# Usage:
#   scripts/build_one.sh <target> [--debug] [--clean] [--run [args...]]
# Targets: aos_soa | false_sharing | sizes | spsc | pool_probe | vector_moves

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <target> [--debug] [--clean] [--run [args...]]" >&2
  exit 1
fi

TARGET="$1"; shift || true

# Map target -> folder + exe name
case "$TARGET" in
  aos_soa)        SRC_DIR="AoS_vs_SoA_Traversal";                EXE="aos_soa" ;;
  false_sharing)  SRC_DIR="False_Sharing_Demo";                   EXE="false_sharing" ;;
  sizes)          SRC_DIR="LP64_vs_LLP64";                        EXE="sizes" ;;
  spsc)           SRC_DIR="Lock_Free_Ring_Buffer";                EXE="spsc" ;;
  pool_probe)     SRC_DIR="Pool_Allocator_w_Placement_New";       EXE="pool_probe" ;;
  vector_moves)   SRC_DIR="Vector_Reallocation_&_noexcept_Move";  EXE="vector_moves" ;;
  *) echo "Unknown target: $TARGET" >&2; exit 2;;
esac

BUILD_TYPE="Release"
CLEAN=0
RUN_AFTER=0
RUN_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug) BUILD_TYPE="Debug"; shift ;;
    --clean) CLEAN=1; shift ;;
    --run)   RUN_AFTER=1; shift; RUN_ARGS=("$@"); break ;;
    *) echo "Unknown option: $1" >&2; exit 3 ;;
  esac
done

if [[ ! -d "$SRC_DIR" ]]; then
  echo "Expected folder '$SRC_DIR' not found. Run this from the repo root." >&2
  exit 4
fi

BUILD_DIR="$SRC_DIR/build-$BUILD_TYPE"

if [[ $CLEAN -eq 1 && -d "$BUILD_DIR" ]]; then
  echo "Cleaning $BUILD_DIR ..."
  rm -rf "$BUILD_DIR"
fi

echo "Configuring $TARGET ($BUILD_TYPE) ..."
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Building $TARGET ..."
cmake --build "$BUILD_DIR" --parallel

EXE_PATH="$BUILD_DIR/$EXE"
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
  EXE_PATH="$EXE_PATH.exe"
fi

# some generators place exe in a config subdir
if [[ ! -x "$EXE_PATH" && ! -f "$EXE_PATH" ]]; then
  if [[ -x "$BUILD_DIR/$BUILD_TYPE/$EXE" ]]; then
    EXE_PATH="$BUILD_DIR/$BUILD_TYPE/$EXE"
  elif [[ -x "$BUILD_DIR/$BUILD_TYPE/$EXE.exe" ]]; then
    EXE_PATH="$BUILD_DIR/$BUILD_TYPE/$EXE.exe"
  else
    echo "Built executable not found." >&2
    exit 5
  fi
fi

echo "Built: $EXE_PATH"
if [[ $RUN_AFTER -eq 1 ]]; then
  echo "Running: $EXE_PATH ${RUN_ARGS[*]}"
  "$EXE_PATH" "${RUN_ARGS[@]}"
fi
