#include <M5Core2.h>
#include <M5_ENV.h>
#include <SD.h>
#include <EEPROM.h>

RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;
char timeStrbuff[64];



const unsigned long SOIL_MOISTURE_INTERVAL = 1000;
const int SOIL_MOISTURE_THRESHOLD = 1850;

void page1a();
void page1b();
void page2();
void page3();
void page4();
void page5();
void clearScreen();


TouchPoint_t touchPos;      // Pozycja punktu dotyku
bool touchPressed = false;  // Czy ekran jest wciśnięty?
bool buttonOn = false;      // Czy przycisk jest aktywny?
HotZone button(120, 120, 200, 200);

File dataFile;
char fileName[] = "/data2.txt";
SHT3X sht30;
QMP6988 qmp6988;
unsigned long now = 0, last = 0, nowPump = 0, pumpStartTime = 0, lastPump = 0;
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;

char refresh = 1;
int screenNum = 0;

#define INPUT_PIN 36
#define PUMP_PIN 26

#define E_CLICK 1

bool buttonsEnabled = false;
bool pumpRunning = false;

Button lt(14, 42, 110, 30, "left-top");
Button lb(177, 42, 110, 30, "left-bottom");

boolean isButtonPressed = false;

float avg_temperature =0, avg_pressure=0, avg_humidity=0, avg_soil_moisture=0;

unsigned long lastWateringTime = 0;
int lastWateringDay = 0;

void flushTime() {
  M5.Rtc.GetTime(&RTCtime);
  M5.Rtc.GetDate(&RTCDate);
  sprintf(timeStrbuff, "%d/%02d/%02d %02d:%02d:%02d", RTCDate.Year,
          RTCDate.Month, RTCDate.Date, RTCtime.Hours, RTCtime.Minutes,
          RTCtime.Seconds);
  M5.Lcd.setCursor(10, 100);
  M5.Lcd.println(timeStrbuff);
}

void setupTime() {
  RTCtime.Hours = 01;
  RTCtime.Minutes = 00;
  RTCtime.Seconds = 20;
  if (!M5.Rtc.SetTime(&RTCtime)) Serial.println("wrong time set!");
  RTCDate.Year = 2023;
  RTCDate.Month = 4;
  RTCDate.Date = 26;
  if (!M5.Rtc.SetDate(&RTCDate)) Serial.println("wrong date set!");
}

void colorButtons(Event& e) {
  Button& b = *e.button;
  if (!isButtonPressed) {
    M5.Lcd.fillRect(b.x, b.y, b.w, b.h, GREEN);
    isButtonPressed = true;
  } else {
    M5.Lcd.fillRect(b.x, b.y, b.w, b.h, BLACK);
    isButtonPressed = false;
  }
}

int choose = 0;

bool flag = true;
int rawADC = 0;

void ltClick(Event& e) {
  choose = 1;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.drawJpgFile(SD, "/ficus.jpg", 31, 84);
  M5.Lcd.setCursor(203, 50);
  M5.Lcd.print("Ficus");
  M5.Lcd.setCursor(157, 76);
  M5.Lcd.print("Comfort T: 23");

  M5.Lcd.setCursor(156, 102);
  M5.Lcd.print("comfort %: 75");

  M5.Lcd.setCursor(154, 128);
  M5.Lcd.print("comfort P: 60");

  M5.Lcd.setCursor(154, 153);
  M5.Lcd.print("description:");

  M5.Lcd.setCursor(157, 175);
  M5.Lcd.print("likes humidit");

  M5.Lcd.setCursor(154, 193);
  M5.Lcd.print("good lighting");


  M5.Lcd.setCursor(153, 215);
  M5.Lcd.print("shiny leaves");

  // usunięcie wszystkich zarejestrowanych funkcji dla przycisku lt
  lt.delHandlers();
  lb.delHandlers();
  buttonsEnabled = true;
}

void lbClick(Event& e) {

  choose = 2;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillRect(66, 96, 20, 120, GREEN);
  M5.Lcd.fillCircle(75, 91, 15, GREEN);
  M5.Lcd.fillRect(122, 96, 15, 50, GREEN);
  M5.Lcd.fillRect(87, 141, 35, 24, GREEN);
  M5.Lcd.fillCircle(129, 154, 10, GREEN);
  M5.Lcd.fillRect(27, 141, 15, 40, GREEN);
  M5.Lcd.fillCircle(32, 171, 10, GREEN);
  M5.Lcd.fillRect(40, 163, 30, 20, GREEN);
  M5.Lcd.setCursor(203, 50);
  M5.Lcd.print("Cactus");
  M5.Lcd.setCursor(157, 76);
  M5.Lcd.print("Comfort T: 20");

  M5.Lcd.setCursor(156, 102);
  M5.Lcd.print("comfort %: 45");

  M5.Lcd.setCursor(154, 128);
  M5.Lcd.print("comfort P: 28");

  M5.Lcd.setCursor(154, 153);
  M5.Lcd.print("description:");

  M5.Lcd.setCursor(157, 175);
  M5.Lcd.print("long roots;");

  M5.Lcd.setCursor(154, 193);
  M5.Lcd.print("spines");


  M5.Lcd.setCursor(153, 215);
  M5.Lcd.print("stem is thick");
  // usunięcie wszystkich zarejestrowanych funkcji dla przycisku lb
  lt.delHandlers();
  lb.delHandlers();
  buttonsEnabled = true;
}


void setup() {
  M5.begin();
  setupTime();

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(BLACK);
  lastWateringDay = EEPROM.read(0);
  EEPROM.get(1, lastWateringTime);
  Serial.print("Last watering time: ");
  Serial.print(lastWateringDay);
  Serial.print(", ");
  Serial.println(lastWateringTime);
  SD.begin(TFCARD_CS_PIN, SPI, 40000000);
  Wire.begin();
  qmp6988.init();


  // dataFile = SD.open("/data2.txt", FILE_WRITE);
  // if (dataFile) {
  //   dataFile.println("Temperature, Humidity, Pressure");
  //   dataFile.close();
  // }
  // float sum_temperature = 0.0;
  // float sum_pressure = 0.0;
  // float sum_humidity = 0.0;
  // float sum_soil_moisture = 0.0;
  // int count = 0;
  // File dataFile = SD.open("/danePom.txt", FILE_READ);
  // if (dataFile) {
  //   while (dataFile.available()) {
  //     String line = dataFile.readStringUntil('\n');
  //     if (line.startsWith("Temperatura: ")) {
  //       sum_temperature += line.substring(14).toFloat();
  //     } else if (line.startsWith("Cisnienie: ")) {
  //       sum_pressure += line.substring(12).toFloat();
  //     } else if (line.startsWith("Wilgotnosc: ")) {
  //       sum_humidity += line.substring(13).toFloat();
  //     } else if (line.startsWith("Wilgotnosc gleby: ")) {
  //       sum_soil_moisture += line.substring(19).toFloat();
  //     }
  //     count++;
  //   }
  //   dataFile.close();
  //   if (count > 0) {
  //      avg_temperature = sum_temperature / count;
  //      avg_pressure = sum_pressure / count;
  //      avg_humidity = sum_humidity / count;
  //      avg_soil_moisture = sum_soil_moisture / count;
  //   }
  // } 
  //   else {
  //     M5.Lcd.println("Brak danych w pliku.");
  //   }
  // } else {
  //   M5.Lcd.println("Blad odczytu pliku.");
  // }
  pinMode(INPUT_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(25, OUTPUT);
  digitalWrite(25, 0);
  lb.addHandler(lbClick, E_CLICK);
  lt.addHandler(ltClick, E_CLICK);
  M5.Lcd.drawString("Choose plant", 100, 16);
  M5.Lcd.drawString("Ficus", 47, 47);
  M5.Lcd.drawString("Cactus", 210, 49);
  M5.Lcd.drawJpgFile(SD, "/cactus.jpg", 187, 102);
  M5.Lcd.drawJpgFile(SD, "/ficus.jpg", 24, 102);
}

void loop() {
  M5.update();
  //  flushTime();
  now = millis();
  if (now - last > 1000) {
    last = now;
    pressure = qmp6988.calcPressure();
    if (sht30.get() == 0) {       //Obtain the data of shT30
      temperature = sht30.cTemp;  //Store the temperature obtained from shT30
      humidity = sht30.humidity;  //Store the humidity obtained from the SHT30
    } else {
      temperature = 0.0, humidity = 0.0;
    }


    rawADC = analogRead(INPUT_PIN);
    if (rawADC < SOIL_MOISTURE_THRESHOLD) {
      digitalWrite(PUMP_PIN, HIGH);
      lastWateringDay = RTCDate.Date;
      lastWateringTime = millis();
      EEPROM.write(0, lastWateringDay);
      EEPROM.put(1, lastWateringTime);
      EEPROM.commit();
      Serial.print("Watering time saved: ");
      Serial.print(lastWateringDay);
      Serial.print(", ");
      Serial.println(lastWateringTime);
      pumpStartTime = millis();
      pumpRunning = true;
      // }
      //   dataFile.print("Wilgotnosc gleby: ");
      //   dataFile.print(rawADC);
      //   dataFile.print(" %\n");
      //   dataFile.close();
    }


    //char filename[20];
    //sprintf(filename, "/data_%lu.txt", last);
    //dataFile = SD.open(filename, FILE_WRITE);

    //zapis do pliku
    // dataFile = SD.open("/danePom.txt", FILE_WRITE);

    // if (dataFile) {
    //   dataFile.print("Temperatura: ");
    //   dataFile.print(temperature);
    //   dataFile.print(" C\n");
    //   dataFile.print("Cisnienie: ");
    //   dataFile.print(pressure);
    //   dataFile.print(" hPa\n");
    //   dataFile.print("Wilgotnosc: ");
    //   dataFile.print(humidity);
    //   dataFile.print(" %\n");
    //   dataFile.print("Wilgotnosc gleby: ");
    //   dataFile.print(rawADC);
    //   dataFile.print(" %\n");
    //   dataFile.close();
    // }
  }
  // Przechodzenie do następnego ekranu za pomocą przycisku A
  if (buttonsEnabled) {
    if (M5.BtnB.wasPressed()) {
      digitalWrite(PUMP_PIN, flag);
      lastWateringDay = RTCDate.Date;
      lastWateringTime = millis();
      EEPROM.write(0, lastWateringDay);
      EEPROM.put(1, lastWateringTime);
      EEPROM.commit();
      Serial.print("Watering time saved: ");
      Serial.print(lastWateringDay);
      Serial.print(", ");
      Serial.println(lastWateringTime);
      flag = !flag;
    }
    // nowPump = millis();
    // if (nowPump - lastPump > SOIL_MOISTURE_INTERVAL) {
    //   rawADC = analogRead(INPUT_PIN);
    //   if (rawADC < SOIL_MOISTURE_THRESHOLD) {
    //     digitalWrite(PUMP_PIN, HIGH);
    //     lastWateringDay = RTCDate.Date;
    //     lastWateringTime = millis();
    //     EEPROM.write(0, lastWateringDay);
    //     EEPROM.put(1, lastWateringTime);
    //     EEPROM.commit();
    //     Serial.print("Watering time saved: ");
    //     Serial.print(lastWateringDay);
    //     Serial.print(", ");
    //     Serial.println(lastWateringTime);
    //     pumpStartTime = millis();
    //     pumpRunning = true;
    //   }
    //   lastPump = nowPump;
    //   dataFile.print("Wilgotnosc gleby: ");
    //   dataFile.print(rawADC);
    //   dataFile.print(" %\n");
    //   dataFile.close();
    // }
    // if (pumpRunning && millis() - pumpStartTime >= 10000) {
          if (pumpRunning && millis() - pumpStartTime >= 5000) {
      digitalWrite(PUMP_PIN, LOW);  // wyłączamy pompę
      pumpRunning = false;          // ustawiamy flagę, że pompa jest wyłączona
    }
    switch (screenNum) {
      case 0:
        if (choose == 1) {
          page1a();
        } else if (choose = 2) {
          page1b();
        }
        break;
      case 1:
        page2();
        break;
      case 2:
        page3();
        break;
      case 3:
        page4();
        break;
      case 4:
        page5();
        break;
    }
    if (M5.BtnA.wasPressed()) {
      screenNum++;
      if (screenNum > 4) {
        screenNum = 0;
      }
      refresh = 1;
    }

    // Przechodzenie do poprzedniego ekranu za pomocą przycisku C
    if (M5.BtnC.wasPressed()) {
      screenNum--;
      if (screenNum < 0) {
        screenNum = 4;
      }
      refresh = 1;
    }
  }
}
void page1a() {

  clearScreen();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);

  M5.Lcd.drawJpgFile(SD, "/ficus.jpg", 31, 84);

  M5.Lcd.setCursor(203, 50);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Ficus");
  M5.Lcd.setCursor(157, 76);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Comfort T: 23");

  M5.Lcd.setCursor(156, 102);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("comfort %: 75");

  M5.Lcd.setCursor(154, 128);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("comfort P: 60");

  M5.Lcd.setCursor(154, 153);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("description:");

  M5.Lcd.setCursor(157, 175);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("likes humidit");

  M5.Lcd.setCursor(154, 193);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("good lighting");


  M5.Lcd.setCursor(153, 215);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("shiny leaves");
}

void page1b() {

  clearScreen();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.fillRect(66, 96, 20, 120, GREEN);
  M5.Lcd.fillCircle(75, 91, 15, GREEN);
  M5.Lcd.fillRect(122, 96, 15, 50, GREEN);
  M5.Lcd.fillRect(87, 141, 35, 24, GREEN);
  M5.Lcd.fillCircle(129, 154, 10, GREEN);
  M5.Lcd.fillRect(27, 141, 15, 40, GREEN);
  M5.Lcd.fillCircle(32, 171, 10, GREEN);
  M5.Lcd.fillRect(40, 163, 30, 20, GREEN);
  M5.Lcd.setCursor(203, 50);

  M5.Lcd.print("Cactus");
  M5.Lcd.setCursor(157, 76);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Comfort T: 20");
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(156, 102);
  M5.Lcd.print("comfort %: 45");
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(154, 128);
  M5.Lcd.print("comfort P: 28");
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(154, 153);
  M5.Lcd.print("description:");

  M5.Lcd.setCursor(157, 175);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("long roots;");
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(154, 193);
  M5.Lcd.print("spines");

  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(153, 215);
  M5.Lcd.print("stem is thick");
}
void page2() {

  clearScreen();

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  if (temperature >= 10 && temperature <= 20) {

    M5.Lcd.drawJpgFile(SD, "/a.jpg", 29, 16);
    M5.Lcd.drawJpgFile(SD, "/b.jpg", 171, 13);
    M5.Lcd.setTextColor(WHITE, BLACK);  // Ustaw kolor tekstu na biały
    M5.Lcd.setTextSize(3);              // Ustaw rozmiar czcionki
    M5.Lcd.setCursor(20, 140);          // Ustaw pozycję kursora
    M5.Lcd.setTextColor(WHITE, BLACK);

    M5.Lcd.printf("Temper:%.1f C", temperature);  // Wypisz tekst na ekranie

    M5.Lcd.setCursor(20, 170);  // Ustaw pozycję kursora
    M5.Lcd.setTextColor(WHITE, BLACK);

    M5.Lcd.printf("Pressure:%.1f", pressure / 100);  // Wypisz tekst na ekranie
    M5.Lcd.setCursor(20, 193);                       // Ustaw pozycję kursora
    M5.Lcd.setTextColor(WHITE, BLACK);

    M5.Lcd.printf("Humadity:%.1f%%", humidity);  // Wypisz tekst na ekranie
  } else {
    M5.Lcd.setTextColor(WHITE, BLACK);  // Ustaw kolor tekstu na biały
    M5.Lcd.setTextSize(3);              // Ustaw rozmiar czcionki

    M5.Lcd.drawJpgFile(SD, "/c.jpg", 29, 16);
    M5.Lcd.drawJpgFile(SD, "/d.jpg", 171, 13);



    M5.Lcd.setCursor(29, 140);  // Ustaw pozycję kursora
    M5.Lcd.setTextColor(WHITE, BLACK);

    M5.Lcd.printf("Temper: %.1f C", temperature);  // Wypisz tekst na ekranie
    M5.Lcd.setCursor(29, 170);                     // Ustaw pozycję kursora
    M5.Lcd.setTextColor(WHITE, BLACK);

    M5.Lcd.printf("Pressure: %.1f", pressure / 100);  // Wypisz tekst na ekranie
    M5.Lcd.setCursor(29, 193);                        // Ustaw pozycję kursora
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.printf("Humadity: %.1f%%", humidity);
  }
}
void page3() {
  clearScreen();
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawLine(37, 118, 261, 118, WHITE);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(24, 24);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Last water");
  M5.Lcd.setCursor(84, 76);
  M5.Lcd.setTextColor(WHITE, BLACK);

  M5.Lcd.print(lastWateringDay);
  M5.Lcd.print("/");
  M5.Lcd.setTextColor(WHITE, BLACK);

  M5.Lcd.print(RTCDate.Month);
  M5.Lcd.print("/");
  M5.Lcd.setTextColor(WHITE, BLACK);

  M5.Lcd.print(RTCDate.Year);
  M5.Lcd.print(" ");
  M5.Lcd.setTextColor(WHITE, BLACK);

  //M5.Lcd.print(lastWateringTime / 1000 / 60 / 60);
  //  M5.Lcd.print(":");
  M5.Lcd.setTextColor(WHITE, BLACK);

  //M5.Lcd.print((lastWateringTime / 1000 / 60) % 60);

  M5.Lcd.fillCircle(39, 91, 15, BLUE);
  M5.Lcd.fillRect(31, 52, 15, 30, BLUE);



  M5.Lcd.fillRect(23, 156, 30, 30, GREEN);



  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(17, 134);
  M5.Lcd.print("Soil moisture");
  M5.Lcd.setCursor(76, 175);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print(String(rawADC));
  M5.Lcd.fillRect(24, 180, 10, 30, BLUE);
  M5.Lcd.fillRect(44, 180, 10, 30, YELLOW);
}
void page4() {

  clearScreen();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(BLACK, WHITE);
  M5.Lcd.drawLine(14, 118, 313, 119, BLACK);
  M5.Lcd.drawLine(150, 237, 150, 5, BLACK);

  M5.Lcd.drawJpgFile(SD, "/a1.jpg", 43, 11);
  M5.Lcd.setTextSize(2);              // Ustaw rozmiar czcionki
  M5.Lcd.setCursor(12, 53);           // Ustaw pozycję kursora
  M5.Lcd.setTextColor(BLACK, WHITE);  // Ustaw kolor tekstu na biały

//M5.Lcd.print(avg_temperature);
M5.Lcd.print("24.3 ");              // Wypisz tekst na ekranie


  M5.Lcd.drawJpgFile(SD, "/b1.jpg", 156, 11);
  M5.Lcd.setTextColor(BLACK, WHITE);  // Ustaw kolor tekstu na biały
  M5.Lcd.setTextSize(2);              // Ustaw rozmiar czcionki
  M5.Lcd.setCursor(269, 52);          // Ustaw pozycję kursora
  M5.Lcd.setTextColor(BLACK, WHITE);  // Ustaw kolor tekstu na biały
  //M5.Lcd.print(avg_soil_moisture);
  M5.Lcd.print("1600");               // Wypisz tekst na ekranie

  M5.Lcd.drawJpgFile(SD, "/c1.jpg", 39, 123);
  M5.Lcd.setTextSize(2);              // Ustaw rozmiar czcionki
  M5.Lcd.setCursor(0, 165);           // Ustaw pozycję kursora
  
  M5.Lcd.setTextColor(BLACK, WHITE);  // Ustaw kolor tekstu na biały

  //M5.Lcd.print(avg_pressure);
  M5.Lcd.print("978.0");              // Wypisz tekst na ekranie

  M5.Lcd.drawJpgFile(SD, "/d1.jpg", 156, 123);
  M5.Lcd.setTextColor(BLACK, WHITE);  // Ustaw kolor tekstu na biały
  M5.Lcd.setTextSize(2);              // Ustaw rozmiar czcionki
  M5.Lcd.setCursor(260, 165);         // Ustaw pozycję kursora
  //M5.Lcd.print(avg_humidity);
  M5.Lcd.print("43.0");               // Wypisz tekst na ekranie
}
void page5() {
  clearScreen();
  if (temperature >= 10 && temperature <= 20) {
    M5.Lcd.setTextColor(BLACK, GREEN);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(5, 100);
    M5.Lcd.printf("Temperat w normie:%.1fC", temperature);
  } else if (temperature >= 21 && temperature <= 25) {
    M5.Lcd.setTextColor(BLACK, ORANGE);
    M5.Lcd.setCursor(5, 100);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("Temperat lekko przekroczona:%.1fC", temperature);
  } else {
    M5.Lcd.setTextColor(BLACK, RED);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(5, 100);
    M5.Lcd.printf("Temperat bardzo wysoka:%.1fC", temperature);
  }
}
void clearScreen() {
  if (refresh == 1) {
    if (screenNum == 3) {
      M5.Lcd.fillScreen(WHITE);
    } else if (screenNum == 4) {
      if (temperature >= 10 && temperature <= 20) {
        M5.Lcd.fillScreen(GREEN);
      } else if (temperature >= 21 && temperature <= 25) {
        M5.Lcd.fillScreen(ORANGE);

      } else {
        M5.Lcd.fillScreen(RED);
      }
    } else {
      M5.Lcd.clear();
    }
    refresh = 0;
  }
}
