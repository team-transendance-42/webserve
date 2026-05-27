#!/usr/bin/env bash
# Run all webserv tests against a fresh server instance.
# Usage: ./tests/run.sh  (invokable from anywhere)

set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

CONFIG="tests/conf/default.conf"
READY_URL="http://127.0.0.1:8080/"
SERVER_LOG="webserv.log"
SERVER_PID=""

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# 1. Build if missing
if [ ! -x ./webserv ]; then
    echo "--- Building webserv ---"
    make
fi

# 2. Start server
echo "--- Starting webserv ($CONFIG) ---"
./webserv "$CONFIG" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# 3. Wait for readiness (10s budget)
for _ in $(seq 1 40); do
    if curl -sf -o /dev/null "$READY_URL"; then
        break
    fi
    sleep 0.25
done
if ! curl -sf -o /dev/null "$READY_URL"; then
    echo "webserv failed to become ready" >&2
    cat "$SERVER_LOG" >&2 || true
    exit 1
fi

# 4. Run each test, accumulate failures
fail=0
run_test() {
    local name="$1"
    shift
    echo ""
    echo "========================================"
    echo "  $name"
    echo "========================================"
    if "$@"; then
        echo "[OK]   $name"
    else
        echo "[FAIL] $name"
        fail=1
    fi
}

run_test "test_locations.sh"    ./tests/test_locations.sh
run_test "test_methods.sh"      ./tests/test_methods.sh
run_test "test_multi_server.sh" ./tests/test_multi_server.sh
run_test "test_err_codes.py"    python3 ./tests/test_err_codes.py
run_test "subject/tester"       ./tests/subject/tester

echo ""
echo "========================================"
if [ "$fail" -eq 0 ]; then
    echo "  All tests PASSED"
else
    echo "  Some tests FAILED"
fi
echo "========================================"

exit "$fail"
