#include <PubSubClient.h>
#include <WiFi.h>
#include "DHTesp.h"
#include <ESP32Servo.h>
#include <string.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"

Servo myservo;

int LDR_val=0;
float val=0;
float angle=0.0;
int buzzer_delay=2;
int buzzer_frequency=800;
String state="1";
double Time1;
double Time2;
double Time3;
double remaining1;
double remaining2;
double remaining3;
int currentDay=2;
int prev_day;
int to_day;

const int DHT_PIN=15;
WiFiClient espClient;

PubSubClient mqttClient(espClient);
DHTesp dhtSensor;

char tempAr[6];
char humAr[6];
char ldr_val[6];
char rm_day[6];
RTC_DS1307 DS1307_RTC;

LiquidCrystal_I2C lcd(0x27,20,4); 
DateTime now;
DateTime now1;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!DS1307_RTC.begin()) {
    Serial.println("Couldn't find RTC");
    while(1);
  }

   DS1307_RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lcd.init();
  lcd.clear();         
  lcd.backlight();
  pinMode(13, OUTPUT);
  setupWiFi();
  setupMqtt();
  dhtSensor.setup(DHT_PIN,DHTesp::DHT22);
  myservo.attach(18);
  myservo.write(angle);

  // String(currentDay).toCharArray(rm_day_copy,6);

  now1= DS1307_RTC.now();

  int minutes=now1.minute()+30;
  int Hours= now1.hour()+5;
  if (minutes>=60){
    minutes=minutes-60;
    Hours=Hours+1;
  }
  if (Hours>=24){
    Hours=Hours-24;
  }
  
  int Day=now1.day();
  int Month=now1.month();
  if (0<=Hours && Hours<5){
    Day+=1;
  }else if(Hours==5 && minutes<30){
    Day+=1;
  }
  if (Month==2 && Day==29){
    Day=1;
  
  }else if((Month==4 || Month==6 || Month==9 || Month==11) && Day==31){
    Day=1;
  }else if(Day==32){
    Day=1;
  }
  prev_day=Day;

}

void loop() {

  now= DS1307_RTC.now();
  double unix1=now.unixtime()/3600;
  remaining1=Time1-unix1;
  remaining2=Time2-unix1;
  remaining3=Time3-unix1;

  int minutes=now.minute()+30;
  int Hours= now.hour()+5;
  if (minutes>=60){
    minutes=minutes-60;
    Hours=Hours+1;
  }
  if (Hours>=24){
    Hours=Hours-24;
  }
  String TIME=(String)Hours+":"+(String)minutes;
  lcd.setCursor(0,1);
  lcd.print(TIME);

  int Day=now.day();
  int Month=now.month();
  int Year=now.year();
  if (0<=Hours && Hours<5){
    Day+=1;
  }else if(Hours==5 && minutes<30){
    Day+=1;
  }

  if (Month==2 && Day==29){
    Day=1;
    Month=3;
  }else if((Month==4 || Month==6 || Month==9 || Month==11) && Day==31){
    Month+=1;
    Day=1;
  }else if(Day==32){
    Month+=1;
    Day=1;
  }

  if (Month==13){
    Year+=1;
  }
  String date= (String)Year+"/"+(String)Month+"/"+(String)Day;
  lcd.setCursor(0,0);
  lcd.print(date);
  lcd.setCursor(0,2);
  lcd.print("Next dose in (hours)");
  
  if (remaining1>0){
    lcd.setCursor(0,3); 
    lcd.print((String)remaining1);
  }
  else{
    lcd.setCursor(0,3); 
    lcd.print("---");
  }
  if (remaining2>0){
    lcd.setCursor(6,3); 
    lcd.print((String)remaining2);
  }else{
    lcd.setCursor(6,3); 
    lcd.print("---");
  }
  if (remaining3>0){
    lcd.setCursor(12,3);
    lcd.print((String)remaining3);
  }else{
    lcd.setCursor(12,3);
    lcd.print("---");
  }

  if(!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();
  LDR();
  updateTemperature();

  to_day=Day;
  if (currentDay!=0){
    if(to_day!=prev_day){
      currentDay=currentDay-1;
      prev_day=to_day;
        String(currentDay).toCharArray(rm_day,6);
        mqttClient.publish("reduce_day",rm_day);
    }
  }

  mqttClient.publish("Medi-Temp",tempAr);
  mqttClient.publish("Medi-Humidity",humAr);
  mqttClient.publish("LDR_OUTPUT",ldr_val);
  delay(1000);
  lcd.clear();
}

void setupWiFi(){
  WiFi.begin("Wokwi-GUEST","");
  while (WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.println(".");
  }
  Serial.println(".");
  Serial.println("Wifi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(receiveCallback1);
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.println("Attempting MQTT connection....");
    if(mqttClient.connect("ESP32-4654")){
      Serial.println("connected");
      mqttClient.subscribe("Buzzer-on-off");
      mqttClient.subscribe("Sch-Output");
      mqttClient.subscribe("Servo-control");
      mqttClient.subscribe("Buz-delay");
      mqttClient.subscribe("Buz-frequency");
      mqttClient.subscribe("Buz-state");
      mqttClient.subscribe("Time-input");
      mqttClient.subscribe("current_day");

    }
    else{
      Serial.print("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}
void updateTemperature(){
  TempAndHumidity data=dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempAr,6);
  String(data.humidity,2).toCharArray(humAr,6);

}
void receiveCallback1(char* topic,byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]");

  char payloadCharAr[length];
  for(int i=0;i<length;i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i]=(char)payload[i];
  }
  Serial.println();

  String C_DAY;
  if(strcmp(topic,"current_day")==0){
    for(int i=0;i<length;i++){
      C_DAY+=payloadCharAr[i];
    }
    currentDay=C_DAY.toInt();
  }
  if(strcmp(topic,"Sch-Output")==0){
    if(payloadCharAr[0]=='1'){
      if(state=="0"){
        tone(13, buzzer_frequency);
        delay(buzzer_delay*1000);
      }else{
        for(int i=1;i<(buzzer_delay+1);i++){
          tone(13, buzzer_frequency);
          delay(500);
          noTone(13);
          delay(500);
        }
      }
    }
    else{
      noTone(13);
    }
  }

   if(strcmp(topic,"Buzzer-on-off")==0){
    if(payloadCharAr[0]=='1'){
      if(state=="0"){
        tone(13, buzzer_frequency);
        delay(buzzer_delay*1000);
      }else{
        for(int i=1;i<(buzzer_delay+1);i++){
          tone(13, buzzer_frequency);
          delay(500);
          noTone(13);
          delay(500);
        }
      }
      
    }
    else{
      noTone(13);
    }
  }

  String Angle;
  if(strcmp(topic,"Servo-control")==0){
    for(int i=0;i<length;i++){
      Angle+=payloadCharAr[i];
    }
    angle=Angle.toFloat();
    myservo.write(angle);
  }
    String Delay;
  if(strcmp(topic,"Buz-delay")==0){
    for(int i=0;i<length;i++){
      Delay+=payloadCharAr[i];
    }
    buzzer_delay=Delay.toInt();
  }

  String Frequency;
  if(strcmp(topic,"Buz-frequency")==0){
    for(int i=0;i<length;i++){
      Frequency+=payloadCharAr[i];
    }
    buzzer_frequency=Frequency.toInt();
    
  }

 
  if(strcmp(topic,"Buz-state")==0){
    if(payloadCharAr[0]=='1'){
      state='1';
    }
    else{
      state='0';
    }
  }

  String time1;
  String time2;
  String time3;
  int r=0;

  if(strcmp(topic,"Time-input")==0){
    for(int i=0;i<length;i++){
      if (payloadCharAr[i]!='|'){
        if(r==0){
          time1+=payloadCharAr[i];
        }else if(r==1){
          time2+=payloadCharAr[i];
        }else if(r==2){
          time3+=payloadCharAr[i];
        }
      }else{
        r+=1;
      }
    }
   Time1=time1.toDouble();
   Time2=time2.toDouble();
   Time3=time3.toDouble();
  }

}

void LDR(){
  LDR_val=analogRead(34);
  val=-((float)LDR_val-32.0)/(4063.0-32.0)+1.0;
  String(val,3).toCharArray(ldr_val,6);
}


