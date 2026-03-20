#!/usr/bin/env bash
set -u

BASE_URL="${1:-http://127.0.0.1:8080}"

# path|expected_status|note
TESTS=(
  "/|200|root index"
  "/zombie_kittens|200|specific page"
  "/game_start|200|specific page"
  "/play|301|redirect"
  "/secret|403|deny_all location"
  "/notAllowed|403|filesystem permission denied"
  "/files|403|directory, autoindex off, no configured index"
  "/files_auto|200|directory autoindex on"
  "/api/data_json|200|json endpoint"
)

pass=0
fail=0

printf "Testing against: %s\n\n" "$BASE_URL"
printf "%-18s %-9s %-9s %-8s %s\n" "PATH" "EXPECTED" "ACTUAL" "RESULT" "NOTE"
printf "%-18s %-9s %-9s %-8s %s\n" "------------------" "---------" "---------" "--------" "-------------------------------"

for t in "${TESTS[@]}"; do
  IFS='|' read -r path expected note <<< "$t"

  # no -L so redirects stay 301/302 instead of following to final 200
  code="$(curl -sS -o /dev/null -w "%{http_code}" "$BASE_URL$path" 2>/dev/null || echo "000")"

  if [[ "$code" == "$expected" ]]; then
    result="PASS"
    ((pass++))
  else
    result="FAIL"
    ((fail++))
  fi

  printf "%-18s %-9s %-9s %-8s %s\n" "$path" "$expected" "$code" "$result" "$note"
done

printf "\nSummary: %d passed, %d failed\n" "$pass" "$fail"

if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
exit 0
