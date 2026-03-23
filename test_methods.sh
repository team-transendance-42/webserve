#!/bin/bash

# Test script for unauthorized HTTP methods (DELETE, POST)
# Tests locations that don't allow these methods

BASE_URL="http://localhost:8080"

echo "=========================================="
echo "Testing Unauthorized HTTP Methods"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

test_count=0
passed=0

# Function to test a method on a location
test_method() { # $1 = method, $2 = path, $3 = resp code
    local method=$1 # assing func's first arg
    local location=$2
    local expected_code=$3
    
    test_count=$((test_count + 1))
    echo -n "Test $test_count: $method $location ... "
    
    response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$location") # make http req in silent mode, _w .. add hte http status code at the end
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "$expected_code" ]; then
        echo -e "${GREEN}PASS${NC} (HTTP $http_code)"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (Expected $expected_code, got $http_code)"
    fi
}

echo "--- Testing DELETE on GET-only locations (expected 405) ---"
test_method "DELETE" "/" "405" # method not allowed 405
test_method "DELETE" "/zombie_kittens" "405"
test_method "DELETE" "/game_start" "405"
test_method "DELETE" "/play" "405"
test_method "DELETE" "/files" "405"
test_method "DELETE" "/files_auto" "405"
test_method "DELETE" "/api/data_json" "405"

echo ""
echo "--- Testing POST on GET-only locations (expected 405) ---"
test_method "POST" "/zombie_kittens" "405"
test_method "POST" "/game_start" "405"
test_method "POST" "/play" "405"
test_method "POST" "/files" "405"
test_method "POST" "/files_auto" "405"
test_method "POST" "/secret" "405"
test_method "POST" "/api/data_json" "405"

echo ""
echo "--- Testing allowed methods (expected 200 or 403 for denyAll) ---"
test_method "POST" "/" "200"
test_method "POST" "/zk_apply_form" "200"
test_method "GET" "/zk_apply_form" "200"

echo ""
echo "=========================================="
echo "Results: $passed/$test_count tests passed"
echo "=========================================="
