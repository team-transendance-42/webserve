#!/usr/bin/env bash
set -u # cathes typos err if any undefined vars

BASE_URL="${1:-http://127.0.0.1:8080}"

# path|method|expected_status|note
TESTS=(
  "/|GET|200|root index"
  "/zombie_kittens|GET|200|specific page"
  "/game_start|GET|200|specific page"
  "/play|GET|301|redirect"
  "/secret|GET|403|denyAll config location had dany all (access not allowed)"
  "/not_allowed|GET|200|directory served when permissions allow"
  "/files|GET|403|directory, autoindex off, no configured index"
  "/files-auto|GET|301|directory autoindex on, bare path redirects to trailing slash"
  "/api/data_json|GET|200|json endpoint"
  "/does_not_exist|GET|404|missing location/resource"
  "/zombie_kittens|POST|405|method not allowed"
  "/api/data_json|DELETE|405|method not allowed"
  "/play|POST|405|redirect location disallows POST"
)

pass=0
fail=0

printf "Testing against: %s\n\n" "$BASE_URL"
printf "%-18s %-8s %-9s %-9s %-8s %s\n" "PATH" "METHOD" "EXPECTED" "ACTUAL" "RESULT" "NOTE"
printf "%-18s %-8s %-9s %-9s %-8s %s\n" "------------------" "--------" "---------" "---------" "--------" "-------------------------------"

for t in "${TESTS[@]}"; do
  IFS='|' read -r path method expected note <<< "$t" # internal field separator

  # no -L so redirects stay 301/302 instead of following to final 200
  code="$(curl -sS -X "$method" -o /dev/null -w "%{http_code}" "$BASE_URL$path" 2>/dev/null || echo "000")"

  if [[ "$code" == "$expected" ]]; then
    result="PASS"
    ((pass++))
  else
    result="FAIL"
    ((fail++))
  fi

  printf "%-18s %-8s %-9s %-9s %-8s %s\n" "$path" "$method" "$expected" "$code" "$result" "$note"
done

#printf "\nSummary: %d passed, %d failed\n" "$pass" "$fail"

# Oversized body test for 413 Payload Too Large.
# Uses 1.1MB body against a 1MB configured limit.
largeCode="$(head -c 1100000 /dev/zero | tr '\0' 'a' | \
  curl -sS -X POST --data-binary @- -o /dev/null -w "%{http_code}" \
    "$BASE_URL/zk_apply_form" 2>/dev/null || echo "000")"

if [[ "$largeCode" == "413" ]]; then
  printf "%-18s %-8s %-9s %-9s %-8s %s\n" "/zk_apply_form" "POST" "413" "$largeCode" "PASS" "oversized body over maxBody"
  ((pass++))
else
  printf "%-18s %-8s %-9s %-9s %-8s %s\n" "/zk_apply_form" "POST" "413" "$largeCode" "FAIL" "oversized body over maxBody"
  ((fail++))
fi

printf "\nFinal Summary: %d passed, %d failed\n" "$pass" "$fail"

if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
exit 0
