#!/usr/bin/env bash
set -euo pipefail

# Source .env from the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set -a
source "$SCRIPT_DIR/.env"
set +a

# 1) Sign in and capture headers
HEADERS="$(mktemp)"
trap 'rm -f "$HEADERS"' EXIT

curl --http1.1 -sS -D "$HEADERS" -o /dev/null \
  -H "Content-Type: application/json" \
  -d "{
    \"email\": \"${USER_EMAIL}\",
    \"password\": \"${USER_PASSWORD}\"
  }" \
  "${NEON_AUTH_URL}/sign-in/email"

# 2) Extract the session cookie from Set-Cookie (NOT the JSON token)
SESSION_COOKIE="$(awk -F': ' '
  tolower($1)=="set-cookie" && $2 ~ /^__Secure-neon-auth.session_token=/ {
    sub(/;.*/, "", $2); print $2
  }' "$HEADERS")"

if [[ -z "$SESSION_COOKIE" ]]; then
  echo "Failed to extract session cookie" >&2
  exit 1
fi

# 3) Call get-session, capture headers+body, and extract JWT from `set-auth-jwt` header
RESP_HEADERS="$(mktemp)"
RESP_BODY="$(mktemp)"
trap 'rm -f "$HEADERS" "$RESP_HEADERS" "$RESP_BODY"' EXIT

curl -sS -D "$RESP_HEADERS" -o "$RESP_BODY" \
  -H "Cookie: ${SESSION_COOKIE}" \
  "${NEON_AUTH_URL}/get-session"

JWT="$(awk -F': ' 'tolower($1)=="set-auth-jwt" { sub(/\r$/, "", $2); print $2 }' "$RESP_HEADERS")"
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