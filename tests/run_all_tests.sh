#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Starting server ==="
./webserv &
SERVER_PID=$!
sleep 0.5

echo ""
echo "=== Default-server tests ==="
for f in tests/execution/test_*.py; do
    echo "--- $f ---"
    python3 "$f"
done

kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Self-contained tests ==="
for f in test_err_codes.py test_vhost.py test_signals.py; do
    echo "--- $f ---"
    python3 "tests/execution/$f"
done
