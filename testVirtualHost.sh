#!/usr/bin/env bash
set -u

BASE="http://127.0.0.1:8090"
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

# alpha virtual host → serves www/one (200)
check "Host: alpha → 200"            "200" -H "Host: alpha"       "$BASE/"
# beta virtual host → serves www/api (200, different root)
check "Host: beta → 200"             "200" -H "Host: beta"        "$BASE/"
# unknown name → default_server (alpha) → 200
check "Unknown Host → default 200"   "200" -H "Host: unknown"     "$BASE/"
# no Host header (HTTP/1.0) → default_server → 200
check "No Host (HTTP/1.0) → 200"     "200" --http1.0              "$BASE/"
# Host with port suffix → strip port, match name
check "Host: alpha:8090 → 200"       "200" -H "Host: alpha:8090"  "$BASE/"
# beta body should contain JSON (proves different root was selected)
body=$(curl -s -H "Host: beta" "$BASE/" 2>/dev/null)
if echo "$body" | grep -q '"'; then
    echo "PASS [beta body is JSON-like]"; ((pass++))
else
    echo "FAIL [beta body unexpected: $body]"; ((fail++))
fi

echo ""
echo "Virtual host tests: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]] && exit 0 || exit 1
