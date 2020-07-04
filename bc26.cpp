#include "bc26.h"

static String         bc26_buff, bc26_host, bc26_user, bc26_key;
static int            bc26_port;
static SoftwareSerial bc26(8, 9);

static bool _BC26SendCmdReplyS(String cmd, String reply, unsigned long timeout);
static bool _BC26SendCmdReplyC(char *cmd, const char *reply, unsigned long timeout);

static bool _BC26SendCmdReplyS(String cmd, String reply, unsigned long timeout)
{
    bc26_buff = "";
    DEBUG_PRINT(cmd);
    bc26.println(cmd);
    unsigned long timer = millis();
    while (millis() - timer < timeout) {
        if (bc26.available()) {
            bc26_buff += bc26.readStringUntil('\n');
        }
        if (bc26_buff.indexOf(reply) != -1) {
            DEBUG_PRINT(bc26_buff);
            return true;
        }
    }
    return false;
}

static bool _BC26SendCmdReplyC(char *cmd, const char *reply, unsigned long timeout)
{
    bc26_buff = "";
    DEBUG_PRINT(cmd);
    bc26.println(cmd);
    unsigned long timer = millis();
    while (millis() - timer < timeout) {
        if (bc26.available()) {
            bc26_buff += bc26.readStringUntil('\n');
        }
        if (bc26_buff.indexOf(reply) != -1) {
            DEBUG_PRINT(bc26_buff);
            return true;
        }
    }
    return false;
}

bool BC26Init(long baudrate, String apn, int band)
{
    // set random seed
    randomSeed(analogRead(A0));
    // wait boot
    delay(5000);
    long gsm_load = 46692;
    // init nbiot SoftwareSerial
    bc26.begin(baudrate);
    // echo mode off
    _BC26SendCmdReplyS("ATE0", "OK", 2000);
    // set band
    _BC26SendCmdReplyS("AT+QBAND=1," + String(band), "OK", 2000);
    // close EDRX
    _BC26SendCmdReplyS("AT+CEDRXS=0", "OK", 2000);
    // close SCLK
    _BC26SendCmdReplyS("AT+QSCLK=0", "OK", 2000);

    while (!_BC26SendCmdReplyS("AT+CGATT?", "+CGATT: 1", 2000)) {
        if (apn == "internet.iot") {
            gsm_load = 46692;
        } else if (apn == "twm.nbiot") {
            gsm_load = 46697;
        } else {
            Serial.println(F("apn error !!"));
        }
        Serial.println(F("Connect to 4GAP....."));
        if (_BC26SendCmdReplyS("AT+COPS=1,2,\"" + String(gsm_load, DEC) + "\"", "OK", 20000)) {
            Serial.println(F("Network is ok !!"));
        } else {
            Serial.println(F("Network is not ok !!"));
        }
    }
    return true;
}

bool BC26ConnectMQTTServer(String host, String user, String key, int port)
{
    long random_id = random(65535);
    bc26_host      = host;
    bc26_user      = user;
    bc26_key       = key;
    bc26_port      = port;

    while (!_BC26SendCmdReplyS("AT+QMTCONN?", "+QMTCONN: 0,3", 2000)) {
        while (!_BC26SendCmdReplyS("AT+QMTOPEN?", "+QMTOPEN: 0,", 2000)) {
            if (_BC26SendCmdReplyS("AT+QMTOPEN=0,\"" + host + "\"," + String(port, DEC),
                                   "+QMTOPEN: 0,0", 20000)) {
                Serial.println(F("Opened MQTT Channel Successfully"));
            } else {
                Serial.println(F("Failed to open MQTT Channel"));
            }
        }
        if (_BC26SendCmdReplyS("AT+QMTCONN=0,\"Arduino_BC26_" + String(random_id, DEC) + "\",\"" +
                                    user + "\",\"" + key + "\"",
                               "+QMTCONN: 0,0,0", 20000)) {
        } else {
            Serial.println(F("Failed to Connect MQTT Server"));
        }
    }
    Serial.println(F("Connect MQTT Server Successfully"));
    return true;
}

bool BC26MQTTPublish(const char *topic, char *msg, int qos)
{
    char buff[200];
    long msgID = 0;
    if (qos > 0) {
        msgID = random(1, 65535);
    }
    sprintf(buff, "AT+QMTPUB=0,%ld,%d,0,\"%s\",\"%s\"", msgID, qos, topic, msg);
    DEBUG_PRINT(buff);
    while (!_BC26SendCmdReplyC(buff, "+QMTPUB: 0,0,0", 10000)) {
        BC26ConnectMQTTServer(bc26_host, bc26_user, bc26_key, bc26_port);
    }
    return true;
}

bool BC26MQTTSubscribe(const char *topic, int qos)
{
    char buff[200];
    sprintf(buff, "AT+QMTSUB=0,1,\"%s\",%d", topic, qos);
    while (!_BC26SendCmdReplyC(buff, "+QMTSUB: 0,1,0,0", 10000)) {
        BC26ConnectMQTTServer(bc26_host, bc26_user, bc26_key, bc26_port);
    }
    Serial.print(F("Subscribe Topic("));
    Serial.print(topic);
    Serial.println(F(") Successfully"));
    return true;
}

int getBC26CSQ(void)
{
    String rssi;
    int    s_idx;

    if (_BC26SendCmdReplyS("AT+CSQ", "+CSQ: ", 2000)) {
        s_idx = bc26_buff.indexOf("+CSQ: ");
        s_idx += 6;
        while (bc26_buff[s_idx] != ',') {
            rssi += bc26_buff[s_idx++];
        }
        return rssi.toInt();
    }
    return -1;
}

bool readBC26MQTTMsg(const char *topic, char *msg)
{
    char *head, *tail;
    char  buff[50];
    bc26_buff = "";

    if (bc26.available()) {
        bc26_buff += bc26.readStringUntil('\n');
        sprintf(buff, "+QMTRECV: 0,0,\"%s\",\"", topic);
        head = strstr(bc26_buff.c_str(), buff);
        if (head) {
            DEBUG_PRINT("receive:");
            DEBUG_PRINT(bc26_buff);
            DEBUG_PRINT(head);
            head += (15 + strlen(topic) + 3);
            DEBUG_PRINT(head);
            tail = strstr(head, "\"");
            DEBUG_PRINT(tail);
            strncpy(msg, head, tail - head);
            msg[tail - head] = '\0';
            DEBUG_PRINT(msg);
            return true;
        }
    }
    return false;
}
