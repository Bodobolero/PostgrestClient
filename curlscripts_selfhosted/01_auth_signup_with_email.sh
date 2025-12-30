#!/usr/bin/env bash
set -euo pipefail

# Source .env from the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set -a
source "$SCRIPT_DIR/.env"
set +a

curl -i -X POST \
  "${POSTGREST_AUTH_URL}/rpc/create_user" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "Content-Profile: auth" \
  -d "{
    \"email\": \"${USER_EMAIL}\",
    \"password\": \"${USER_PASSWORD}\",
    \"api_key\": \"${AUTH_SIGNUP_KEY}\"
  }"