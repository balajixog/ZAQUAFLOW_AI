package com.zaquaflow.config;
import org.eclipse.paho.client.mqttv3.*;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.*;
@Configuration
public class MqttConfig {
 @Bean public MqttClient mqttClient(@Value("${zaquaflow.mqtt.host}") String host,
 @Value("${zaquaflow.mqtt.port}") int port,@Value("${zaquaflow.mqtt.username:}") String user,
 @Value("${zaquaflow.mqtt.password:}") String pass)throws MqttException{
  MqttClient c=new MqttClient("tcp://"+host+":"+port,"zaquaflow-backend-"+MqttClient.generateClientId());
  MqttConnectOptions o=new MqttConnectOptions();o.setAutomaticReconnect(true);o.setCleanSession(true);
  if(user!=null&&!user.isBlank()){o.setUserName(user);o.setPassword(pass.toCharArray());}
  c.connect(o);return c;
 }
}
