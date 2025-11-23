// ** GY-521 (MPU-6050) 센서 및 SD 카드 로깅 프로그램 **
// 3축 자이로 및 3축 가속도 데이터를 읽어 SD 카드에 CSV 형식으로 저장합니다.
// 로켓 비행 데이터 기록에 적합합니다.

#include <Wire.h> // I2C 통신 라이브러리 (GY-521 센서용: SDA=A4, SCL=A5)
#include <SD.h>   // SD 카드 통신 라이브러리 (SPI: CS=D10, MOSI=D11, MISO=D12, SCK=D13)

// SD 카드 칩 셀렉트 (CS) 핀 설정
const int chipSelect = 10;

// GY-521 센서의 I2C 주소 (AD0 핀이 GND에 연결되었을 때 기본 주소)
const int MPU_ADDRESS = 0x68;

// 센서 값 저장을 위한 변수 (Raw Data)
int accX, accY, accZ; // 가속도 (Accelerometer)
int gyroX, gyroY, gyroZ; // 자이로 (Gyroscope)
String dataString = "";

// ------------------------------------------------------------------
// GY-521 (MPU-6050) 초기화 함수
// ------------------------------------------------------------------
void MPU6050_Init() {
  // 1. 센서 깨우기 (Power Management Register 0x6B)
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0x00); // 클럭 소스: 자이로 X축, 슬립 모드 해제
  Wire.endTransmission(true);

  // 2. 자이로 설정 (Gyro Config Register 0x1B)
  // 0x00: +/- 250 deg/s (가장 정밀한 범위)
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x1B); // GYRO_CONFIG register
  Wire.write(0x00); 
  Wire.endTransmission(true);
  
  // 3. 가속도 설정 (Accel Config Register 0x1C)
  // 0x00: +/- 2g (가장 정밀한 범위. 로켓의 경우 0x08 (+/- 4g) 또는 0x10 (+/- 8g)이 필요할 수 있습니다.)
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x1C); // ACCEL_CONFIG register
  Wire.write(0x00); // 0x00은 +/- 2g 설정
  Wire.endTransmission(true);

  Serial.println("GY-521 센서 초기화 완료. (자이로 +/- 250dps, 가속도 +/- 2g)");
}

// ------------------------------------------------------------------
// GY-521 모든 데이터 (가속도 + 자이로) 읽기 함수
// ------------------------------------------------------------------
void readAllData() {
  // 가속도 X축 High Byte 레지스터 시작 주소 (0x3B)로 전송 준비
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x3B); // ACCEL_XOUT_H (가속도 X축 High Byte) 주소
  Wire.endTransmission(false); // 재시작을 위해 false로 설정

  // 가속도(X, Y, Z), 온도(Temp), 자이로(X, Y, Z) 값 (총 14바이트) 요청
  Wire.requestFrom(MPU_ADDRESS, 14, true);

  if (Wire.available() == 14) {
    // 가속도 (총 6바이트)
    accX = Wire.read() << 8 | Wire.read();
    accY = Wire.read() << 8 | Wire.read();
    accZ = Wire.read() << 8 | Wire.read();
    
    // 온도 데이터 (총 2바이트) - 여기서는 건너뜀 (read() 2번 호출)
    Wire.read(); 
    Wire.read(); 
    
    // 자이로 (총 6바이트)
    gyroX = Wire.read() << 8 | Wire.read();
    gyroY = Wire.read() << 8 | Wire.read();
    gyroZ = Wire.read() << 8 | Wire.read();

    // 4. 저장할 문자열 생성 (CSV 형식)
    dataString = String(millis()) + "," + 
                 String(accX) + "," + String(accY) + "," + String(accZ) + "," +
                 String(gyroX) + "," + String(gyroY) + "," + String(gyroZ);
  } else {
    // 데이터 수신 실패 시 에러 로깅
    dataString = String(millis()) + ",ERROR,ERROR,ERROR,ERROR,ERROR,ERROR";
    accX = 0; accY = 0; accZ = 0; 
    gyroX = 0; gyroY = 0; gyroZ = 0;
  }
}

// ------------------------------------------------------------------
// SD 카드에 데이터 기록 함수
// ------------------------------------------------------------------
void logToSDCard() {
  // 파일 열기 (FILE_WRITE 모드로, 끝에 덧붙여 씀)
  File dataFile = SD.open("GY521_LOG.TXT", FILE_WRITE);

  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close(); // 파일 닫기
  }
  else {
    Serial.println("SD 카드 파일 쓰기 실패!");
  }
}


void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println("=== 아두이노 GY-521 통합 로거 시작 ===");

  // I2C 통신 시작 (A4, A5 핀 사용)
  Wire.begin();

  // GY-521 초기화 및 설정
  MPU6050_Init();

  // SD 카드 초기화 (SPI)
  pinMode(chipSelect, OUTPUT);

  if (!SD.begin(chipSelect)) {
    Serial.println("SD 카드 초기화 실패! (카드를 확인하세요)");
  } else {
    Serial.println("SD 카드 초기화 성공.");

    // 파일 헤더 작성 (최초 1회)
    File dataFile = SD.open("GY521_LOG.TXT", FILE_WRITE);
    if (dataFile) {
      // Time(ms), AccX, AccY, AccZ, GyroX, GyroY, GyroZ 순서로 헤더를 작성합니다.
      dataFile.println("Time(ms),AccX(Raw),AccY(Raw),AccZ(Raw),GyroX(Raw),GyroY(Raw),GyroZ(Raw)");
      dataFile.close();
      Serial.println("로그 파일 헤더 작성 완료.");
    }
  }
}


void loop() {
  // 1. 센서 데이터 읽기
  readAllData();

  // 2. 시리얼 모니터에 출력
  Serial.print("Acc(X, Y, Z): ");
  Serial.print(accX); Serial.print(",");
  Serial.print(accY); Serial.print(",");
  Serial.print(accZ); Serial.print(" | Gyro(X, Y, Z): ");
  Serial.print(gyroX); Serial.print(",");
  Serial.print(gyroY); Serial.print(",");
  Serial.println(gyroZ);

  // 3. SD 카드에 데이터 기록
  logToSDCard();

  // 데이터 기록 간격 (로켓은 50ms (초당 20회) 이내로 빠르게 기록하는 것이 좋습니다.)
  delay(50);
}
