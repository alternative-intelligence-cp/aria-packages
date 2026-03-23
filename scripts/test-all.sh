#!/bin/bash
# Test all Aria packages
# Requires: ariac in PATH

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGES_DIR="$SCRIPT_DIR/../packages"

PASS=0
FAIL=0
SKIP=0
FAILED_PKGS=""

for pkg_dir in "$PACKAGES_DIR"/aria-*/; do
    pkg_name=$(basename "$pkg_dir")
    test_dir="$pkg_dir/tests"
    
    if [ ! -d "$test_dir" ]; then
        echo "SKIP  $pkg_name (no tests/)"
        SKIP=$((SKIP + 1))
        continue
    fi
    
    found_test=0
    for test_file in "$test_dir"/test_*.aria "$test_dir"/*_test.aria; do
        [ -f "$test_file" ] || continue
        found_test=1
        test_name=$(basename "$test_file")
        
        if ariac "$test_file" -o /tmp/aria_test_bin 2>/dev/null; then
            if /tmp/aria_test_bin 2>/dev/null; then
                echo "PASS  $pkg_name/$test_name"
                PASS=$((PASS + 1))
            else
                echo "FAIL  $pkg_name/$test_name (runtime error)"
                FAIL=$((FAIL + 1))
                FAILED_PKGS="$FAILED_PKGS $pkg_name"
            fi
        else
            echo "FAIL  $pkg_name/$test_name (compile error)"
            FAIL=$((FAIL + 1))
            FAILED_PKGS="$FAILED_PKGS $pkg_name"
        fi
        rm -f /tmp/aria_test_bin
    done
    
    if [ "$found_test" -eq 0 ]; then
        echo "SKIP  $pkg_name (no test files)"
        SKIP=$((SKIP + 1))
    fi
done

echo ""
echo "=== Results ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo "SKIP: $SKIP"

if [ -n "$FAILED_PKGS" ]; then
    echo "Failed:$FAILED_PKGS"
    exit 1
fi
