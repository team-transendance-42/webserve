#!/bin/bash

cd "$(dirname "$0")/.."

FAILED=0
SERVER_PID=""

cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null
}
trap cleanup EXIT

echo "=== Starting server ==="
./webserv &
SERVER_PID=$!
sleep 0.5

echo ""
echo "=== Default-server tests ==="

echo "--- tests/execution/taste_cookies.py ---"
python3 tests/execution/taste_cookies.py || FAILED=1

shopt -s extglob
for f in tests/execution/test_!(err_codes|vhost|signals).py; do
    echo "--- $f ---"
    python3 "$f" || FAILED=1
done

cleanup
SERVER_PID=""

echo ""
echo "=== Self-contained tests ==="
for f in test_err_codes.py test_vhost.py test_signals.py; do
    echo "--- $f ---"
    python3 "tests/execution/$f" || FAILED=1
done

exit $FAILED
