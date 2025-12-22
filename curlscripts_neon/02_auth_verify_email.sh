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
  "${NEON_AUTH_URL}/email-otp/verify-email" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "Origin: https://example.com" \
  -d "{
    \"email\": \"${USER_EMAIL}\",
    \"otp\": \"${OTP}\"
  }"