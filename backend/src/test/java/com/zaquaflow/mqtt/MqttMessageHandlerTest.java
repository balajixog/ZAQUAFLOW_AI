package com.zaquaflow.mqtt;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.junit.jupiter.api.Test;
import org.springframework.jdbc.core.JdbcTemplate;

import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;

class MqttMessageHandlerTest {
    @Test
    void telemetryUpdatesDeviceAndPersistsReading() throws Exception {
        JdbcTemplate db = mock(JdbcTemplate.class);
        MqttMessageHandler handler = new MqttMessageHandler(mock(MqttClient.class), db, new ObjectMapper());
        String json = """
          {"message_id":"d9b8c321-6e9d-4b2d-9f44-51d6c9f4e002","sequence_no":7,
           "timestamp":"2026-11-17T00:00:00Z","soil_moisture_pct":42.5,
           "flow_lpm":1.82,"current_a":0.74,"pump_state":"ON",
           "operating_mode":"AUTO","fault_status":"NORMAL"}
          """;
        handler.handle("zaquaflow/v1/devices/ZAQ-ESP32-001/telemetry", json);
        verify(db).update(contains("INSERT INTO devices"), eq("ZAQ-ESP32-001"), eq("ON"), eq("AUTO"));
        verify(db).update(contains("INSERT INTO telemetry"), any(), eq("ZAQ-ESP32-001"), eq(7L), any(), eq(42.5), eq(1.82), eq(0.74), eq("ON"), eq("NORMAL"), eq(json));
    }

    @Test
    void acknowledgementUpdatesOnlyMatchingDeviceCommand() throws Exception {
        JdbcTemplate db = mock(JdbcTemplate.class);
        MqttMessageHandler handler = new MqttMessageHandler(mock(MqttClient.class), db, new ObjectMapper());
        String json = "{\"command_id\":\"d9b8c321-6e9d-4b2d-9f44-51d6c9f4e002\",\"status\":\"ACCEPTED\",\"pump_state\":\"ON\",\"message\":\"accepted\"}";
        handler.handle("zaquaflow/v1/devices/ZAQ-ESP32-001/ack", json);
        verify(db).update(contains("UPDATE pump_commands"), eq("ACCEPTED"), eq("accepted"), eq("d9b8c321-6e9d-4b2d-9f44-51d6c9f4e002"), eq("ZAQ-ESP32-001"));
        verify(db).update(contains("UPDATE devices"), eq("ON"), eq("ZAQ-ESP32-001"));
    }
}
