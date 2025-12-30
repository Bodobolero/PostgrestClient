#!/usr/bin/env bash
set -euo pipefail

# Source .env from the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set -a
source "$SCRIPT_DIR/.env"
set +a

# 1) Sign in and capture response
RESP_BODY="$(mktemp)"
trap 'rm -f "$RESP_BODY"' EXIT

curl --http1.1 -sS -o "$RESP_BODY"\
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "Content-Profile: auth" \
  -d "{
    \"email\": \"${USER_EMAIL}\",
    \"password\": \"${USER_PASSWORD}\"
  }" \
  "${POSTGREST_AUTH_URL}/rpc/login"

#cat "$RESP_BODY"

# 2) Extract the JWT / access_token
JWT="$(
  awk 'BEGIN{p=0} /^\{/ {p=1} p{print}' "$RESP_BODY" | jq -r '.token'
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