// -----------------------------------------------------------------
// Powerline Frequency Counter - A simple powerline frequency analyzer
// Copyright (C) 2020  Gabriele Bonacini
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------

#pragma once

#include<ESP8266WiFi.h> 
#include<map>
#include<string>
#include<cstdio>

namespace powerline {
  
class FrequencyMeter {
    public:
        // Default pin -> D3
        FrequencyMeter(std::string _ssid, std::string _pwd, int _pin=0, int _port=80);
        void init(void)                                    noexcept;
        void measure(int samples=50)                       noexcept;
        void response(void)                                noexcept;
        
    private:
        std::string ssid,
                    password,
                    request;                 
        int acPin, 
            pHigh{0},
            pLow{0};

        size_t  sampleCounter{0};
            
        float pTime{0.0F},
              freq{0.0F};

        WiFiServer server;

        std::map<int, int>  harmonics;

        void connect(void)                                 const noexcept;
        void startService(void)                                  noexcept;
        void reset(void)                                         noexcept;
        void fillFreq(char* buff, std::string& data)       const noexcept;
        void fillAnomalies(char* buff, std::string& data)  const noexcept;
};

FrequencyMeter::FrequencyMeter(std::string _ssid, std::string _pwd, int _pin, int _port) 
   : ssid(_ssid), password(_pwd), acPin(_pin), server(_port)
{
    Serial.begin(115200);
    pinMode(acPin, INPUT);
}

void FrequencyMeter::init(void) noexcept {
    connect();
    startService();
}

void FrequencyMeter::reset(void) noexcept{
    sampleCounter = 0;
    harmonics.clear();
}

void FrequencyMeter::connect(void) const noexcept{
  Serial.print("\n\nConnecting to: ");
  Serial.println(ssid.c_str()); 
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected"); 
}

void FrequencyMeter::startService(void) noexcept{
  server.begin();
  Serial.print("Server started.\nURL: \nhttp://");
  Serial.print(WiFi.localIP());
  Serial.println("/"); 
}

void FrequencyMeter::measure(int samples) noexcept{
  float freqSum{0.0F},
        cSample{0.0F};
  int   nHarmonics{0};

  reset();
  
  for(int i=0; i<samples; i++){
      pHigh = pulseIn(acPin, HIGH);
      pLow  = pulseIn(acPin, LOW);
      pTime = pHigh + pLow;
      cSample = 1000000 / pTime;
      // Consider everything above/below  1% of 50Hz an anomaly
      if(cSample >= 49.50F && cSample <= 50.50F ){
          freqSum += cSample;
          sampleCounter++;
      }else{
          harmonics.emplace(static_cast<int>(cSample), 0);
          harmonics[cSample] = harmonics[cSample] + 1; 
          nHarmonics++;  
      }
  }
  freq = freqSum / sampleCounter;
}

void FrequencyMeter::fillFreq(char* buff, std::string& data) const noexcept{
        if(sampleCounter >= 1){
            // TODO: use to_string() or stringstream when available
            data.append("Frequency(Hz):");
            sprintf(buff, "%f", freq);
            data.append(buff).append(":");
            sprintf(buff, "%u", sampleCounter);
            data.append(buff).append(":");
        }else{
            data.append("Empty");
        }
}

void FrequencyMeter::fillAnomalies(char* buff, std::string& data) const noexcept{
        if(harmonics.size() >= 1){
            // TODO: use to_string() or stringstream when available
            data.append("Anomalies:");
            for(auto it = harmonics.begin(); it != harmonics.end(); ++it){
                 sprintf(buff, "%d", it->first);
                 data.append(buff).append(":");
                 sprintf(buff, "%d", it->second);
                 data.append(buff).append(":");
             }
        }else{
            data.append("Empty");
        }
}

void FrequencyMeter::response(void) noexcept{
  WiFiClient client = server.available();
  if (!client) 
    return;
 
  Serial.println("New client");
  while(!client.available())
    delay(1);

  char buff[64];

  client.readStringUntil('\r').toCharArray(buff, sizeof(buff));
  request = buff;
  
  std::string data;
  std::string response("HTTP/1.1 200 OK\nAccept-Ranges: bytes\nContent-Length: ");

  if (request.find("frequency") != std::string::npos){
        fillFreq(buff, data);
  } else if (request.find("anomalies") != std::string::npos){
        fillAnomalies(buff, data);
  } else if(request.find("alldata") != std::string::npos){
        fillFreq(buff, data);
        data.append("\n");
        fillAnomalies(buff, data);
  }else{
        data.append("Invalid Request.");
  }

  sprintf(buff, "%d", data.size());
  std::string nFreq(buff);
  response.append(nFreq.c_str()).append("\nConnection: close\nContent-Type: text/plain\n\n")
          .append(data.c_str()).append("\n");

  std::string info("Request: ");
  info.append(request).append("\nResponse: ").append(response);
  
  Serial.println(info.c_str());
  client.print(response.c_str());
  client.flush();

  delay(1);
  Serial.print("Client disonnected.\n");
}

} // End Namespace
