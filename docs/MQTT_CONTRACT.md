# ZAQUAFLOW MQTT contract v1

All messages are UTF-8 JSON. QoS 1 is required. One-shot commands must not be retained.

## Topics

- `zaquaflow/v1/devices/{id}/telemetry` ESP32 -> backend
- `zaquaflow/v1/devices/{id}/status` ESP32 -> backend
- `zaquaflow/v1/devices/{id}/events` ESP32 -> backend (reserved for event stream)
- `zaquaflow/v1/devices/{id}/commands` backend -> ESP32
- `zaquaflow/v1/devices/{id}/ack` ESP32 -> backend

## Telemetry payload

```json
{
  "message_id": "a UUID generated for this telemetry message",
  "sequence_no": 1234,
  "timestamp": "2026-11-17T03:00:00Z",
  "soil_moisture_pct": 42.5,
  "flow_lpm": 1.82,
  "current_a": 0.74,
  "pump_state": "ON",
  "operating_mode": "AUTO",
  "fault_status": "NORMAL"
}
```

`message_id` should be unique. If absent/invalid, the backend generates one, so replay de-duplication is not guaranteed for that message. Numeric readings must be calibrated values, not raw ADC counts. `timestamp` should be ISO-8601 UTC. The backend stores the original payload for traceability.

## Status payload

```json
{
  "status": "ONLINE",
  "pump_state": "OFF",
  "operating_mode": "AUTO",
  "firmware_version": "1.0.0"
}
```

## Command payload

```json
{
  "schema_version": 1,
  "command_id": "UUID",
  "device_id": "ZAQ-ESP32-001",
  "command": "PUMP_ON",
  "duration_seconds": 30,
  "issued_at": "2026-11-17T03:00:00Z",
  "expires_in_seconds": 15
}
```

`PUMP_OFF` may use `duration_seconds: null`. The firmware must validate device ID, command ID/idempotency, expiry and duration. It must reject PUMP_ON during a local fault lock and enforce the run timeout locally. PUMP_OFF is always permitted. Never let AI reset fault locks or bypass the local safety controller.

## Acknowledgement payload

```json
{
  "command_id": "same UUID as command",
  "status": "ACCEPTED",
  "pump_state": "ON",
  "message": "Command accepted; local runtime timer started",
  "timestamp": "2026-11-17T03:00:01Z"
}
```

Allowed acknowledgement statuses: `ACCEPTED`, `REJECTED`, `COMPLETED`, `FAILED`, `EXPIRED`. `ACCEPTED` means the firmware accepted the request; it is not proof that the requested irrigation cycle completed. Publish a later completion/failure acknowledgement or status/telemetry update.

## Backend ingestion

The Spring Boot MQTT subscriber listens to telemetry, status and ack topics. Telemetry is inserted into PostgreSQL, device last-seen/pump status is updated, and acknowledgements update the matching `pump_commands` row. Invalid messages are rejected from persistence and logged. The backend must never infer pump state from a command publish alone.

## Safety requirements

- ESP32 local control and safety continue when backend, broker, network or LLM is unavailable.
- Local rule engine is authoritative for dry-run, blockage and cavitation protection.
- AI and scheduler can request actions but cannot override local protection.
- Manual PUMP_ON duration is bounded to 1–600 seconds at API level and must also be bounded in firmware.
- Use QoS 1, no retained one-shot commands, and idempotent command IDs.
