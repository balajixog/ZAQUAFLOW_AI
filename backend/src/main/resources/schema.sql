CREATE TABLE IF NOT EXISTS devices (
 id BIGSERIAL PRIMARY KEY, device_code VARCHAR(80) UNIQUE NOT NULL,
 status VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN', pump_state VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',
 operating_mode VARCHAR(20) NOT NULL DEFAULT 'AUTO', firmware_version VARCHAR(40),
 last_seen_at TIMESTAMPTZ, created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
INSERT INTO devices(device_code) VALUES ('ZAQ-ESP32-001') ON CONFLICT DO NOTHING;
CREATE TABLE IF NOT EXISTS telemetry (
 id BIGSERIAL PRIMARY KEY, message_id UUID UNIQUE NOT NULL,
 device_code VARCHAR(80) NOT NULL REFERENCES devices(device_code),
 sequence_no BIGINT, device_timestamp TIMESTAMPTZ, received_at TIMESTAMPTZ DEFAULT NOW(),
 soil_moisture_pct NUMERIC(6,2), flow_lpm NUMERIC(10,4), current_a NUMERIC(10,4),
 pump_state VARCHAR(20), firmware_fault_status VARCHAR(30), raw_payload JSONB NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_telemetry_device_time ON telemetry(device_code,received_at DESC);
CREATE TABLE IF NOT EXISTS pump_commands (
 id BIGSERIAL PRIMARY KEY, command_id UUID UNIQUE NOT NULL,
 device_code VARCHAR(80) NOT NULL REFERENCES devices(device_code),
 command VARCHAR(20) NOT NULL, command_source VARCHAR(20) NOT NULL,
 status VARCHAR(20) NOT NULL DEFAULT 'PENDING', requested_duration_seconds INTEGER,
 created_at TIMESTAMPTZ DEFAULT NOW(), acknowledged_at TIMESTAMPTZ, result_message TEXT
);

CREATE TABLE IF NOT EXISTS irrigation_cycles (
 id BIGSERIAL PRIMARY KEY,
 device_code VARCHAR(80) NOT NULL REFERENCES devices(device_code),
 started_at TIMESTAMPTZ NOT NULL,
 ended_at TIMESTAMPTZ,
 water_litres NUMERIC(12,4),
 energy_wh NUMERIC(12,4),
 start_soil_moisture_pct NUMERIC(6,2),
 end_soil_moisture_pct NUMERIC(6,2),
 average_flow_lpm NUMERIC(10,4),
 average_current_a NUMERIC(10,4),
 cycle_status VARCHAR(20) NOT NULL DEFAULT 'OPEN',
 created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_cycles_device_started ON irrigation_cycles(device_code,started_at DESC);

CREATE TABLE IF NOT EXISTS diis_results (
 id BIGSERIAL PRIMARY KEY,
 cycle_id BIGINT NOT NULL UNIQUE REFERENCES irrigation_cycles(id),
 eci_score NUMERIC(7,3),
 hci_score NUMERIC(7,3),
 srit_score NUMERIC(7,3),
 eui_score NUMERIC(7,3),
 diis_score NUMERIC(7,3),
 formula_version VARCHAR(40) NOT NULL,
 calculation_status VARCHAR(20) NOT NULL DEFAULT 'PENDING',
 explanation JSONB NOT NULL DEFAULT '{}'::jsonb,
 calculated_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS fault_events (
 id BIGSERIAL PRIMARY KEY,
 device_code VARCHAR(80) NOT NULL REFERENCES devices(device_code),
 fault_code VARCHAR(30) NOT NULL CHECK (fault_code IN ('NORMAL','DRY_RUN','BLOCKAGE','CAVITATION')),
 predicted_by VARCHAR(20) NOT NULL,
 confidence NUMERIC(7,5),
 flow_lpm NUMERIC(10,4),
 current_a NUMERIC(10,4),
 shap_values JSONB NOT NULL DEFAULT '{}'::jsonb,
 occurred_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
 cleared_at TIMESTAMPTZ,
 explanation TEXT
);
CREATE INDEX IF NOT EXISTS idx_fault_device_time ON fault_events(device_code,occurred_at DESC);

CREATE TABLE IF NOT EXISTS irrigation_schedules (
 id BIGSERIAL PRIMARY KEY,
 device_code VARCHAR(80) NOT NULL REFERENCES devices(device_code),
 name VARCHAR(120) NOT NULL,
 timezone VARCHAR(80) NOT NULL DEFAULT 'Asia/Kolkata',
 cron_expression VARCHAR(120) NOT NULL,
 duration_seconds INTEGER NOT NULL CHECK (duration_seconds BETWEEN 1 AND 600),
 moisture_threshold_pct NUMERIC(6,2),
 enabled BOOLEAN NOT NULL DEFAULT TRUE,
 next_run_at TIMESTAMPTZ,
 created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS schedule_executions (
 id BIGSERIAL PRIMARY KEY,
 schedule_id BIGINT NOT NULL REFERENCES irrigation_schedules(id),
 scheduled_for TIMESTAMPTZ NOT NULL,
 command_id UUID,
 execution_status VARCHAR(20) NOT NULL DEFAULT 'PENDING',
 result_message TEXT,
 created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
 UNIQUE(schedule_id, scheduled_for)
);

CREATE TABLE IF NOT EXISTS digital_twin_sessions (
 id UUID PRIMARY KEY,
 device_code VARCHAR(80) NOT NULL REFERENCES devices(device_code),
 scenario VARCHAR(30) NOT NULL CHECK (scenario IN ('NORMAL','DRY_RUN','BLOCKAGE','CAVITATION')),
 simulation_payload JSONB NOT NULL,
 created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS ai_tool_audit (
 id BIGSERIAL PRIMARY KEY,
 conversation_id UUID,
 tool_name VARCHAR(80) NOT NULL,
 arguments JSONB NOT NULL DEFAULT '{}'::jsonb,
 result JSONB NOT NULL DEFAULT '{}'::jsonb,
 created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
