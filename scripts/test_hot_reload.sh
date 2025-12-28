#!/bin/bash
#
# Hot Reload API Test Script
# Tests the /config/set endpoint for runtime parameter updates
#
# Usage: ./test_hot_reload.sh [host] [port] [camera_id]
#   host      - Motion web control host (default: localhost)
#   port      - Motion web control port (default: 8080)
#   camera_id - Camera ID to test (default: 0 for all cameras)
#

set -e

HOST="${1:-localhost}"
PORT="${2:-8080}"
CAMERA="${3:-0}"
BASE_URL="http://${HOST}:${PORT}/${CAMERA}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS_COUNT=0
FAIL_COUNT=0

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# Test a hot-reloadable parameter
test_hot_reload() {
    local param="$1"
    local value="$2"
    local desc="$3"
    
    log_info "Testing: $desc ($param=$value)"
    
    response=$(curl -s "${BASE_URL}/config/set?${param}=${value}")
    
    # Check for success status
    if echo "$response" | grep -q '"status":"ok"'; then
        # Check for hot_reload:true
        if echo "$response" | grep -q '"hot_reload":true'; then
            log_pass "$desc - Parameter applied immediately"
            return 0
        else
            log_fail "$desc - Expected hot_reload:true"
            echo "    Response: $response"
            return 1
        fi
    else
        log_fail "$desc - Expected status:ok"
        echo "    Response: $response"
        return 1
    fi
}

# Test a non-hot-reloadable parameter (should fail gracefully)
test_no_hot_reload() {
    local param="$1"
    local value="$2"
    local desc="$3"
    
    log_info "Testing: $desc (should require restart)"
    
    response=$(curl -s "${BASE_URL}/config/set?${param}=${value}")
    
    # Should return error status
    if echo "$response" | grep -q '"status":"error"'; then
        if echo "$response" | grep -q 'requires daemon restart'; then
            log_pass "$desc - Correctly rejected (requires restart)"
            return 0
        fi
    fi
    
    log_fail "$desc - Should have been rejected"
    echo "    Response: $response"
    return 1
}

# Test unknown parameter
test_unknown_param() {
    log_info "Testing: Unknown parameter"
    
    response=$(curl -s "${BASE_URL}/config/set?nonexistent_param=123")
    
    if echo "$response" | grep -q '"status":"error"'; then
        if echo "$response" | grep -q 'Unknown parameter'; then
            log_pass "Unknown parameter correctly rejected"
            return 0
        fi
    fi
    
    log_fail "Unknown parameter should be rejected"
    echo "    Response: $response"
    return 1
}

# Test invalid query format
test_invalid_format() {
    log_info "Testing: Invalid query format"
    
    response=$(curl -s "${BASE_URL}/config/set")
    
    if echo "$response" | grep -q '"status":"error"'; then
        if echo "$response" | grep -q 'Invalid query format'; then
            log_pass "Invalid format correctly rejected"
            return 0
        fi
    fi
    
    log_fail "Invalid format should be rejected"
    echo "    Response: $response"
    return 1
}

# Test URL encoding
test_url_encoding() {
    log_info "Testing: URL encoded value"
    
    # Test with spaces encoded as %20
    response=$(curl -s "${BASE_URL}/config/set?text_left=Test%20Camera%201")
    
    if echo "$response" | grep -q '"status":"ok"'; then
        if echo "$response" | grep -q 'Test Camera 1'; then
            log_pass "URL encoding handled correctly"
            return 0
        fi
    fi
    
    log_fail "URL encoding test failed"
    echo "    Response: $response"
    return 1
}

echo "========================================"
echo "Hot Reload API Test Suite"
echo "========================================"
echo "Target: ${BASE_URL}"
echo ""

# Check if Motion is running
log_info "Checking Motion connectivity..."
if ! curl -s --connect-timeout 5 "${BASE_URL}/config.json" > /dev/null 2>&1; then
    echo -e "${RED}ERROR: Cannot connect to Motion at ${BASE_URL}${NC}"
    echo "Make sure Motion is running with webcontrol enabled."
    exit 1
fi
log_pass "Connected to Motion"
echo ""

echo "--- Hot-Reloadable Parameters ---"
test_hot_reload "threshold" "2000" "Detection threshold"
test_hot_reload "threshold_maximum" "0" "Maximum threshold"
test_hot_reload "noise_level" "32" "Noise level"
test_hot_reload "text_left" "TestCam" "Text overlay left"
test_hot_reload "event_gap" "30" "Event gap seconds"
test_hot_reload "snapshot_interval" "0" "Snapshot interval"
test_hot_reload "lightswitch_percent" "15" "Lightswitch percent"
echo ""

echo "--- Non-Hot-Reloadable Parameters ---"
test_no_hot_reload "width" "640" "Image width"
test_no_hot_reload "height" "480" "Image height"
test_no_hot_reload "webcontrol_port" "8080" "Webcontrol port"
test_no_hot_reload "framerate" "15" "Framerate"
echo ""

echo "--- Error Handling ---"
test_unknown_param
test_invalid_format
echo ""

echo "--- URL Encoding ---"
test_url_encoding
echo ""

echo "========================================"
echo "Test Results"
echo "========================================"
echo -e "Passed: ${GREEN}${PASS_COUNT}${NC}"
echo -e "Failed: ${RED}${FAIL_COUNT}${NC}"

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed.${NC}"
    exit 1
fi
