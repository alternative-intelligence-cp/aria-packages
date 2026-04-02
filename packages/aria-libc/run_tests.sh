#!/usr/bin/env bash
set -e

ARIAC=/home/randy/Workspace/REPOS/aria/build/ariac
SHIM_DIR="$(cd shim && pwd)"
TEST_DIR=tests
OUT_DIR=/tmp/aria_libc_tests
mkdir -p "$OUT_DIR"

TOTAL_PASS=0
TOTAL_FAIL=0

run_test() {
    local name="$1"
    shift
    local libs=""
    for lib in "$@"; do
        libs="$libs -L $SHIM_DIR -l$lib"
    done
    
    echo ""
    echo "────────────────────────────────────────"
    echo "Compiling: $name"
    if ! $ARIAC "$TEST_DIR/test_${name}.aria" -o "$OUT_DIR/test_${name}" $libs 2>/dev/null; then
        echo "  COMPILE FAILED: $name"
        TOTAL_FAIL=$((TOTAL_FAIL + 1))
        return
    fi
    
    echo "Running: $name"
    if LD_LIBRARY_PATH="$SHIM_DIR" "$OUT_DIR/test_${name}" 2>&1; then
        TOTAL_PASS=$((TOTAL_PASS + 1))
    else
        echo "  RUNTIME FAILED: $name (exit=$?)"
        TOTAL_FAIL=$((TOTAL_FAIL + 1))
    fi
}

run_test string    aria_libc_string
run_test math      aria_libc_math
run_test io        aria_libc_io aria_libc_string aria_libc_posix
run_test mem       aria_libc_mem aria_libc_string
run_test time      aria_libc_time aria_libc_string
run_test posix_fs  aria_libc_posix aria_libc_fs aria_libc_io aria_libc_string
run_test process   aria_libc_process aria_libc_string
run_test regex     aria_libc_regex aria_libc_string
run_test net       aria_libc_net aria_libc_string

echo ""
echo "════════════════════════════════════════"
echo "Test suites passed: $TOTAL_PASS"
echo "Test suites failed: $TOTAL_FAIL"
echo "════════════════════════════════════════"

if [ "$TOTAL_FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
