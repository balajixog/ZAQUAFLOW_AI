# AI safety
- LLM interprets intent; it is not a safety controller.
- Pump ON requires explicit user confirmation and bounded duration.
- Spring Boot authenticates/authorizes; ESP32 independently validates.
- Local fault protection always overrides remote and AI requests.
- AI cannot clear fault locks or change safety thresholds.
- No direct LLM access to GPIO, unrestricted MQTT, SQL or shell.
- Never claim actuation succeeded without ESP32 ACK and actual-state report.
