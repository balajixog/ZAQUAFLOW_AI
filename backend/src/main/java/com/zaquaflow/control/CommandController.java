package com.zaquaflow.control;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;
import org.eclipse.paho.client.mqttv3.*;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.*;
@RestController @RequestMapping("/api/v1/control")
public class CommandController {
 private final JdbcTemplate db; private final MqttClient mqtt; private final String key,device;
 public CommandController(JdbcTemplate db,MqttClient mqtt,@Value("${zaquaflow.internal-api-key}")String key,@Value("${zaquaflow.device-id}")String device){this.db=db;this.mqtt=mqtt;this.key=key;this.device=device;}
 public record Request(String deviceId,String command,Integer durationSeconds){}
 @PostMapping("/commands") public Map<String,Object> send(@RequestHeader(value="X-Internal-Api-Key",required=false)String supplied,@RequestBody Request r)throws Exception{
  if(!Objects.equals(key,supplied))throw new ResponseStatusException(HttpStatus.UNAUTHORIZED);
  if(!device.equals(r.deviceId()))throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"Unknown device");
  if(!Set.of("PUMP_ON","PUMP_OFF").contains(r.command()))throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"Unsupported command");
  if("PUMP_ON".equals(r.command())&&(r.durationSeconds()==null||r.durationSeconds()<1||r.durationSeconds()>600))throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"PUMP_ON requires 1..600 seconds");
  UUID id=UUID.randomUUID();
  db.update("INSERT INTO pump_commands(command_id,device_code,command,command_source,requested_duration_seconds) VALUES (?,?,?,'AI_AGENT',?)",id,device,r.command(),r.durationSeconds());
  String payload="{\"schema_version\":1,\"command_id\":\""+id+"\",\"device_id\":\""+device+"\",\"command\":\""+r.command()+"\",\"duration_seconds\":"+(r.durationSeconds()==null?"null":r.durationSeconds())+",\"issued_at\":\""+Instant.now()+"\",\"expires_in_seconds\":15}";
  MqttMessage m=new MqttMessage(payload.getBytes(StandardCharsets.UTF_8));m.setQos(1);
  mqtt.publish("zaquaflow/v1/devices/"+device+"/commands",m);
  return Map.of("commandId",id.toString(),"status","PENDING","message","Published; awaiting ESP32 acknowledgement.");
 }
}
