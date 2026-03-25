#!/usr/bin/env bash
set -u
set -o pipefail

BASE_URL="${1:-http://127.0.0.1:8080}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FILES_DIR="$SCRIPT_DIR/www/files"
PROTECTED_FILE="$SCRIPT_DIR/www/one/index.html"

pass=0
fail=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

ok() {
  printf "${GREEN}PASS${NC} %s\n" "$1"
  pass=$((pass + 1))
}

ko() {
  printf "${RED}FAIL${NC} %s\n" "$1"
  fail=$((fail + 1))
}

info() {
  printf "${YELLOW}INFO${NC} %s\n" "$1"
}

#$1 is first arg
http_code() {
  local method="$1"
  local path="$2"
  curl --path-as-is -sS -o /dev/null -w "%{http_code}" -X "$method" "$BASE_URL$path" 2>/dev/null || echo "000"
}

assert_http_in() {
  local method="$1"
  local path="$2"
  local expected_csv="$3"
  local note="$4"
  local code
  code="$(http_code "$method" "$path")"

  IFS=',' read -r -a expected_arr <<< "$expected_csv"
  local matched=0
  for e in "${expected_arr[@]}"; do
    if [[ "$code" == "$e" ]]; then
      matched=1
      break
    fi
  done

  if [[ "$matched" -eq 1 ]]; then
    ok "$method $path -> $code ($note)"
  else
    ko "$method $path -> got $code, expected one of [$expected_csv] ($note)"
  fi
}

assert_exists() {
  local p="$1"
  local note="$2"
  if [[ -e "$p" ]]; then
    ok "$note"
  else
    ko "$note (missing: $p)"
  fi
}

assert_not_exists() {
  local p="$1"
  local note="$2"
  if [[ ! -e "$p" ]]; then
    ok "$note"
  else
    ko "$note (still exists: $p)"
  fi
}

printf "DELETE tests against %s\n" "$BASE_URL"
printf "Project root: %s\n\n" "$SCRIPT_DIR"

if [[ ! -d "$FILES_DIR" ]]; then
  printf "${RED}ERROR${NC} missing directory: %s\n" "$FILES_DIR"
  exit 2
fi

if [[ ! -f "$PROTECTED_FILE" ]]; then
  printf "${RED}ERROR${NC} missing protected file: %s\n" "$PROTECTED_FILE"
  exit 2
fi

# Create 3 files and verify all can be deleted.
TEST_FILE_1="delete_me_1_$$.txt"
TEST_FILE_2="delete_me_2_$$.txt"
TEST_FILE_3="delete_me_3_$$.txt"

TEST_PATH_1="$FILES_DIR/$TEST_FILE_1"
TEST_PATH_2="$FILES_DIR/$TEST_FILE_2"
TEST_PATH_3="$FILES_DIR/$TEST_FILE_3"

printf "temporary delete target 1\n" > "$TEST_PATH_1"
printf "temporary delete target 2\n" > "$TEST_PATH_2"
printf "temporary delete target 3\n" > "$TEST_PATH_3"

assert_exists "$TEST_PATH_1" "setup: created deletable file 1"
assert_exists "$TEST_PATH_2" "setup: created deletable file 2"
assert_exists "$TEST_PATH_3" "setup: created deletable file 3"

# 1) Core correctness: allowed DELETE removes regular files.
assert_http_in "DELETE" "/files_auto/$TEST_FILE_1" "204" "delete existing file 1 should succeed"
assert_http_in "DELETE" "/files_auto/$TEST_FILE_2" "204" "delete existing file 2 should succeed"
assert_http_in "DELETE" "/files_auto/$TEST_FILE_3" "204" "delete existing file 3 should succeed"

assert_not_exists "$TEST_PATH_1" "deleted file 1 should be gone on filesystem"
assert_not_exists "$TEST_PATH_2" "deleted file 2 should be gone on filesystem"
assert_not_exists "$TEST_PATH_3" "deleted file 3 should be gone on filesystem"

# 2) Not found behavior.
assert_http_in "DELETE" "/files_auto/no_such_file_$$.txt" "404" "delete missing file"

# 3) Method constraints still enforced by location rules.
assert_http_in "DELETE" "/zombie_kittens" "405" "DELETE on GET-only location should be rejected"

# 4) Directory delete attempt must never succeed.
TEST_DIR="delete_dir_$$"
TEST_DIR_PATH="$FILES_DIR/$TEST_DIR"
mkdir -p "$TEST_DIR_PATH"
assert_http_in "DELETE" "/files_auto/$TEST_DIR" "400,403,404,409,500" "deleting a directory should not return success"
assert_exists "$TEST_DIR_PATH" "directory target must still exist"
rmdir "$TEST_DIR_PATH" 2>/dev/null || true

# 5) Path traversal attempts must not succeed and must not touch protected files.
assert_http_in "DELETE" "/files_auto/../one/index.html" "400,403,404" "raw traversal blocked"
assert_http_in "DELETE" "/files_auto/%2e%2e/one/index.html" "400,403,404" "encoded traversal blocked"
assert_http_in "DELETE" "/files_auto/..%2Fone%2Findex.html" "400,403,404" "slash-encoded traversal blocked"
assert_http_in "DELETE" "/files_auto//../one/index.html" "400,403,404" "double-slash traversal blocked"
assert_exists "$PROTECTED_FILE" "protected file must remain untouched after traversal probes"

# 6) Missing filename should not be treated as a valid delete target.
assert_http_in "DELETE" "/files_auto" "400,403,404" "missing filename is invalid"

printf "\nSummary: %d passed, %d failed\n" "$pass" "$fail"

if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
exit 0
