#!/bin/sh

set -eu

usage() {
	echo "usage: $0 LES_CHATD_BINARY" >&2
	exit 2
}

[ "$#" -eq 1 ] || usage

binary=$1
[ -x "$binary" ] || {
	echo "not an executable: $binary" >&2
	exit 1
}

test_dir=$(mktemp -d /tmp/les-chat-integration.XXXXXX)
node_a_pid=''
node_b_pid=''
failure_node_pid=''

cleanup() {
	[ -z "$node_a_pid" ] || kill "$node_a_pid" 2>/dev/null || true
	[ -z "$node_b_pid" ] || kill "$node_b_pid" 2>/dev/null || true
	[ -z "$failure_node_pid" ] || kill "$failure_node_pid" 2>/dev/null || true
	[ -z "$node_a_pid" ] || wait "$node_a_pid" 2>/dev/null || true
	[ -z "$node_b_pid" ] || wait "$node_b_pid" 2>/dev/null || true
	[ -z "$failure_node_pid" ] || wait "$failure_node_pid" 2>/dev/null || true
	rm -rf -- "$test_dir"
}

show_logs() {
	echo "--- node-a log ---" >&2
	sed -n '1,200p' "$test_dir/node-a.log" >&2 || true
	echo "--- node-b log ---" >&2
	sed -n '1,200p' "$test_dir/node-b.log" >&2 || true
}

wait_for_text() {
	url=$1
	text=$2
	attempts=${3:-40}

	while [ "$attempts" -gt 0 ]; do
		if curl -fsS "$url" 2>/dev/null | grep -Fq "$text"; then
			return 0
		fi
		attempts=$((attempts - 1))
		sleep 0.25
	done
	return 1
}

trap cleanup EXIT HUP INT TERM

"$binary" \
	--node-id integration-failure-node \
	--callsign FailureTest \
	--bind 127.0.0.1 \
	--port 18779 \
	--discovery-address 127.255.255.255 \
	--discovery-interface les-chat-missing0 \
	--discovery-port 19779 \
	--database "$test_dir/failure-node.db" \
	>"$test_dir/failure-node.log" 2>&1 &
failure_node_pid=$!

if ! wait_for_text http://127.0.0.1:18779/healthz '"status":"ok"'; then
	sed -n '1,200p' "$test_dir/failure-node.log" >&2 || true
	echo "discovery send failure stopped the HTTP service" >&2
	exit 1
fi
if ! grep -Fq \
	'Discovery interface is unavailable: les-chat-missing0' \
	"$test_dir/failure-node.log"; then
	sed -n '1,200p' "$test_dir/failure-node.log" >&2 || true
	echo "missing discovery interface was not diagnosed" >&2
	exit 1
fi

kill "$failure_node_pid" 2>/dev/null || true
wait "$failure_node_pid" 2>/dev/null || true
failure_node_pid=''

"$binary" \
	--node-id integration-node-a \
	--callsign Alpha \
	--bind 127.0.0.1 \
	--port 18777 \
	--discovery-address 127.255.255.255 \
	--discovery-interface lo \
	--discovery-port 19777 \
	--database "$test_dir/node-a.db" \
	>"$test_dir/node-a.log" 2>&1 &
node_a_pid=$!

"$binary" \
	--node-id integration-node-b \
	--callsign Bravo \
	--bind 127.0.0.1 \
	--port 18778 \
	--discovery-address 127.255.255.255 \
	--discovery-interface lo \
	--discovery-port 19777 \
	--database "$test_dir/node-b.db" \
	>"$test_dir/node-b.log" 2>&1 &
node_b_pid=$!

if ! wait_for_text http://127.0.0.1:18777/healthz '"status":"ok"' ||
   ! wait_for_text http://127.0.0.1:18778/healthz '"status":"ok"'; then
	show_logs
	echo "nodes did not become healthy" >&2
	exit 1
fi

if ! wait_for_text \
	http://127.0.0.1:18777/api/v1/peers 'integration-node-b' 60 ||
   ! wait_for_text \
	http://127.0.0.1:18778/api/v1/peers 'integration-node-a' 60; then
	show_logs
	echo "nodes did not discover each other" >&2
	exit 1
fi

node_a_status=$(curl -sS \
	-o "$test_dir/node-a-send.json" \
	-w '%{http_code}' \
	-H 'Content-Type: application/json' \
	-d '{"body":"message from node a"}' \
	http://127.0.0.1:18777/api/v1/messages)
if [ "$node_a_status" != 200 ] ||
   ! grep -Fq '"message"' "$test_dir/node-a-send.json"; then
	show_logs
	echo "node-a send response was not OpenWrt wget compatible" >&2
	exit 1
fi

if ! wait_for_text \
	http://127.0.0.1:18778/api/v1/messages 'message from node a'; then
	show_logs
	echo "node-a to node-b replication failed" >&2
	exit 1
fi

node_b_status=$(curl -sS \
	-o "$test_dir/node-b-send.json" \
	-w '%{http_code}' \
	-H 'Content-Type: application/json' \
	-d '{"body":"message from node b"}' \
	http://127.0.0.1:18778/api/v1/messages)
if [ "$node_b_status" != 200 ] ||
   ! grep -Fq '"message"' "$test_dir/node-b-send.json"; then
	show_logs
	echo "node-b send response was not OpenWrt wget compatible" >&2
	exit 1
fi

if ! wait_for_text \
	http://127.0.0.1:18777/api/v1/messages 'message from node b'; then
	show_logs
	echo "node-b to node-a replication failed" >&2
	exit 1
fi

echo "Two-node discovery and bidirectional replication passed"
