#!/usr/bin/env bash
set -euo pipefail

# This script can be executed normally:
#   ./insert-sensorvalue.sh
#
# It will source the sign-in script (in the same directory) to obtain $JWT.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source the previous sign-in script (it should export JWT)
source "$SCRIPT_DIR/03_auth_signin_with_email.sh"

if [[ -z "${JWT:-}" ]]; then
  echo "JWT was not set by sign-in script" >&2
  exit 1
fi

curl -k -i -X POST "${SUPABASE_DATA_API_URL%/}/sensorvalues" \
  -H "Content-Type: application/json" \
  -H "apikey: ${ANON_PUBLIC_KEY}" \
  -H "Authorization: Bearer ${JWT}" \
  -d '{
    "sensor_name": "temperature",
    "sensor_value": 21.5
  }'