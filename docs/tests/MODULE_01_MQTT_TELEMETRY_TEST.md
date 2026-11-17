# Module 01 — MQTT telemetry and acknowledgement test

## Scope

This module connects ESP32 MQTT telemetry/status/ack messages to Spring Boot and PostgreSQL. It does not yet implement the DIIS formula, ML inference, scheduler, or digital twin hardware coupling.

## Preconditions

- Docker Desktop is running.
- `.env` exists locally and contains valid credentials. Do not share it.
- `DEVICE_ID=ZAQ-ESP32-001`.

## Build and start

```powershell
cd C:\Users\balaj\Downloads\ZAQUAFLOW_AI
docker compose up --build -d
docker compose ps
```

Check logs:

```powershell
docker compose logs --tail=100 backend
docker compose logs --tail=50 postgres
```

## Publish a simulated telemetry message

Use the Mosquitto container if its image includes the client utility:

```powershell
docker compose exec mosquitto sh -lc 'mosquitto_pub -h localhost -t zaquaflow/v1/devices/ZAQ-ESP32-001/telemetry -q 1 -m '\''{"message_id":"d9b8c321-6e9d-4b2d-9f44-51d6c9f4e002","sequence_no":1,"timestamp":"2026-11-17T00:00:00Z","soil_moisture_pct":42.5,"flow_lpm":1.82,"current_a":0.74,"pump_state":"ON","operating_mode":"AUTO","fault_status":"NORMAL"}'\''' 
```

If `mosquitto_pub` is not installed in the broker container, use an MQTT client on the host or your ESP32 publisher. Do not install or expose a public broker just for this test.

## Verify REST API

```powershell
Invoke-RestMethod http://localhost:8080/api/v1/devices/ZAQ-ESP32-001/status
Invoke-RestMethod http://localhost:8080/api/v1/devices/ZAQ-ESP32-001/telemetry/latest
Invoke-RestMethod 'http://localhost:8080/api/v1/devices/ZAQ-ESP32-001/telemetry?limit=10'
```

Expected: `available=true`, soil moisture `42.5`, flow `1.82`, current `0.74`, and the received pump state `ON`. The exact timestamps may differ.

## Acknowledgement test

Publish an ack with a real pending `command_id` to:

`zaquaflow/v1/devices/ZAQ-ESP32-001/ack`

Example body:

```json
{"command_id":"<existing-command-uuid>","status":"ACCEPTED","pump_state":"ON","message":"Test acknowledgement","timestamp":"2026-11-17T00:00:01Z"}
```

Verify the corresponding `pump_commands.status` and `acknowledged_at` in PostgreSQL. Do not test PUMP_ON against a connected physical pump until firmware expiry, fault-lock rejection, runtime timeout and PUMP_OFF behaviour have been verified.

## Pass criteria

- Backend starts without SQL or MQTT subscription errors.
- Telemetry message is persisted once for a unique `message_id`.
- Device last-seen and status update.
- Latest and history REST endpoints return the saved values.
- Acknowledgement updates only the matching command ID and device.
- Invalid/unrecognized acknowledgement statuses are stored as `REJECTED`.
- No simulated telemetry is represented as live hardware data in reports.
