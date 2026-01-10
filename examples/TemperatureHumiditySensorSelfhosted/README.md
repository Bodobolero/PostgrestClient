# Example with self-hosted postgres

This example uses a self-hosted postgrest with self-hosted auth and a different schema.
It also uses a higher precision AHT20 I2C sensor and a LCD display.


## Schema for temperature and humidity

```sql
-- =========================
-- Specific sensor table: temperature + humidity (either may be NULL)
-- Per-user isolation via PRIMARY KEY (user_id, sensor_name, measure_time)
-- =========================
CREATE TABLE IF NOT EXISTS public.temphum_values (
  measure_time  timestamp        NOT NULL DEFAULT now(),
  user_id       text             NOT NULL DEFAULT (auth.user_id()),
  sensor_name   text             NOT NULL,
  temperature   double precision,
  humidity      double precision,

  CONSTRAINT temphum_values_pkey
    PRIMARY KEY (user_id, sensor_name, measure_time),

  -- optional sanity checks
  CONSTRAINT temphum_humidity_range
    CHECK (humidity IS NULL OR (humidity >= 0 AND humidity <= 100)),

  -- ensure at least one measurement is present
  CONSTRAINT temphum_at_least_one_value
    CHECK (temperature IS NOT NULL OR humidity IS NOT NULL)
);

-- =========================
-- Grants
-- =========================
GRANT SELECT, INSERT, UPDATE, DELETE ON public.temphum_values TO authenticated;

-- =========================
-- RLS policies (use your auth.user_id() helper)
-- =========================
ALTER TABLE public.temphum_values ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS temphum_values_own ON public.temphum_values;
CREATE POLICY temphum_values_own ON public.temphum_values
FOR ALL
TO authenticated
USING (user_id = auth.user_id())
WITH CHECK (user_id = auth.user_id());

-- =========================
-- Anti-spoofing trigger: force user_id from JWT on INSERT/UPDATE
-- (reuses your existing public.set_user_id_from_jwt() function)
-- =========================
DROP TRIGGER IF EXISTS trg_temphum_values_set_user_id
ON public.temphum_values;

CREATE TRIGGER trg_temphum_values_set_user_id
BEFORE INSERT OR UPDATE ON public.temphum_values
FOR EACH ROW
EXECUTE FUNCTION public.set_user_id_from_jwt();

NOTIFY pgrst, 'reload schema';
```