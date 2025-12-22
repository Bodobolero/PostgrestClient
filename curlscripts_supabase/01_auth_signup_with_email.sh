#!/usr/bin/env bash
set -euo pipefail

# Source .env from the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set -a
source "$SCRIPT_DIR/.env"
set +a

curl -i -X POST \
  "${SUPABASE_AUTH_URL}/signup" \
  -H "Content-Type: application/json" \
  -H "apikey: ${ANON_PUBLIC_KEY}" \
  -H "Accept: application/json" \
  -d "{
    \"email\": \"${USER_EMAIL}\",
    \"password\": \"${USER_PASSWORD}\"
  }"