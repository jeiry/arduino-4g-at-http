#include <ArduinoJson.h>
//rx - 17 tx - 16
bool sending = false;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(115200);
  Serial.println("start");
  delay(2000);
}
int step = 0;
int retry = 0;
void loop() {
  if (step == 0) {
    Serial.println("---查询卡是否插好---");
    if (sentAt("AT+CPIN?").indexOf("+CPIN: READY") != -1) {
      step = 1;
    } else {
      Serial.println("卡没有插好");
    }
    delay(1000);
  } else if (step == 1) {
    Serial.println("---查询设置信号质量---");
    if (sentAt("AT+CSQ").indexOf("OK") != -1) {
      Serial.println("第1个数20左右正常");
      step = 2;
    }
    delay(1000);
  } else if (step == 2) {
    Serial.println("---查询网络注册状态---");
    if (sentAt("AT+CREG?").indexOf("OK") != -1) {
      Serial.println("第2个数为1就是已经连网了");
      step = 3;
    }
    delay(1000);
  } else if (step == 3) {
    Serial.println("---设置中国电信4G APN---");
    if (sentAt("AT+CSTT=\"ctlte\",\"ctnet@mycdma.cn\",\"vnet.mobi\"").indexOf("OK") != -1) {
      Serial.println("APN设置成功");
      step = 4;
      retry = 0;
    } else {
      retry ++;
      if (retry == 3) {
        sentAt("AT+RESET");
        Serial.println("重启4G");
        retry = 0;
        step = 0;
        delay(3000);
      }
      Serial.println("APN设置失败");
    }
    delay(1000);
  } else if (step == 4) {
    Serial.println("---激活移动场景，激活后能获取到IP---");
    if (sentAt("AT+CIICR").indexOf("OK") != -1) {
      Serial.println("激活成功");
      step = 5;
      retry = 0;
    } else {
      retry ++;
      if (retry == 3) {
        sentAt("AT+RESET");
        Serial.println("重启4G");
        retry = 0;
        step = 0;
        delay(3000);
      }
      Serial.println("激活失败");
    }
    delay(1000);
  } else if (step == 5) {
    Serial.println("---查询IP，只有获取到IP后才能上网---");
    if (sentAt("AT+CIFSR=?").indexOf("OK") != -1) {
      Serial.println("查询成功");
      step = 6;
    } else {
      Serial.println("查询失败");
    }
    delay(1000);
  } else if (step == 6) {
    Serial.println("---同步网络时间---");
    if (sentAt("AT+CNTP").indexOf("OK") != -1) {
      Serial.println("同步成功");
      step = 7;
    } else {
      Serial.println("同步失败");
    }
    delay(1000);
  } else if (step == 7) {
    Serial.println("---同实时时钟---");
    if (sentAt("AT+CCLK?").indexOf("OK") != -1) {
      Serial.println("获取成功");
      step = 8;

      String str = httpGet("www.weather.com.cn/data/sk/101010100.html");
      Serial.println("http结果：");
      //      Serial.println(str.substring(str.indexOf("{"),str.indexOf("OK")));
      String jsonStr = str.substring(str.indexOf("{"), str.indexOf("OK"));
      StaticJsonDocument<512> doc;
      auto error = deserializeJson(doc, jsonStr); //解析消息
      if (error) { //检查解析中的错误
        Serial.println("Json 解析错误" + String(error.c_str()));
      }else{
        const char* weatherinfo_city = doc["weatherinfo"]["city"]; 
        Serial.println(weatherinfo_city);
      }

      

    } else {
      //      601 网络错误(Network Error)
      Serial.println("获取失败");
    }
    delay(1000);
  }


}

String sentAt(String command) {
  if (sending == false) {
    sending = true;
    Serial.print("send:");
    Serial2.println(command);
    int i = 0;
    while (1) {
      if (Serial2.available()) {
        String respond = Serial2.readString();
        Serial.print(respond);
        sending = false;
        return respond;
      }
      i ++;
      if (i > 5000) {
        Serial.println("超时");
        sending = false;
        return "";
      }
    }
  } else {
    Serial.println("BUSY");
    return "";
  }

}
int httpStep = 0;
String httpGet(String Url) {
  if (step >= 5) {
    while (1) {
      if (httpStep == 0) {
        Serial.println("---设置PDP承载之APN参数---");
        if (sentAt("AT+SAPBR=3,1,\"APN\",\"ctlte\"").indexOf("OK") != -1) {
          Serial.println("成功");
          httpStep = 1;
        }
        delay(1000);
      } else if (httpStep == 1) {
        Serial.println("---激活GPRS PDP上下文---");
        if (sentAt("AT+SAPBR=1,1").indexOf("OK") != -1) {
          Serial.println("成功");

        }
        httpStep = 2;
        delay(1000);
      } else if (httpStep == 2) {
        delay(1000);
        Serial.println("---初始化HTTP---");
        if (sentAt("AT+HTTPINIT").indexOf("OK") != -1) {
          Serial.println("成功");
          httpStep = 3;
        }
        delay(1000);
      } else if (httpStep == 3) {
        Serial.println("---初始化HTTP url---");
        String cmd = "AT+HTTPPARA=\"URL\",\"" + Url + "\"";
        if (sentAt(cmd).indexOf("OK") != -1) {
          Serial.println("成功");
          httpStep = 4;
          delay(1000);
        }
        delay(1000);
      } else if (httpStep == 4) {
        Serial.println("---HTTP get 数据,收到OK表示发送成功---");
        //0 get\ 1 post\ 2 head
        if (sentAt("AT+HTTPACTION=0").indexOf("200") != -1) {
          Serial.println("收到数据");
          httpStep = 5;
        } else {
          sentAt("AT+HTTPTERM");
          httpStep = 0;
          Serial.println("HTTP错误状态码");
          /**
             600 Not HTTP PDU
             601 网路问题 没联网
             602 内存不够
             603 DNS Error
            604 Stack Busy
           **/
          return "";
        }
        delay(1000);
      } else if (httpStep == 5) {
        Serial.println("---读取获取到的HTTP数据---");
        //1~319488 单位:字节
        String respond = sentAt("AT+HTTPREAD=0,14615");
        if (respond.indexOf("OK") != -1) {
          Serial.println("返回数据");
          sentAt("AT+HTTPTERM");
          httpStep = 0;
          return respond;
        }
        delay(1000);
      }
    }
  } else {
    return "";
  }


}
