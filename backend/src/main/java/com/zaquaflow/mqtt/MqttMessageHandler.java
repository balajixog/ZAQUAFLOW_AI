package com.zaquaflow.mqtt;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.annotation.PostConstruct;
import org.eclipse.paho.client.mqttv3.*;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;

import java.nio.charset.StandardCharsets;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.UUID;

/** Ingests device telemetry and command acknowledgements. ESP32 remains the safety authority. */
@Component
public class MqttMessageHandler {
    private final MqttClient mqtt;
    private final JdbcTemplate db;
    private final ObjectMapper mapper;

    public MqttMessageHandler(MqttClient mqtt, JdbcTemplate db, ObjectMapper mapper) {
        this.mqtt = mqtt;
        this.db = db;
        this.mapper = mapper;
    }

    @PostConstruct
    public void subscribe() throws MqttException {
        mqtt.setCallback(new MqttCallback() {
            @Override public void connectionLost(Throwable cause) { /* Paho automatic reconnect is enabled. */ }
            @Override public void messageArrived(String topic, MqttMessage message) {
                try { handle(topic, new String(message.getPayload(), StandardCharsets.UTF_8)); }
                catch (Exception ex) { System.err.println("Rejected MQTT message on " + topic + ": " + ex.getMessage()); }
            }
            @Override public void deliveryComplete(IMqttDeliveryToken token) { }
        });
        mqtt.subscribe("zaquaflow/v1/devices/+/telemetry", 1);
        mqtt.subscribe("zaquaflow/v1/devices/+/status", 1);
        mqtt.subscribe("zaquaflow/v1/devices/+/ack", 1);
    }

    void handle(String topic, String payload) throws Exception {
        String[] parts = topic.split("/");
        if (parts.length != 5 || !"devices".equals(parts[2])) return;
        String device = parts[3];
        JsonNode root = mapper.readTree(payload);
        switch (parts[4]) {
            case "telemetry" -> saveTelemetry(device, root, payload);
            case "status" -> saveStatus(device, root);
            case "ack" -> saveAck(device, root);
            default -> { }
        }
    }

    private void saveTelemetry(String device, JsonNode n, String raw) throws Exception {
        UUID messageId = uuidOrGenerate(n.path("message_id").asText(null));
        Instant deviceTime = instantOrNow(n.path("timestamp").asText(null));
        db.update("INSERT INTO devices(device_code,status,pump_state,operating_mode,last_seen_at) VALUES (?, 'ONLINE', ?, COALESCE(?, 'AUTO'), NOW()) ON CONFLICT(device_code) DO UPDATE SET status='ONLINE', pump_state=COALESCE(EXCLUDED.pump_state,devices.pump_state), operating_mode=COALESCE(EXCLUDED.operating_mode,devices.operating_mode), last_seen_at=NOW()",
                device, textOrNull(n,"pump_state"), textOrNull(n,"operating_mode"));
        db.update("INSERT INTO telemetry(message_id,device_code,sequence_no,device_timestamp,soil_moisture_pct,flow_lpm,current_a,pump_state,firmware_fault_status,raw_payload) VALUES (?,?,?,?,?,?,?,?,?,?::jsonb) ON CONFLICT(message_id) DO NOTHING",
                messageId, device, longOrNull(n,"sequence_no"), Timestamp.from(deviceTime), decimalOrNull(n,"soil_moisture_pct"), decimalOrNull(n,"flow_lpm"), decimalOrNull(n,"current_a"), textOrNull(n,"pump_state"), textOrNull(n,"fault_status"), raw);
    }

    private void saveStatus(String device, JsonNode n) {
        db.update("INSERT INTO devices(device_code,status,pump_state,operating_mode,firmware_version,last_seen_at) VALUES (?, ?, ?, COALESCE(?, 'AUTO'), ?, NOW()) ON CONFLICT(device_code) DO UPDATE SET status=EXCLUDED.status,pump_state=EXCLUDED.pump_state,operating_mode=EXCLUDED.operating_mode,firmware_version=EXCLUDED.firmware_version,last_seen_at=NOW()",
                device, valueOr(n,"status","ONLINE"), valueOr(n,"pump_state","UNKNOWN"), textOrNull(n,"operating_mode"), textOrNull(n,"firmware_version"));
    }

    private void saveAck(String device, JsonNode n) {
        String commandId = n.path("command_id").asText("");
        if (commandId.isBlank()) return;
        String ack = valueOr(n,"status","REJECTED").toUpperCase();
        if (!java.util.Set.of("ACCEPTED","REJECTED","COMPLETED","FAILED","EXPIRED").contains(ack)) ack = "REJECTED";
        String result = n.path("message").asText("");
        db.update("UPDATE pump_commands SET status=?, acknowledged_at=NOW(), result_message=? WHERE command_id=?::uuid AND device_code=?",
                ack, result, commandId, device);
        if (n.hasNonNull("pump_state")) {
            db.update("UPDATE devices SET pump_state=?, status='ONLINE', last_seen_at=NOW() WHERE device_code=?",
                    n.path("pump_state").asText(), device);
        }
    }

    private static UUID uuidOrGenerate(String value) { try { return UUID.fromString(value); } catch (Exception e) { return UUID.randomUUID(); } }
    private static Instant instantOrNow(String value) { try { return Instant.parse(value); } catch (Exception e) { return Instant.now(); } }
    private static String textOrNull(JsonNode n,String key) { return n.hasNonNull(key) ? n.path(key).asText() : null; }
    private static String valueOr(JsonNode n,String key,String fallback) { String v=textOrNull(n,key); return v==null||v.isBlank()?fallback:v; }
    private static Long longOrNull(JsonNode n,String key) { return n.hasNonNull(key)&&n.path(key).canConvertToLong()?n.path(key).asLong():null; }
    private static Double decimalOrNull(JsonNode n,String key) { return n.hasNonNull(key)&&n.path(key).isNumber()?n.path(key).asDouble():null; }
}
