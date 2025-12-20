#!/usr/bin/env bash
set -euo pipefail

# Source .env from the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set -a
source "$SCRIPT_DIR/.env"
set +a

curl -i -X POST \
  "${NEON_AUTH_URL}/sign-up/email" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "Origin: https://example.com" \
  -d "{
    \"email\": \"${USER_EMAIL}\",
    \"password\": \"${USER_PASSWORD}\",
    \"name\": \"${USER_NAME}\"
  }"