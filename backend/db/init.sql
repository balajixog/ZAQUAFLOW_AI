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
