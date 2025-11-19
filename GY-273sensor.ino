// ** 아두이노 나노/우노용 3축 센서 및 SD 카드 로깅 프로그램 **
// GY-273 (HMC5883L 자이로/자기장 센서)의 3축 데이터를 읽고
// 시리얼 모니터에 출력하며 SD 카드에 저장합니다.

#include <Wire.h> // I2C (Inter-Integrated Circuit) 통신 라이브러리 (GY-273 센서용)
#include <SD.h>   // SD 카드 통신 라이브러리 (SPI)

// SD 카드 어댑터의 칩 셀렉트 (Chip Select, CS) 핀 설정
// 아두이노 나노/우노에서 10번 핀은 SD 카드의 CS 핀으로 일반적으로 사용됩니다.
const int chipSelect = 10;

// GY-273 센서의 I2C 주소 (HMC5883L의 일반적인 주소)
const int MPU_ADDRESS = 0x1E; // 0x1E는 HMC5883L의 주소입니다.

// 센서 값 저장을 위한 변수
int magX, magY, magZ;
String dataString = "";

// ------------------------------------------------------------------
// 센서 데이터 읽기 함수 (GY-273 / HMC5883L 기준)
// 이 함수는 간단한 HMC5883L 센서의 데이터 읽기를 흉내냅니다.
// 실제 사용 시에는 전문 라이브러리(예: Adafruit HMC5883L)를 사용하는 것이 좋습니다.
// ------------------------------------------------------------------
void readSensorData() {
  // 1. I2C 통신을 통해 센서에 데이터 읽기 요청
  Wire.beginTransmission(MPU_ADDRESS);
  // HMC5883L의 데이터 레지스터 시작 주소 (Data Register Start Address)
  // X축, Z축, Y축 순으로 읽는 것이 일반적입니다.
  Wire.write(0x03);
  Wire.endTransmission();

  // 2. 센서로부터 6바이트 (X, Z, Y 각각 2바이트씩) 데이터 수신
  // HMC5883L은 6바이트의 16비트 데이터를 전송합니다.
  Wire.requestFrom(MPU_ADDRESS, 6);

  if (Wire.available() == 6) {
    // 3. 수신된 데이터를 16비트 정수 (High Byte, Low Byte)로 변환
    // X축
    magX = Wire.read() << 8; // High Byte
    magX |= Wire.read();     // Low Byte

    // Z축 (HMC5883L은 Z축이 중간에 위치함)
    magZ = Wire.read() << 8; // High Byte
    magZ |= Wire.read();     // Low Byte

    // Y축
    magY = Wire.read() << 8; // High Byte
    magY |= Wire.read();     // Low Byte

    // 4. 저장할 문자열 생성 (시간, X, Y, Z 순서)
    dataString = String(millis()) + "," + String(magX) + "," + String(magY) + "," + String(magZ);

  } else {
    // 데이터 수신 실패 시 기본값 설정
    dataString = String(millis()) + ",ERROR,ERROR,ERROR";
    magX = 0; magY = 0; magZ = 0; // 시리얼 모니터 출력을 위한 임시 값
  }
}

// ------------------------------------------------------------------
// SD 카드에 데이터 기록 함수
// ------------------------------------------------------------------
void logToSDCard() {
  // 1. 파일 열기 (FILE_WRITE 모드로)
  // 파일이 없으면 생성되고, 있으면 끝에 추가됩니다.
  File dataFile = SD.open("LOGDATA.TXT", FILE_WRITE);

  // 2. 파일 쓰기
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close(); // 파일 닫기
  }
  // 3. 파일 쓰기 실패 시 에러 출력
  else {
    Serial.println("SD 카드 파일 쓰기 실패!");
  }
}


void setup() {
  // 시리얼 통신 시작 (9600 bps)
  Serial.begin(9600);
  while (!Serial); // 시리얼 포트가 열릴 때까지 대기 (특히 나노 보드에서는 필요할 수 있음)
  Serial.println("=== 아두이노 센서 로거 시작 ===");

  // I2C 통신 시작 (GY-273 센서용)
  Wire.begin();
  Serial.println("I2C 통신 초기화 완료.");

  // SD 카드 초기화 (SPI)
  // 1. CS 핀 (D10)을 출력으로 설정
  pinMode(chipSelect, OUTPUT);

  // 2. SD 카드 초기화 시도
  if (!SD.begin(chipSelect)) {
    Serial.println("SD 카드 초기화 실패! (카드를 확인하세요)");
    // SD 카드 없이도 시리얼 모니터 출력은 계속되도록 setup()을 종료하지 않습니다.
  } else {
    Serial.println("SD 카드 초기화 성공.");

    // 파일 헤더 작성
    File dataFile = SD.open("LOGDATA.TXT", FILE_WRITE);
    if (dataFile) {
      // Time(ms), X, Y, Z 순서로 헤더를 작성합니다.
      dataFile.println("Time(ms),MagX,MagY,MagZ");
      dataFile.close();
      Serial.println("로그 파일 헤더 작성 완료.");
    }
  }
}


void loop() {
  // 1. 센서 데이터 읽기
  readSensorData();

  // 2. 시리얼 모니터에 출력
  // magX, magY, magZ 변수에 저장된 센서의 16비트 정수 값 출력
  Serial.print("센서 값 (X, Y, Z): ");
  Serial.print(magX);
  Serial.print(", ");
  Serial.print(magY);
  Serial.print(", ");
  Serial.println(magZ);

  // 3. SD 카드에 데이터 기록
  // dataString 변수에 저장된 CSV 형식의 문자열을 기록합니다.
  logToSDCard();

  // 데이터 기록 간격 조절 (예: 500ms마다 기록)
  delay(500);
}
