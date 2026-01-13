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

curl -k -i -X POST "${POSTGREST_DATA_API_URL%/}/rpc/time_parts_berlin_timezone" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer ${JWT}" \
  -d '{}'


# This RPC all invokes the following function

# CREATE OR REPLACE FUNCTION public.time_parts_berlin_timezone()
# RETURNS TABLE (
#   year   int,
#   month  int,
#   day    int,
#   hour   int,
#   minute int,
#   second int
# )
# LANGUAGE sql
# STABLE
# AS $$
#   SELECT
#     EXTRACT(YEAR   FROM now() AT TIME ZONE 'Europe/Berlin')::int,
#     EXTRACT(MONTH  FROM now() AT TIME ZONE 'Europe/Berlin')::int,
#     EXTRACT(DAY    FROM now() AT TIME ZONE 'Europe/Berlin')::int,
#     EXTRACT(HOUR   FROM now() AT TIME ZONE 'Europe/Berlin')::int,
#     EXTRACT(MINUTE FROM now() AT TIME ZONE 'Europe/Berlin')::int,
#     EXTRACT(SECOND FROM now() AT TIME ZONE 'Europe/Berlin')::int;
# $$;

# GRANT EXECUTE ON FUNCTION public.time_parts_berlin_timezone() TO authenticated, anonymous;
# NOTIFY pgrst, 'reload schema';