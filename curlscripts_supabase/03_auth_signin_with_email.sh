#!/usr/bin/env bash
set -euo pipefail

# Source .env from the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set -a
source "$SCRIPT_DIR/.env"
set +a

# 1) Sign in and capture headers
RESP_HEADERS="$(mktemp)"
RESP_BODY="$(mktemp)"
trap 'rm -f "$RESP_HEADERS" "$RESP_BODY"' EXIT

curl --http1.1 -sS -D "$RESP_HEADERS" -o "$RESP_BODY"\
  -H "Content-Type: application/json" \
  -H "apikey: ${ANON_PUBLIC_KEY}" \
  -d "{
    \"email\": \"${USER_EMAIL}\",
    \"password\": \"${USER_PASSWORD}\"
  }" \
  "${SUPABASE_AUTH_URL}/token?grant_type=password"

# 2) Extract the JWT / access_token
JWT="$(
  awk 'BEGIN{p=0} /^\{/ {p=1} p{print}' "$RESP_BODY" | jq -r '.access_token'
)"
export JWT

if [[ -z "$JWT" ]]; then
  echo "Failed to extract JWT from set-auth-jwt header" >&2
  echo "Response headers were:" >&2
  cat "$RESP_HEADERS" >&2
  exit 1
fi
# Uncomment to see full response body
# cat "$RESP_BODY"

echo -e "The JWT is:\n\n$JWT"