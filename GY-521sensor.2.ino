// **GY-521 (MPU-6050) 센서 및 SD 카드 로깅 프로그램 (개선판)**
// 3축 자이로 및 3축 가속도 데이터를 읽어 SD 카드에 CSV 형식으로 저장합니다.
// 로켓 비행 데이터 기록에 적합합니다.

#include <Wire.h> // I2C 통신 라이브러리 (GY-521 센서용: SDA=A4, SCL=A5)
#include <SD.h>   // SD 카드 통신 라이브러리 (SPI: CS=D10, MOSI=D11, MISO=D12, SCK=D13)

// SD 카드 칩 셀렉트 (CS) 핀 설정
const int chipSelect = 10;

// GY-521 센서의 I2C 주소 (AD0 핀이 GND에 연결되었을 때 기본 주소)
const int MPU_ADDRESS = 0x68;

// 센서 값 저장을 위한 변수 (Raw Data)
int accX, accY, accZ;     // 가속도 (Accelerometer)
int gyroX, gyroY, gyroZ;  // 자이로 (Gyroscope)
String dataString = "";

// SD 카드 파일 객체 (전역으로 선언)
File dataFile;
unsigned long lastFlush = 0;
const unsigned long FLUSH_INTERVAL = 100; // 100ms마다 flush

// ------------------------------------------------------------------
// GY-521 (MPU-6050) 초기화 함수
// ------------------------------------------------------------------
void MPU6050_Init() {
  // 1. 센서 깨우기 (Power Management Register 0x6B)
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0x00); // 클럭 소스: 자이로 X축, 슬립 모드 해제
  Wire.endTransmission(true);
  delay(10);
  
  // 2. 자이로 설정 (Gyro Config Register 0x1B)
  // 0x00: +/- 250 deg/s (가장 정밀한 범위)
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x1B); // GYRO_CONFIG register
  Wire.write(0x00);
  Wire.endTransmission(true);
  delay(10);
  
  // 3. 가속도 설정 (Accel Config Register 0x1C)
  // 0x00: +/- 2g (가장 정밀한 범위)
  // 로켓의 높은 가속도가 필요하면:
  // 0x08: +/- 4g
  // 0x10: +/- 8g
  // 0x18: +/- 16g
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x1C); // ACCEL_CONFIG register
  Wire.write(0x00); // 0x00은 +/- 2g 설정
  Wire.endTransmission(true);
  delay(10);
  
  Serial.println("✓ GY-521 센서 초기화 완료. (자이로 +/- 250dps, 가속도 +/- 2g)");
}

// ------------------------------------------------------------------
// GY-521 진단 함수 (센서 통신 상태 확인)
// ------------------------------------------------------------------
void diagnosticTest() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║      GY-521 센서 진단 시작             ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  // I2C 통신 테스트
  Wire.beginTransmission(MPU_ADDRESS);
  int error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.println("✓ GY-521 센서 I2C 통신 성공");
    
    // WHO_AM_I 레지스터 확인 (0x75, 기본값: 0x68)
    Wire.beginTransmission(MPU_ADDRESS);
    Wire.write(0x75);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDRESS, 1, true);
    
    if (Wire.available()) {
      byte whoAmI = Wire.read();
      Serial.print("✓ WHO_AM_I 레지스터 값: 0x");
      Serial.println(whoAmI, HEX);
      
      if (whoAmI == 0x68) {
        Serial.println("✓ MPU-6050 센서 정상 인식!");
      } else {
        Serial.println("⚠ 센서 응답이 예상과 다릅니다.");
      }
    }
    
    // 간단한 데이터 읽기 테스트
    Serial.println("\n테스트 데이터 읽기...");
    readAllData();
    Serial.print("  AccX: "); Serial.print(accX);
    Serial.print(" | AccY: "); Serial.print(accY);
    Serial.print(" | AccZ: "); Serial.println(accZ);
    Serial.print("  GyroX: "); Serial.print(gyroX);
    Serial.print(" | GyroY: "); Serial.print(gyroY);
    Serial.print(" | GyroZ: "); Serial.println(gyroZ);
    
  } else {
    Serial.print("✗ I2C 통신 실패 (에러 코드: ");
    Serial.print(error);
    Serial.println(")");
    Serial.println("   → 센서 연결을 확인하세요 (SDA=A4, SCL=A5)");
  }
  
  Serial.println("╚════════════════════════════════════════╝\n");
}

// ------------------------------------------------------------------
// GY-521 모든 데이터 (가속도 + 자이로) 읽기 함수
// ------------------------------------------------------------------
void readAllData() {
  // 가속도 X축 High Byte 레지스터 시작 주소 (0x3B)로 전송 준비
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x3B); // ACCEL_XOUT_H (가속도 X축 High Byte) 주소
  int error = Wire.endTransmission(false); // 재시작을 위해 false로 설정
  
  if (error != 0) {
    Serial.print("⚠ I2C 전송 오류: ");
    Serial.println(error);
    dataString = String(millis()) + ",ERROR,ERROR,ERROR,ERROR,ERROR,ERROR";
    accX = 0; accY = 0; accZ = 0;
    gyroX = 0; gyroY = 0; gyroZ = 0;
    return;
  }
  
  // 가속도(X, Y, Z), 온도(Temp), 자이로(X, Y, Z) 값 (총 14바이트) 요청
  Wire.requestFrom(MPU_ADDRESS, 14, true);
  delay(2); // I2C 안정화 대기
  
  if (Wire.available() >= 14) {
    // 가속도 (총 6바이트)
    accX = Wire.read() << 8 | Wire.read();
    accY = Wire.read() << 8 | Wire.read();
    accZ = Wire.read() << 8 | Wire.read();
    
    // 온도 데이터 (총 2바이트) - 여기서는 건너뜀
    Wire.read();
    Wire.read();
    
    // 자이로 (총 6바이트)
    gyroX = Wire.read() << 8 | Wire.read();
    gyroY = Wire.read() << 8 | Wire.read();
    gyroZ = Wire.read() << 8 | Wire.read();
    
    // 저장할 문자열 생성 (CSV 형식)
    dataString = String(millis()) + "," +
                 String(accX) + "," + String(accY) + "," + String(accZ) + "," +
                 String(gyroX) + "," + String(gyroY) + "," + String(gyroZ);
  } else {
    // 데이터 수신 실패 시 에러 로깅
    Serial.print("⚠ I2C 데이터 수신 오류: ");
    Serial.print(Wire.available());
    Serial.println(" bytes available (expected 14)");
    
    dataString = String(millis()) + ",ERROR,ERROR,ERROR,ERROR,ERROR,ERROR";
    accX = 0; accY = 0; accZ = 0;
    gyroX = 0; gyroY = 0; gyroZ = 0;
  }
}

// ------------------------------------------------------------------
// SD 카드에 데이터 기록 함수 (버퍼링 사용)
// ------------------------------------------------------------------
void logToSDCard() {
  if (dataFile) {
    dataFile.println(dataString);
    
    // 일정 시간마다 flush (데이터 손상 방지)
    if (millis() - lastFlush > FLUSH_INTERVAL) {
      dataFile.flush();
      lastFlush = millis();
    }
  } else {
    Serial.println("⚠ 데이터 파일이 열려있지 않습니다!");
  }
}

// ------------------------------------------------------------------
// SD 카드 파일 닫기 함수
// ------------------------------------------------------------------
void closeDataFile() {
  if (dataFile) {
    dataFile.flush(); // 남은 데이터 모두 쓰기
    dataFile.close();
    Serial.println("✓ 데이터 파일 저장 완료.");
  }
}

// ------------------------------------------------------------------
// Setup 함수
// ------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   아두이노 GY-521 통합 로거 시작      ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // I2C 통신 시작 (A4, A5 핀 사용)
  Wire.begin();
  delay(100);
  
  // GY-521 센서 진단 및 초기화
  diagnosticTest();
  MPU6050_Init();
  
  delay(500);
  
  // SD 카드 초기화 (SPI)
  Serial.println("SD 카드 초기화 중...");
  pinMode(chipSelect, OUTPUT);
  
  if (!SD.begin(chipSelect)) {
    Serial.println("✗ SD 카드 초기화 실패!");
    Serial.println("  → SD 카드 연결을 확인하세요 (CS=D10, MOSI=D11, MISO=D12, SCK=D13)");
    Serial.println("  → SD 카드가 FAT32로 포맷되어 있는지 확인하세요.");
    while (1); // 무한 대기
  } else {
    Serial.println("✓ SD 카드 초기화 성공.\n");
    
    // 파일이 없을 때만 헤더 작성
    if (!SD.exists("GY521_LOG.TXT")) {
      dataFile = SD.open("GY521_LOG.TXT", FILE_WRITE);
      if (dataFile) {
        dataFile.println("Time(ms),AccX(Raw),AccY(Raw),AccZ(Raw),GyroX(Raw),GyroY(Raw),GyroZ(Raw)");
        dataFile.close();
        Serial.println("✓ 새 로그 파일 생성 및 헤더 작성 완료.");
      } else {
        Serial.println("✗ 헤더 파일 생성 실패!");
      }
    } else {
      Serial.println("ℹ 기존 로그 파일이 있습니다. (데이터 추가 모드)");
    }
    
    // 데이터 기록용 파일 열기
    dataFile = SD.open("GY521_LOG.TXT", FILE_WRITE);
    if (dataFile) {
      Serial.println("✓ 로그 파일 준비 완료.\n");
      Serial.println("╔════════════════════════════════════════╗");
      Serial.println("║     데이터 기록 시작 (50ms 간격)       ║");
      Serial.println("╚════════════════════════════════════════╝\n");
    } else {
      Serial.println("✗ 로그 파일 열기 실패!");
      while (1); // 무한 대기
    }
  }
  
  lastFlush = millis();
}

// ------------------------------------------------------------------
// Loop 함수
// ------------------------------------------------------------------
void loop() {
  // 1. 센서 데이터 읽기
  readAllData();
  
  // 2. 시리얼 모니터에 출력
  Serial.print("[");
  Serial.print(millis());
  Serial.print("ms] Acc(X,Y,Z): ");
  Serial.print(accX); Serial.print(",");
  Serial.print(accY); Serial.print(",");
  Serial.print(accZ); Serial.print(" | Gyro(X,Y,Z): ");
  Serial.print(gyroX); Serial.print(",");
  Serial.print(gyroY); Serial.print(",");
  Serial.println(gyroZ);
  
  // 3. SD 카드에 데이터 기록
  logToSDCard();
  
  // 데이터 기록 간격
  // 50ms: 초당 20회 기록
  // 로켓의 높은 가속도를 정확히 기록하려면 더 짧은 간격이 필요할 수 있습니다.
  delay(50);
  
  // ※ 특정 조건에서 데이터 기록 종료:
  // closeDataFile();
  // while(1); // 프로그램 종료
}