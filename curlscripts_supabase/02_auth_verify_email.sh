#!/usr/bin/env bash
set -euo pipefail

# Source .env from the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set -a
source "$SCRIPT_DIR/.env"
set +a

# Prompt for OTP
read -p "Enter the OTP/code received in your email inbox: " OTP

curl -i -X POST \
  "${SUPABASE_AUTH_URL}/verify" \
  -H "Content-Type: application/json" \
  -H "apikey: ${ANON_PUBLIC_KEY}" \
  -H "Accept: application/json" \
  -d "{
    \"type\": \"email\",
    \"email\": \"${USER_EMAIL}\",
    \"token\": \"${OTP}\"
  }"