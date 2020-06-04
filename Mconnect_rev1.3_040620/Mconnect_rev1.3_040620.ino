/*-----------------------------------------------22/05/20 By MMM TEAM------------------------------------------------------*/
#include <WiFi.h>
#include "time.h"
#include <Wire.h>
#include <DNSServer.h>
#include <Adafruit_MCP3008.h>
#include <PubSubClient.h> 
#include <HttpFOTA.h>
#include <HTTPClient.h>
#include <RTClib.h>
#include <Machine.h>
#include <string.h>
#include <stdio.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define LED2 13 //CS2#
#define RST 12


/*--------------Config Variable---------------------*/
const char* wifi_pwd;
const char* wifi_ssid;
const char* mqtt_server;
int mqttPort;
const char* mqttUser;
const char* mqttPassword;
const char* clientId;
int otatimeout; // OTA Timeout limit 30 sec
const char* sendtopic; // Machine send data Topic
const char* gtopic; //OTA Group Topic 
const char* ctopic; //OTA Sub Companny Topic
const char* stopic; //OTA Self Machine Topic
const char* ackota; //OTA Acknowledge use for Machine confirm received OTA
const char* getconf; // //Topic of this machine subscribe (or listen) for receive command from web socket command getcf(get config)
const char* sendconf; // Topic for Machine send config back to(publish to) web server (use web socket)
const char* dbreply;//Topic for check db active  Server Reply OK if Insert data already  ADD BY RJK 
/*--------------Config Variable---------------------*/


String eachline;// String  Object receiving each line in file conf.txt

char* certssl; // SSL Certification for download firmware OTA Load from certi.txt
String  Certs = "";// String Object for concatination certification from file certi.txt


/* String Object array for keep config each line in file conf.txt in SD Card */
String Line[16];

File iotfmx; //create object root from Class File use SD Card use write new config to SD Card
File root; //create object root from Class File use SD Card read from SD Card and assign to config variable

/*----------TIME NTP Server-------------*/
const char* ntpServer = "time.uni.net.th";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;

String datareceivedmqtt; // receive OK from DBserv

  
WiFiClient FMXClient;
PubSubClient client(FMXClient);


Machine mac;
RTC_DS3231 RTC;
Adafruit_MCP3008 adc;

// Variables to save date and time
//String formattedDate;
//#define SENDTIME 1000 //Send Data time interval for Testsend


#define LEDTIME 500 //Send Data time interval for Testsend
#define Qos  1 //Quality of Service
uint64_t chipid;  //Declaration for storage ChipID
unsigned long tnow = 0; //Init time for plus current time
unsigned long  startota = 0; //Initial time for counter 
int ota_t; //time difference
int prog; //percent download progress
unsigned long tled = 0;//Time start led blink use in LED2Status


/*-----------------------Machine-------------------------*/
#define IO_1 mac.READ_DATASW(sw1)
#define IO_2 mac.READ_DATASW(sw2)
#define IO_3 mac.READ_DATASW(sw3)
#define IO_4 mac.READ_DATASW(sw4)
#define IO_5 mac.READ_DATASW(sw5)
#define IO_6 mac.READ_DATASW(sw6)
#define IO_7 mac.READ_DATASW(sw7)
#define IO_8 mac.READ_DATASW(sw8)
#define writeaddr_eeprom1 32001
#define writeaddr_eeprom2 32002
#define FILE_COUNT_INHISTORYSD 31250 //1GB:1,000,000KB
#define addrsize 128

#define time_limitwifi  5
char DATA_PACKHEADHIS[16];
char DATA_PACKHEAD[21];
char DATA_PACKDATE[6];
char DATA_PACKIO[2];
char Chipid_buf[12];
char filnamechar[12];
char buf_date[12];
char buf_io[4];
char v_fw[4];

unsigned char DATA_PACKPWM1[4];
unsigned char DATA_PACKPWM2[4];
unsigned char DATA_PACKRELAY[1];
unsigned char DATA_PACKPWM3[4];
unsigned char DATA_PACKPWM4[4];
unsigned char DATA_PACKAD1[4];
unsigned char DATA_PACKAD2[4];
unsigned char DATA_PACKAD3[4];
unsigned char DATA_PACKGPS[6];
unsigned char DATA_PACKM1[8];
unsigned char DATA_PACKM2[3];
unsigned char DATA_PACKM3[3];

const char* datasaveram;
const char* datamqtt;
const char* datamqttinsdcard;
const char* filenamesavesd;

unsigned int data_IO;
unsigned int write_addeeprom;
int countfileinsd = 0;
int buf_head = 0;
int bufwrite_eeprom1, bufwrite_eeprom2;
int read_packADD;
int time_outwifi = 0;
int checksettime = 0;
int filename = 0;
int checksendmqtt = 0;

//uint64_t chipid;

String sDate;
String filenames;
String datainfilesd;
String Headerhistory = "";
String buffilenamedel;

long time_out = 0;
long time_limit = 100;


typedef enum {
    Runnning_e = 0x01,
   Fota_e  
}SysState;

/*-----------------Firmware Source Download------------------------*/
char url[100];//Url firmware download 
char md5_1[50];// md5 checksum firmware filename .bin

SysState state = Runnning_e;  //Create an instance state



void progress(DlState state, int percent){ //Show % Progress Download Firmware from Server
  Serial.printf("state = %d - percent = %d %c\n", state, percent,'%');//Print format show % progress
     prog = percent; 
     ota_t = millis() - startota;
     chk_ota_timeout(ota_t);//Call function for check timeout
 }
 
/*  Refer to errorCallback method in HttpFOTA.h   */
void error(char *message){ //Show Error Message During OTA
  printf("%s\n", message);
}

/*  Refer to startDownloadCallback method in HttpFOTA.h  */
void startDl(void){ // Start Download Firmware Function
  startota = millis();
}

void endDl(void){ //Show Time to OTA Finish Function 
  ota_t = millis() - startota;
  float otafinish = ota_t/1000.0;  //Sec
  Serial.print(String(ota_t) + " ms ");
  Serial.print(otafinish,2);
  Serial.println(" Sec.");
}

void chk_ota_timeout(unsigned long tm){ //Check TimeOut OTA Function 
  if((tm >= otatimeout)&&(prog < 100)){
   Serial.printf(" Time out! %d\n",tm);
   delay(50);
   ESP.restart();
  }
}

void LED2Status(byte delay1,byte delay2){ 
 if(millis() > tled + LEDTIME){
  for(byte b=0;b<=1;b++){
    //digitalWrite(LED2,!digitalRead(LED2));
    digitalWrite(LED2,LOW);
    delay(delay1);
    digitalWrite(LED2,HIGH);
    delay(delay2);
   }
   tled = millis();
  }
}

void ChipID(){//Show Chip ID
  chipid=ESP.getEfuseMac();//The chip ID is  MAC address(length: 6 bytes).
  Serial.printf("Machine Board Chip ID = %04X",(uint16_t)(chipid>>32));//print High 2 bytes
  Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.
 
}

void sendId(){
         chipid=ESP.getEfuseMac();
         (uint16_t)(chipid>>32);
         String cID = String((uint16_t)(chipid>>32),HEX);//Revise By RJK
         String eID = String((uint32_t)chipid,HEX);//Revise By RJK
         cID.toUpperCase();
         eID.toUpperCase();
         client.publish(sendconf,(cID+eID).c_str()); //Publish to Cloud
}
/*
     Machine Publish Configs to cloud if subscribe CONFIG_TOPIC Callback function call this function
     For mat data packet with delimeter $ between each value and | each key|value
     such as CF$id|chipid$v|version$w|WiFi SSID$p|WiFi Password$ms|mqttserver$mp|mqtt port$u|mqtt user$mw|mqtt password$config topic 
     CF is config header , id is chip id , v is version , w is WiFi SSID , p is WiFi password , ms is mqttserver ,mp is mqtt port , u is mqtt user
        mw is mqtt password , CFG Topic 
     such as get config  of machine G00001/C01/M001 
     CF$id|c49b38e0d84$v|AB01V1$w|FMX-Wireless$p|fmxfmxfmx$ms|m20.cloudmqtt.com$mp|11580$u|jqgehwzu$mw|Xmrb3tebPNY6$CFG/G00001/C01/M001         
*/

void sendconfig(){ 
         chipid=ESP.getEfuseMac();
         (uint16_t)(chipid>>32);
         String cID = String((uint16_t)(chipid>>32),HEX);//Revise By RJK
         String eID = String((uint32_t)chipid,HEX);//Revise By RJK
         cID.toUpperCase();
         eID.toUpperCase();
         String id = "id|" +cID + eID; id += "$"; //pack chip id
         String v = String(v_fw[0])+String(v_fw[1])+String(v_fw[2])+String(v_fw[3]); v = "v|" + v + "$";//pack version firmware
         String w = String(wifi_ssid); w = "w|"+w+"$";// pack WiFi SSID
         String wp = String(wifi_pwd); wp = "p|"+ wp + "$";// pack WiFi Password
         String ms = String(mqtt_server); ms = "ms|"+ ms + "$";// pack mqtt server
         String mp = String(mqttPort,DEC); mp = "mp|" + mp + "$"; //pack mqtt port 
         String u = String(mqttUser); u = "u|" + u + "$";//pack mqtt user 
         String mw = String(mqttPassword); mw = "mw|" + mw +"$"+String(getconf);//pack mqtt password
         String cfg = id + v + w + wp + ms + mp + u + mw; //Concatination 
         client.publish(sendconf,("CF$" +cfg).c_str()); //Publish to Cloud
}

void IOTCallback(char *topic, byte *payload, unsigned int length){
  Serial.println(topic);//Print topic received
  Serial.println((char*)payload);//print payload (or message) in topic received
  if((strncmp(gtopic, topic, strlen(gtopic)) == 0)||(strncmp(ctopic, topic, strlen(ctopic)) == 0)||(strncmp(stopic, topic, strlen(stopic)) == 0)){
    memset(url, 0, 100);
    memset(md5_1, 0, 50);
    char *tmp = strstr((char *)payload, "url:");//Query url: in payload(message received) and assign to pointer tmp
    char *tmp1 = strstr((char *)payload, ",");//Query , in payload(message received) and assign to pointer tmp1
    memcpy(url, tmp+strlen("url:"), tmp1-(tmp+strlen("url:")));
    
    char *tmp2 = strstr((char *)payload, "md5:");//Query md5: in payload(message received) and assign to pointer tmp2
    memcpy(md5_1, tmp2+strlen("md5:"), length-(tmp2+strlen("md5:")-(char *)&payload[0]));

    Serial.printf("started fota url: %s\n", url);
    Serial.printf("started fota md5: %s\n", md5_1);
    client.publish(ackota,stopic);// Publish message OTA TOPIC if acknowledge OTA backward to web server 
    state = Fota_e; //Start state Firmware OTA
   }
   /*  Check topic received and payload (message) is equal command for getconfig (getcf)  */
  if(strncmp(getconf, topic, strlen(getconf)) == 0){//Cmd from php or websocket getcf (Get config)
    if((char)payload[0] == 'g' && (char)payload[1] == 'e' && (char)payload[2] == 't' && (char)payload[3] == 'c' && (char)payload[4] == 'f'){
        sendconfig();
    }
    if((char)payload[0] == 'g' && (char)payload[1] == 'e' && (char)payload[2] == 't' && (char)payload[3] == 'i' && (char)payload[4] == 'd'){
        sendId();
    }
    if((char)payload[0] == 'c' && (char)payload[1] == 'f'){//Change config by https get to server iotfmx.com
        ChangeConfig();
     }
    if((char)payload[0] == 'c' && (char)payload[1] == 'f' && (char)payload[2] == 'b' ){//Change config by https get to server iotfmx.com
        ChangeConfig_Restart();
      } 
    if((char)payload[0] == 'c' && (char)payload[1] == 'c' && (char)payload[2] == 'f'){//Check current config
       currentConfig(SD,"/conf.txt");
     }
    if((char)payload[0] == 'c' && (char)payload[1] == 's'){// Check Cert SSL
       String rep = CheckCertSSL();
       client.publish(sendconf,rep.c_str(),Qos);
     }
   }
   
   
   if (strncmp(dbreply, topic, strlen(dbreply)) == 0) {
     Serial.println((char*)payload);
     
    for (int r = 0; r < length; r++) {
      //Serial.write((char)payload[r]);
        datareceivedmqtt += (char)payload[r];
      //         delay(20);
    }

    Serial.println();

  }

   
 }
int mqttconnect() {
  /* Loop until reconnected */
  while (1) {
    /* connect now */
    if (client.connect(clientId, mqttUser, mqttPassword)) {
      // if (client.connect(clientId,mqttUser,mqttPassword)) {
      Serial.println("Mqtt....connected");
      
      /* subscribe topic */
       client.subscribe(stopic,Qos);
       client.subscribe(gtopic,Qos);
        client.subscribe(ctopic,Qos);
        client.subscribe(getconf,Qos);
        client.subscribe(dbreply,Qos);
        
        
      }
    return 1;
  }
}

String CheckCertSSL(){
   String url = "https://www.iotfmx.com/iot/api/iotapi_checkcert.php?cid="+ String(clientId);
   Serial.println("Check use Certificate SSL from Server: " + url);
   HTTPClient http;
   http.begin(url, certssl);
   int httpCode = http.GET();
   if (httpCode == 200){
    String reply = http.getString();
    return reply;
   } else {
   Serial.println("Fail. error code " + String(httpCode));
   return "Fail";
  }
}

void ChangeConfig_Restart(){
  String url = "https://www.iotfmx.com/iot/api/iotapi_getconfig.php?getcfg="+ String(clientId);
 Serial.println();
 Serial.println("Get Coonfig from Server: " + url);
 HTTPClient http;
 http.begin(url, certssl);
  int httpCode = http.GET();
 if (httpCode == 200){
   String content = http.getString();
   Serial.println(content);
    WriteSDcfg(SD,"/conf.txt",content);
    client.publish(sendconf,"Write Config OK",Qos);
    digitalWrite(RST,!digitalRead(RST));
    delay(500);
    
   
  } else {
   Serial.println("Fail. error code " + String(httpCode));
  }
}


void ChangeConfig(){
 String url = "https://www.iotfmx.com/iot/api/iotapi_getconfig.php?getcfg="+ String(clientId);
 Serial.println();
 Serial.println("Get Coonfig from Server: " + url);
 HTTPClient http;
 http.begin(url, certssl);
  int httpCode = http.GET();
 if (httpCode == 200){
   String content = http.getString();
   Serial.println(content);
    WriteSDcfg(SD,"/conf.txt",content);
    client.publish(sendconf,"Write Config OK",Qos);
  } else {
   Serial.println("Fail. error code " + String(httpCode));
  }
}


void sdbegin()
{
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  } else {
    Serial.println("SD Card OK");
  }

}


void WriteSDcfg(fs::FS &fs, const char * path,String cdata){
  Serial.print(F("         WRITE NEW CONFIG To SD card     "));
    File iotfmx = fs.open(path, FILE_WRITE);
   
   iotfmx = SD.open(path,FILE_WRITE);
    iotfmx.print(cdata);
       iotfmx.close();
     
}


void wifi_setup()
{ 
  
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid,wifi_pwd); //assign wifi ssid , pass

  while (WiFi.status() != WL_CONNECTED) {//Loop is not connect until connected exit loop
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Connect ok");
}

void assignConfig(fs::FS &fs, const char* path) {
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open directory");
    return;
  }
  
  Serial.print("Reading file: ");
  Serial.println(path);
    int n =0;
    while (file.available()) {
     eachline = file.readStringUntil('\n');    
     int posi = eachline.indexOf(':');
     String val = eachline.substring(posi+1);
      //Serial.println(val);
      Line[n] = val;
      Line[n].trim();
      n++;
    }
   wifi_ssid = (const char*)Line[0].c_str();
   
   wifi_pwd = (const char*)Line[1].c_str();
  
   mqtt_server = (const char*)Line[2].c_str();
 
   mqttPort = Line[3].toInt();
   
   //Serial.println(mqtt_server);//debug ok
   //Serial.println(mqttPort);//debug ok

   mqttUser = (const char*)Line[4].c_str();

   mqttPassword = (const char*)Line[5].c_str();
   
   clientId = (const char*)Line[6].c_str();
 
   otatimeout = Line[7].toInt();
   sendtopic = (const char*)Line[8].c_str();
   gtopic = (const char*)Line[9].c_str();
   ctopic = (const char*)Line[10].c_str();
   stopic = (const char*)Line[11].c_str();
   ackota = (const char*)Line[12].c_str();
   getconf = (const char*)Line[13].c_str();
   sendconf = (const char*)Line[14].c_str();
   dbreply =  (const char*)Line[15].c_str(); // add by rjk
}


void currentConfig(fs::FS &fs, const char* path){

  String Content="";
  File frc = fs.open(path);
  if (!frc) {
    Serial.println("Failed to open directory");
    return;
  }
  while(frc.available()){
     Content += frc.readStringUntil('\n');
    }
   frc.close();
  Serial.println(Content);
  client.publish(sendconf,Content.c_str());
}

String readcert(fs::FS &fs, const char* path){
 File fr = fs.open(path);
  String SL = "";
  while(fr.available()){
    SL += fr.readStringUntil('\n');
    }
   fr.close();
  return SL;
}




void function_ota()
{
  /*-------------------FOR OTA--------------------------*/
  switch (state)
  {
    case Runnning_e:

      if (!client.connected()) {
        mqttconnect();
      }

      client.loop();
      break;
    case Fota_e:
      DlInfo info;
      info.url = url;
      // info.caCert = NULL;//if only use http then remember to set this to NULL
      info.caCert =  certssl; //SSL Cert iotfmx.com (secure server load from certi.txt)
      info.md5 = md5_1; // info.md5 is argument of setMD5(const char * expected_md5) in Update.h
      info.startDownloadCallback =  startDl;
      info.endDownloadCallback =    endDl;
      info.progressCallback  = progress;
      info.errorCallback     = error;
      int result = httpFOTA.start(info); //OTA Method
      if(result < 0){ // Check error return from class HttpFOTA
        delay(100);
        ESP.restart(); 
      }
      if(result == 1){
       String DT =  DateTimeNOW();
               DT += " OTA OK";
        client.publish(ackota,DT.c_str(),Qos);
        delay(1000);
        ESP.restart();  
      }
     break;
  }
}

String DateTimeNOW(){
   DateTime now = RTC.now();
   String DMY = String(now.day())+"/"+String(now.month())+"/"+String(now.year())+" "+String(now.hour())+":"+String(now.minute())+":"+String(now.second());
   return DMY;
}


void data_time()
{
  DateTime now = RTC.now();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  //  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //    Serial.printf("NTP TIME : %02d/%02d/%04d ",timeinfo.tm_mday,timeinfo.tm_mon + 1,timeinfo.tm_year + 1900);
  //    Serial.printf("%02d:%02d:%02d \r\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  if (checksettime == 0 ) {
    RTC.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    checksettime = 1;
  }
  else
  {
    if (timeinfo.tm_wday == 0 && timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec <= 60)
    {
      RTC.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      Serial.println("Update Time Success");
    }
  }
  Serial.printf("%02d", now.day());
  Serial.print('/');
  Serial.printf("%02d", now.month());
  Serial.print('/');
  Serial.printf("%02d", now.year());
  Serial.print(' ');
  Serial.printf("%02d", now.hour());
  Serial.print(':');
  Serial.printf("%02d", now.minute());
  Serial.print(':');
  Serial.printf("%02d", now.second());
  Serial.println();

  //delay(1000);
}

void sendmqtt()
{
  bufwrite_eeprom1 = mac.readAddress(writeaddr_eeprom1);
  bufwrite_eeprom2 = mac.readAddress(writeaddr_eeprom2);
  write_addeeprom = (bufwrite_eeprom2 << 8) + bufwrite_eeprom1;
  String sText;

  sText += sDate;
  for (int i = 0; i < sizeof(buf_io); i++)
  {
    sText += buf_io[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM1); i++)
  {
    sText += DATA_PACKPWM1[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM2); i++)
  {
    sText += DATA_PACKPWM2[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKRELAY); i++)
  {
    sText += DATA_PACKRELAY[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM3); i++)
  {
    sText += DATA_PACKPWM3[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM4); i++)
  {
    sText += DATA_PACKPWM4[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKAD1); i++)
  {
    sText += DATA_PACKAD1[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKAD2); i++)
  {
    sText += DATA_PACKAD2[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKAD3); i++)
  {
    sText += DATA_PACKAD3[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKGPS); i++)
  {
    sText += DATA_PACKGPS[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKM1); i++)
  {
    sText += DATA_PACKM1[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKM2); i++)
  {
    sText += DATA_PACKM2[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKM3); i++)
  {
    sText += DATA_PACKM3[i];
  }
  sText += ";" ;

  datasaveram = sText.c_str();
  delay(50);
  Serial.print("Data For PackSendMQTT : ");
  Serial.println(sText);
  Serial.print("EEPROM ADDR : ");
  Serial.println(write_addeeprom);
  delay(50);
  //  Serial.println("Save DATA To FRAM.");

  if (write_addeeprom >= 32000) //32000  //ถ้าเขียนถึง address ที่ 32000 ให้เอาข้อมูลทั้งหมดใส่ใน file sdcard
  {
    Serial.println("Please wait for read RAM To SDCARD");
    String datab;
    const char * datasavesdcard;
    datab = mac.read_all();
    
    datasavesdcard = datab.c_str();
    listcountfileindir(SD, "/history");
    delay(100);
    if (countfileinsd >= FILE_COUNT_INHISTORYSD) { //FILE_COUNT_INHISTORYSD
      const char * delfile;
      delfile = buffilenamedel.c_str();
      deleteFile(SD, delfile);
      countfileinsd = 0;
    }
    String a = "/history/" + filenames + ".txt";
    File file = SD.open("/history");
    if (!file)
    {
      Serial.println("Create Directory");
      SD.mkdir("/history");
    }
    filenamesavesd = a.c_str() ;
    //    writeFile(SD, filenamesavesd , datasavesdcard);
    if (!writeFile(SD, filenamesavesd , datasavesdcard)) {
      Serial.println("******** Write DATA TO SDCARD Success ********");
      //Create file in sd card success update address eeprom = 0
      mac.writeAddress(writeaddr_eeprom1, 0); 
      mac.writeAddress(writeaddr_eeprom2, 0);
      write_addeeprom = 0;
      filename++;
    } else {
      Serial.println("Can't Save SD Card To RAM");
    }


  }
  for (int i = 0; i < addrsize; i++)
  {
    mac.writeAddress(write_addeeprom, datasaveram[i]);
    write_addeeprom++;
  }

  mac.writeAddress(writeaddr_eeprom1, write_addeeprom & 0xFF); //ระบุ ADDRESS
  mac.writeAddress(writeaddr_eeprom2, (write_addeeprom >> 8) & 0xFF);

  checkandsendmqtt(sText, write_addeeprom);
}

int checkandsendmqtt(String sdatamqtt, int write_addr)
{
  int buf_lasteeporm = write_addr;
  time_out = 0;
  while (1)
  {
   
    if (client.connected())
    {
      
     if (write_addeeprom > 0)
      {
        String datahistory;
        String datamakemqtt;
        mac.Scan_data_sstv(write_addr);
        datamakemqtt = mac.make_send_string(sdatamqtt);
        datamqtt = datamakemqtt.c_str();
        
        
        //client.setCallback(IOTCallback);
        client.publish(sendtopic,datamqtt);
        if(datareceivedmqtt == "OK"){ 
          Serial.println("*********Send Mqtt Data Realtime Success********");
          Serial.print("write_addr : ");
          Serial.println(write_addr);
          Serial.print("Response : ");
          Serial.println(datareceivedmqtt);
          mac.Check_senddata_fram(write_addr);
          datareceivedmqtt = "";
          checksendmqtt = 1;
         
        } else {
          
          Serial.println("******** Can't Send Server no reply OK (can't insert to database) ********");
        }
         //checksendmqtt = 1; // test
        delay(100);
        
        if (checksendmqtt == 1) {
          sendsdcardtomqtt();
          checksendmqtt = 0;
        }

      }
      
      return 1;
    }
    else {
      mqttconnect();
    }
    time_out++;
    if (time_out > time_limit)
    {
      Serial.println("Can't Conect TO MQTT");
      return 0;
    }
    delay(10);
   
  }
}

void sendsdcardtomqtt() {
  const char *filenameinsd;
  String buffilename;
  sdbegin();
  /*------------------- List Filename In SDCARD ---------------------*/
  filenameinsd = listDir(SD, "/history", 0);
  buffilename = filenameinsd;
  if (filenameinsd != "0") {
    readFileinSD(SD, filenameinsd);
    packdata_HEADSDCARD();
    delay(100);
    datainfilesd = Headerhistory + datainfilesd;
    datamqttinsdcard = datainfilesd.c_str();
    client.publish(sendtopic,datamqttinsdcard);
    client.setCallback(IOTCallback);
    if (datareceivedmqtt == "OK"); {
      Serial.println("***************** Send File In SDCARD OK *****************");
      const char *delfilename;
      delfilename = buffilename.c_str();
      datainfilesd = "";
      deleteFile(SD, delfilename);
    }
  } else
  {
    Serial.println("**************** No File In SDCARD *****************");
  }


}

void listcountfileindir(fs::FS &fs, const char * dirname) {
  int chkfile = 0;
  const char *historydelname;
  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }
  File file = root.openNextFile();
  if (file) {
    delay(50);
    historydelname = file.name();
    delay(50);
    buffilenamedel = historydelname;
  }
  while (file) {
    file = root.openNextFile();
    chkfile++;
  }
  countfileinsd = chkfile;
}

const char* listDir(fs::FS &ff, const char * dirname, uint8_t levels) {
  const char *historyfilename;
  Serial.printf("Listing directory: %s\n", dirname);
  File root = ff.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return "";
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return "";
  }

  File file = root.openNextFile();
  if (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(ff, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.println(file.name());
      delay(50);
      historyfilename = file.name();
      delay(50);
      Serial.print("filenameinsdcard : ");
      Serial.println(historyfilename);
      return historyfilename;
    }
    file = root.openNextFile();
  }
  historyfilename = "0";
  return historyfilename;

}

void createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}


void readFileinSD(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Reading file: ");
  Serial.println(path);
  while (file.available()) {
    datainfilesd += file.readStringUntil('\n');
  }  file.close();
}

int writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.print("Writing file: ");
  Serial.println(path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return file;
  }
  if (file.print(message)) {
    Serial.println("File written");

  } else {
    Serial.println("Write failed");
  }
  file.close();
  return file;
}



void deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}



void packdata_HEADSDCARD() {

  Headerhistory = "";
  DATA_PACKHEADHIS[0] = 0x23;
  DATA_PACKHEADHIS[1] = 0x52; //#Serial
  DATA_PACKHEADHIS[2] = 0x3B;
  DATA_PACKHEADHIS[3] = DATA_PACKHEAD[3];
  DATA_PACKHEADHIS[4] = DATA_PACKHEAD[4];
  DATA_PACKHEADHIS[5] = DATA_PACKHEAD[5];
  DATA_PACKHEADHIS[6] = DATA_PACKHEAD[6];
  DATA_PACKHEADHIS[7] = DATA_PACKHEAD[7];
  DATA_PACKHEADHIS[8] = DATA_PACKHEAD[8];
  DATA_PACKHEADHIS[9] = DATA_PACKHEAD[9];
  DATA_PACKHEADHIS[10] = DATA_PACKHEAD[10];
  DATA_PACKHEADHIS[11] = DATA_PACKHEAD[11];
  DATA_PACKHEADHIS[12] = DATA_PACKHEAD[12];
  DATA_PACKHEADHIS[13] = DATA_PACKHEAD[13];
  DATA_PACKHEADHIS[14] = DATA_PACKHEAD[14];
  DATA_PACKHEADHIS[15] = 0x3B;
  for (int i = 0; i < sizeof(DATA_PACKHEADHIS); i++) {
    Headerhistory += DATA_PACKHEADHIS[i];
    delay(10);
  }
  Serial.print("packdata_HEADSDCARD : ");
  Serial.println(Headerhistory);
}

void packdata_HEAD()
{

  DATA_PACKHEAD[0] = 0x23;
  DATA_PACKHEAD[1] = 0x4D; //#Serial
  DATA_PACKHEAD[2] = 0x3B;
  DATA_PACKHEAD[3] = Chipid_buf[0];
  DATA_PACKHEAD[4] = Chipid_buf[1];
  DATA_PACKHEAD[5] = Chipid_buf[2];
  DATA_PACKHEAD[6] = Chipid_buf[3];
  DATA_PACKHEAD[7] = Chipid_buf[4];
  DATA_PACKHEAD[8] = Chipid_buf[5];
  DATA_PACKHEAD[9] = Chipid_buf[6];
  DATA_PACKHEAD[10] = Chipid_buf[7];
  DATA_PACKHEAD[11] = Chipid_buf[8];
  DATA_PACKHEAD[12] = Chipid_buf[9];
  DATA_PACKHEAD[13] = Chipid_buf[10];
  DATA_PACKHEAD[14] = Chipid_buf[11];
  DATA_PACKHEAD[15] = 0x3B;
  DATA_PACKHEAD[16] = v_fw[0];
  DATA_PACKHEAD[17] = v_fw[1];
  DATA_PACKHEAD[18] = v_fw[2];
  DATA_PACKHEAD[19] = v_fw[3];
  DATA_PACKHEAD[20] = 0x3B;
  if (buf_head == 0) {
    for (int i = 0; i < sizeof(DATA_PACKHEAD); i++)
    {
      mac.packet_header += DATA_PACKHEAD[i];
    }
    buf_head = 1;
    delay(10);
  }


}

void packdata_DATE()
{
  sDate = "";
  DateTime now = RTC.now();
  DATA_PACKDATE[0] = now.day();
  DATA_PACKDATE[1] = now.month();
  DATA_PACKDATE[2] = now.year() - 2000;
  DATA_PACKDATE[3] = now.hour();
  DATA_PACKDATE[4] = now.minute();
  DATA_PACKDATE[5] = now.second();
  
  //sprintf(buf_date, "%02u%02u%02u%02u%02u%02u",now.day(),now.month(),now.year()-2000,now.hour(),now.minute(),now.second());
  
  sprintf(buf_date, "%02u%02u%02u%02u%02u%02u", now.year() - 2000, now.month(), now.day(), now.hour(), now.minute(), now.second());
  for (int i = 0; i < sizeof(buf_date); i++)
  {
    sDate += buf_date[i];
  }
  filenames = sDate;
  sDate += ";";
  delay(50);
}

void packdata_IO()
{
  data_IO = (IO_8 << 7) | (IO_7 << 6) | (IO_6 << 5) | (IO_5 << 4) | (IO_4 << 3) | (IO_3 << 2) | (IO_2 << 1) | (IO_1 << 0);
  DATA_PACKIO[0] = 00;
  DATA_PACKIO[1] = data_IO;
  sprintf(buf_io, "%02X%02X", DATA_PACKIO[0], DATA_PACKIO[1]);
}

void packdata_PWM()
{
  DATA_PACKPWM1[0] = 11;
  DATA_PACKPWM1[1] = 11;
  DATA_PACKPWM1[2] = 11;
  DATA_PACKPWM1[3] = 11;

  DATA_PACKPWM2[0] = 11;
  DATA_PACKPWM2[1] = 11;
  DATA_PACKPWM2[2] = 11;
  DATA_PACKPWM2[3] = 11;
}

void packdata_RELAY()
{
  DATA_PACKRELAY[0] = 11;
}

void packdata_PWM2()
{
  DATA_PACKPWM3[0] = 11;
  DATA_PACKPWM3[1] = 11;
  DATA_PACKPWM3[2] = 11;
  DATA_PACKPWM3[3] = 11;

  DATA_PACKPWM4[0] = 11;
  DATA_PACKPWM4[1] = 11;
  DATA_PACKPWM4[2] = 11;
  DATA_PACKPWM4[3] = 11;
}

void packdata_AD()
{
  DATA_PACKAD1[0] = 11;
  DATA_PACKAD1[1] = 11;
  DATA_PACKAD1[2] = 11;
  DATA_PACKAD1[3] = 11;

  DATA_PACKAD2[0] = 11;
  DATA_PACKAD2[1] = 11;
  DATA_PACKAD2[2] = 11;
  DATA_PACKAD2[3] = 11;

  DATA_PACKAD3[0] = 11;
  DATA_PACKAD3[1] = 11;
  DATA_PACKAD3[2] = 11;
  DATA_PACKAD3[3] = 11;
}

void packdata_GPS()
{
  DATA_PACKGPS[0] = 11;
  DATA_PACKGPS[1] = 11;
  DATA_PACKGPS[2] = 11;
  DATA_PACKGPS[3] = 11;
  DATA_PACKGPS[4] = 11;
  DATA_PACKGPS[5] = 11;
  DATA_PACKGPS[6] = 11;
}



void packdata_M1()
{
  DATA_PACKM1[0] = 11;
  DATA_PACKM1[1] = 11;
  DATA_PACKM1[2] = 11;
  DATA_PACKM1[3] = 11;
  DATA_PACKM1[4] = 11;
  DATA_PACKM1[5] = 11;
  DATA_PACKM1[6] = 11;
  DATA_PACKM1[7] = 11;
}

void packdata_M2()
{
  DATA_PACKM2[0] = 11;
  DATA_PACKM2[1] = 11;
  DATA_PACKM2[2] = 11;
}

void packdata_M3()
{
  DATA_PACKM3[0] = 11;
  DATA_PACKM3[1] = 11;
  DATA_PACKM3[2] = 11;
}

void adcbegin()
{
  if (!adc.begin(18, 23, 19, 15))
  {
    Serial.print("Initializing MCP3008...Failed");
  }
  Serial.println("initialization MCP3008 done.");
  delay(100);
}



void setup() {
  pinMode(LED2,OUTPUT);
  digitalWrite(LED2,HIGH);//HIGH Turn off LOW on
  Serial.begin(115200);
  v_fw[0] = 0x30;
  v_fw[1] = 0x31;
  v_fw[2] = 0x31;
  v_fw[3] = 0x30;
  sdbegin();
   Serial.println(F("Connected SD Card ok."));
    Serial.println(F("Load config from SD card file:conf.txt"));
  assignConfig(SD,"/conf.txt");
  ChipID();//Show Chip ID (IMEI)

  certssl = (char*)readcert(SD,"/certi.txt").c_str(); //Load certificate and convert to char* datatype
  //Serial.println(certssl);
  wifi_setup();
  delay(100);
  Wire.begin();
  delay(100);
  mac.begin();
  delay(100);  
  RTC.begin();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(100);
  Serial.println("NOW: "+DateTimeNOW());
  client.setServer(mqtt_server,mqttPort); 
  client.setCallback(IOTCallback);
   
}
  
void loop(){
  LED2Status(125,250);
   /*-------------------FOR OTA--------------------------*/
   function_ota();

   /*-------------------Start GET CHIPID--------------------------*/
  chipid = ESP.getEfuseMac();
  sprintf(Chipid_buf, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)(chipid));
  delay(50);

  /*-------------------Start DATETIME--------------------------*/
  data_time();

  /*-------------------Start PackData--------------------------*/
  packdata_HEAD();
  packdata_DATE();
  packdata_IO();
  packdata_PWM();
  packdata_RELAY();
  packdata_PWM2();
  packdata_AD();
  packdata_GPS();
  packdata_M1();
  packdata_M2();
  packdata_M3();
  delay(100);

  /*-------------------------- Send DATA MQTT  --------------------------*/
  sendmqtt();
  delay(100);
  Serial.println("");
}
