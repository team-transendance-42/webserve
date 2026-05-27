#!/usr/bin/env bash
set -u

pass=0; fail=0

check() {
    local desc="$1" expected="$2"
    shift 2
    local code
    code=$(curl -s -o /dev/null -w "%{http_code}" "$@" 2>/dev/null || echo "000")
    if [[ "$code" == "$expected" ]]; then
        echo "PASS [$desc] → $code"
        ((pass++))
    else
        echo "FAIL [$desc] → got $code, want $expected"
        ((fail++))
    fi
}

echo "=== PORT 8081 (api server) ==="
check "GET /api/v1 → 200"         "200" "http://127.0.0.1:8081/api/v1"
check "GET /api/health → 200"     "200" "http://127.0.0.1:8081/api/health"
check "POST /api/v1 → 200"        "200" -X POST "http://127.0.0.1:8081/api/v1"
check "POST /api/health → 405"    "405" -X POST "http://127.0.0.1:8081/api/health"
check "GET /unknown → 404"        "404" "http://127.0.0.1:8081/unknown"
tmpfile=$(mktemp)
head -c 614400 /dev/zero | tr '\0' 'a' > "$tmpfile"
check "POST oversized body → 413" "413" -X POST \
    --data-binary "@$tmpfile" \
    "http://127.0.0.1:8081/api/v1"
rm -f "$tmpfile"

echo ""
echo "=== PORT 8082 (static server) ==="
check "GET /static → 200"         "200" "http://127.0.0.1:8082/static"
check "GET /downloads → 200"      "200" -L "http://127.0.0.1:8082/downloads"
check "POST /static → 405"        "405" -X POST "http://127.0.0.1:8082/static"
check "GET /unknown → 404"        "404" "http://127.0.0.1:8082/unknown"

echo ""
echo "Summary: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]] && exit 0 || exit 1
