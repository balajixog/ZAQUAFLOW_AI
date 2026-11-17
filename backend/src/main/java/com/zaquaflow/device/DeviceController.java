package com.zaquaflow.device;

import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.web.bind.annotation.*;
import java.util.*;

@RestController
@RequestMapping("/api/v1/devices")
public class DeviceController {
    private final JdbcTemplate db;
    public DeviceController(JdbcTemplate db) { this.db = db; }

    @GetMapping("/{id}/status")
    public Map<String,Object> status(@PathVariable String id) {
        var rows = db.queryForList("SELECT device_code,status,pump_state,operating_mode,firmware_version,last_seen_at FROM devices WHERE device_code=?", id);
        return rows.isEmpty() ? Map.of("device_code",id,"status","UNKNOWN","pump_state","UNKNOWN","operating_mode","UNKNOWN") : rows.get(0);
    }

    @GetMapping("/{id}/telemetry/latest")
    public Map<String,Object> latest(@PathVariable String id) {
        var rows = db.queryForList("SELECT message_id,sequence_no,soil_moisture_pct,flow_lpm,current_a,pump_state,firmware_fault_status,device_timestamp,received_at FROM telemetry WHERE device_code=? ORDER BY received_at DESC LIMIT 1", id);
        return rows.isEmpty() ? Map.of("available",false,"device_code",id) : withAvailable(rows.get(0));
    }

    @GetMapping("/{id}/telemetry")
    public List<Map<String,Object>> history(@PathVariable String id, @RequestParam(defaultValue="100") int limit) {
        int safeLimit = Math.max(1, Math.min(limit, 1000));
        return db.queryForList(
            "SELECT message_id,sequence_no,soil_moisture_pct,flow_lpm,current_a,pump_state,firmware_fault_status,device_timestamp,received_at FROM telemetry WHERE device_code=? ORDER BY received_at DESC LIMIT ?",
            id,
            safeLimit
        );    }

    private Map<String,Object> withAvailable(Map<String,Object> row) { Map<String,Object> result=new LinkedHashMap<>(row); result.put("available",true); return result; }
}
