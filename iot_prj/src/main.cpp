

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <ESP8266mDNS.h>
#include "SimplePortal.h"

const char* mqtt_server = "syn.homs.win";
const char* mqtt_login = "iot";
const char* mqtt_password = "iotpasssword";
const uint16_t mqtt_port = 7999;

WiFiClient espClient;
PubSubClient client(espClient);
int value = 0;

#include <GyverMAX7219.h>
#include <RunningGFX.h>
MAX7219 < 10, 1, D8 > mtrx;   // одна матрица (1х1), пин CS на D5
RunningGFX run1(&mtrx);


#include <GyverDS18.h>
GyverDS18Single ds(2,false);

#include <GSON.h>
gson::Parser p;

#include <LittleFS.h>
#include <PairsFile.h>
struct element
{
  uint32_t id;
  uint8_t speed;
  float min,max;
};

element elements[30];
uint8_t elements_count=0;

PairsFile textFile(&LittleFS, "/text.dat", 500);
PairsFile dataFile(&LittleFS, "/data.dat", 500);









struct {
  byte count;
  uint32_t  crc32;
} rtcData;

uint32_t calculateCRC32(const uint8_t *data) {
  size_t length = sizeof(rtcData)-4;
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}



void readFromRTCMemory() {
 if (ESP.rtcUserMemoryRead(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
  } else {
    rtcData.count = 0;
    Serial.println("RTC ERROR");
  }

  if (rtcData.crc32 != calculateCRC32((uint8_t*) &rtcData)) {
    rtcData.count = 0;
    rtcData.crc32 = calculateCRC32((uint8_t*) &rtcData);
    ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData));
    Serial.println("RTC REBOOT");
  } else {
    Serial.println("RTC OKAY");
    if (rtcData.count>2 && rtcData.count<4) {
      rtcData.count++;
      rtcData.crc32 = calculateCRC32((uint8_t*) &rtcData);
      ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData));
    }else {
      if (rtcData.count>2)
        rtcData.count=0;
      else
        rtcData.count++;
      rtcData.crc32 = calculateCRC32((uint8_t*) &rtcData);
      ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData));
    }

        
  }
}













uint32_t timer_temp=0;
float now_temp=255.0;
uint8_t pos_m=0;
String now_text;
uint8_t now_speed=20;

void setup_wifi() {
  delay(10);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(portalCfg.SSID, portalCfg.pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (length==0) return;
  String topic_str=String(topic);
  String topic_str_tmp=topic_str;
  uint8_t id_len=String(ESP.getChipId()).length();
  
  topic_str.remove(0,id_len+1);

  Serial.print("Message arrived [");
  Serial.print(topic_str[0]);
  Serial.print("] ");
  char msg_type=topic_str[0];
  topic_str.remove(0,2);
  uint32_t data_id=topic_str.toInt();
  Serial.print("[");
  Serial.print(data_id);
  Serial.println("] ");

  p.parse((char*)payload,length);

for (uint16_t i = 0; i < p.length(); i++) {
    // if (p.type(i) == gson::Type::Object || p.type(i) == gson::Type::Array) continue; // пропустить контейнеры
    Serial.print(i);
    Serial.print(". [");
    Serial.print(p.readType(i));
    Serial.print("] ");
    Serial.print(p.key(i));
    Serial.print(":");
    Serial.print(p.value(i));
    Serial.print(" {");
    Serial.print(p.parent(i));
    Serial.println("}");
}

  
  if (msg_type=='m') {
    client.publish(topic_str_tmp.c_str(), "",true);
    int ttas=p.get("m");
    if (dataFile.get("m")!=ttas && ttas!=0) {
      dataFile.set("m",ttas);
      dataFile.update();
      ESP.restart();
    }
  }else  if (msg_type=='c') {
    textFile.clear();
    textFile.update();
    dataFile.clear();
    dataFile.update();
    ESP.restart();
  }else if (msg_type=='d') {
    String tmpTxt=p.get("t");
    if (tmpTxt.length()!=0) {
      
      textFile.set(topic_str,tmpTxt);

      bool tmp_bool=false;
      for (uint8_t i=0;i<elements_count;i++) {
        if(elements[i].id==data_id) {
          tmp_bool=true;
          elements[i].min=p.get("-");
          elements[i].max=p.get("+");
          elements[i].speed=p.get("s");
          break;
        }
      }

      if (tmp_bool==false) {
        Serial.println("Same to ");
        Serial.println(elements_count);
        elements[elements_count].id=data_id;
        elements[elements_count].min=p.get("-");
        elements[elements_count].max=p.get("+");
        elements[elements_count].speed=p.get("s");
        elements_count++;
      }

      dataFile["d"] = pairs::Value(&elements, sizeof(elements));
      dataFile["e"]=elements_count;

      for (int rrr=0;rrr<10;rrr++) {
            Serial.println(String(elements[rrr].id)+" "+String(elements[rrr].min)+" "+String(elements[rrr].max)+" "+String(textFile.get(String(elements[rrr].id))));
          }

    }else {
      Serial.println("CLear");
      for (uint8_t i=0;i<elements_count;i++) {
        if(elements[i].id==data_id) {
          textFile.remove(topic_str);
          for (uint8_t ii=i;ii<elements_count-1;ii++) {
            elements[ii].id=elements[ii+1].id;
            elements[ii].min=elements[ii+1].min;
            elements[ii].max=elements[ii+1].max;
            elements[ii].speed=elements[ii+1].speed;
          }
          elements_count--;
          if (pos_m>i) pos_m--;
          dataFile["e"]=elements_count;
          Serial.println("new count" + String(elements_count));
          dataFile["d"] = pairs::Value(&elements, sizeof(elements));

          for (int rrr=0;rrr<10;rrr++) {
            Serial.println(String(elements[rrr].id)+" "+String(elements[rrr].min)+" "+String(elements[rrr].max)+" "+String(textFile.get(String(elements[rrr].id))));
          }

          break;
        }
      }


    }
    client.publish(topic_str_tmp.c_str(), "",true);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP8266Client-"+String(ESP.getChipId());
    Serial.println(clientId);
    if (client.connect(clientId.c_str(),mqtt_login,mqtt_password)) {
      Serial.println("connected");
      String subscribe_url=String(ESP.getChipId())+"/d/#";
      String subscribe_url2=String(ESP.getChipId())+"/c/#";
      String subscribe_url3=String(ESP.getChipId())+"/m";
      client.subscribe(subscribe_url.c_str());
      client.subscribe(subscribe_url2.c_str());
      client.subscribe(subscribe_url3.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}






void setup() {
  Serial.begin(115200);
  Serial.println();

  readFromRTCMemory();

  mtrx.begin();   
  mtrx.setBright(3); 
  mtrx.setRotation(3);   
  
  

  LittleFS.begin();
  textFile.begin();
  dataFile.begin();
  Serial.println(dataFile);
  elements_count=dataFile.get("e");
  change_user=dataFile.get("u");
  dataFile.get("d").decodeB64(&elements, sizeof(elements));
  dataFile.get("l").decodeB64(&portalCfg, sizeof(portalCfg));
  uint8_t mc=dataFile.get("m");
  if (mc==0) {
    dataFile.set("m",1);
    mc=1;
  }
  String tmp=portalCfg.SSID;
  if (tmp.length()==0 || rtcData.count==4) {
    mtrx.dot(1, 1);    
    mtrx.update();
    portalRun();
  }

  if (portalStatus() == SP_SUBMIT) {
    dataFile["l"] = pairs::Value(&portalCfg, sizeof(portalCfg));
    if (change_user==true) {
      dataFile["u"]=1;
    }
    mtrx.clear();
  }

  
  
  ds.requestTemp();

  run1.setSpeed(15);
  run1.setText("M"); 
  run1.setWindow(0, mc*8-1, 0);
  run1.start();

  for (int rrr=0;rrr<10;rrr++) {
            Serial.println(String(elements[rrr].id)+" "+String(elements[rrr].min)+" "+String(elements[rrr].max)+" "+String(textFile.get(String(elements[rrr].id))));
          }


  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  
}

uint32_t timer_aaa=0;

void loop() {
  if (ds.ready()) {        
        if (ds.readTemp()) {  
          now_temp=ds.getTemp();  
        } else {
            Serial.println("error");
        }
    ds.requestTemp(); 
  }

  

  uint8_t matrix_state=run1.tick();

  if (millis()-timer_aaa>1000) {
    timer_aaa=millis();
    Serial.println("Matrix state: "+String(matrix_state)+ " Status: "+String( run1.getStatus()));
  }

  if (matrix_state==1) {
    if (pos_m!=0) {
      
      if (now_speed!=elements[pos_m-1].speed) {
        Serial.println("Change speed "+String(now_speed)+" -> "+String(elements[pos_m-1].speed));
        run1.setSpeed(elements[pos_m-1].speed);
        now_speed=elements[pos_m-1].speed;
      }
    }
  }else if(matrix_state==2 || run1.getStatus()==false){

    run1.stop();
    if (pos_m>=elements_count) 
      pos_m=0;
    for (uint8_t i=pos_m; i<elements_count;i++) {
      if (now_temp<=elements[i].max && now_temp>=elements[i].min) {
        now_text=String(textFile.get(String(elements[i].id)));
        run1.setText(now_text);
        run1.setSpeed(elements[i].speed);
        run1.start();
        now_speed=elements[i].speed;
        pos_m=i+1;
        Serial.println("find element "+String(i));
        Serial.println("all element" + String(elements_count));
        break;
      }
      if (i==elements_count-1) {
        pos_m=0;
      }
    }
  }



  if (client.connected()) {
    if (millis()-timer_temp>5000) {
      Serial.print("temp: ");
      Serial.println(now_temp);
      String topic_temp_url=String(ESP.getChipId())+"/t";
      client.publish(topic_temp_url.c_str(),String(now_temp).c_str());

      if (change_user==1) {
        String topic_ch_url=String(ESP.getChipId())+"/u";
        client.publish(topic_ch_url.c_str(),String(portalCfg.user).c_str(),true);
         dataFile["u"]=0;
      }

      timer_temp=millis();
    }
  }


  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  textFile.tick();
  dataFile.tick();
}
