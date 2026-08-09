#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v chromium >/dev/null 2>&1 || {
    echo "test_wasm_browser.sh: chromium is required" >&2
    exit 1
}

server_log=$(mktemp /tmp/siecs-wasm-http.XXXXXX)
server_pid=
cleanup() {
    if [ -n "$server_pid" ]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -f "$server_log"
}
trap cleanup EXIT

python3 -u -m http.server 0 --bind 127.0.0.1 --directory "$repo_root" >"$server_log" 2>&1 &
server_pid=$!
port=
for _ in $(seq 1 50); do
    port=$(sed -n 's/.* port \([0-9][0-9]*\).*/\1/p' "$server_log" | head -1)
    [ -n "$port" ] && break
    sleep 0.1
done

[ -n "$port" ] || {
    cat "$server_log" >&2
    exit 1
}

dom=$(chromium --headless --no-sandbox --disable-gpu --dump-dom \
    --virtual-time-budget=5000 "http://127.0.0.1:$port/test/wasm/index.html")
printf '%s\n' "$dom" | grep -q 'SIECS_WASM_OK'
