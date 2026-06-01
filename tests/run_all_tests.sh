#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Starting server ==="
./webserv &
SERVER_PID=$!
sleep 0.5

echo ""
echo "=== Default-server tests ==="
cd tests/execution
for f in test_static_content.py test_head.py test_methods.py \
          test_locations.py test_cgi.py test_keepalive.py \
          test_request_parsing.py test_multi_server.py; do
    echo "--- $f ---"
    python3 "$f"
done

cd ../..
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Self-contained tests ==="
cd tests/execution
for f in test_err_codes.py test_vhost.py test_signals.py; do
    echo "--- $f ---"
    python3 "$f"
done
