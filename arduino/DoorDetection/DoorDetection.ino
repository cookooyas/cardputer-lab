
#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>

#include "secrets.h"

// ====================================================
// DoorDetection v0.3
// Cardputer ADV - Real-time IMU Motion Monitor
// ====================================================

// Windows PC 주소로 변경
const char* SERVER_URL =
    "http://10.50.13.92:8080/api/events";

// 측정 및 화면 갱신 주기
constexpr uint32_t SAMPLE_MS = 20;
constexpr uint32_t DISPLAY_MS = 100;

// 감지 시작 임계값
constexpr float ACC_ON_G = 0.16f;
constexpr float GYRO_ON_DPS = 20.0f;

// 감지 종료 임계값 (시작보다 낮게 설정)
constexpr float ACC_OFF_G = 0.07f;
constexpr float GYRO_OFF_DPS = 7.0f;

// 이 시간 동안 안정 상태가 지속되면 감지 해제
constexpr uint32_t STABLE_MS = 500;

// 가속도 기준값 갱신 속도
constexpr float FILTER_ALPHA = 0.04f;

// 네트워크 재연결/전송 재시도
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t HTTP_RETRY_MS = 5000;

// ====================================================
// 상태
// ====================================================

bool detected = false;
bool stableCounting = false;
bool accelReady = false;

bool eventPending = false;

uint32_t lastSample = 0;
uint32_t lastDisplay = 0;
uint32_t stableSince = 0;
uint32_t lastWiFiAttempt = 0;
uint32_t lastHttpAttempt = 0;

float eventMotionG = 0.0f;

// 원본 가속도
float ax = 0, ay = 0, az = 0;

// 원본 각속도
float gx = 0, gy = 0, gz = 0;

// 천천히 움직이는 기준 가속도
float refX = 0, refY = 0, refZ = 0;

// 순간 가속도 변화량
float dx = 0, dy = 0, dz = 0;

float accelMotion = 0;
float gyroMotion = 0;

// 화면에 표시할 주요 방향
char accelDirection[3] = "--";
char gyroDirection[3] = "--";

const char* serverStatus = "IDLE";

// ====================================================
// 방향 계산
// ====================================================

void getDominantDirection(
    float x, float y, float z,
    char* output
) {
    float absX = fabsf(x);
    float absY = fabsf(y);
    float absZ = fabsf(z);

    char axis = 'X';
    float value = x;

    if (absY > absX && absY >= absZ) {
        axis = 'Y';
        value = y;
    } else if (absZ > absX && absZ > absY) {
        axis = 'Z';
        value = z;
    }

    output[0] = axis;
    output[1] = value >= 0 ? '+' : '-';
    output[2] = '\0';
}

// ====================================================
// 감지 상태 처리
// ====================================================

void startDetection() {
    detected = true;
    stableCounting = false;

    // 서버 이벤트 발생
    // 아직 보내지 못한 이벤트가 있다면 보존
    if (!eventPending) {
        eventMotionG = accelMotion;
        eventPending = true;
        lastHttpAttempt = millis() - HTTP_RETRY_MS;
    }

    Serial.printf(
        "[DETECTED] ACC=%.3f g, GYRO=%.1f dps\n",
        accelMotion,
        gyroMotion
    );
}

void stopDetection() {
    detected = false;
    stableCounting = false;

    Serial.println("[STABLE] Motion stopped");
}

// ====================================================
// IMU 측정
// ====================================================

void updateIMU() {
    if (!M5.Imu.update()) {
        return;
    }

    auto imu = M5.Imu.getImuData();

    ax = imu.accel.x;
    ay = imu.accel.y;
    az = imu.accel.z;

    gx = imu.gyro.x;
    gy = imu.gyro.y;
    gz = imu.gyro.z;

    // 최초 측정값으로 기준 초기화
    if (!accelReady) {
        refX = ax;
        refY = ay;
        refZ = az;
        accelReady = true;
        return;
    }

    // 현재 가속도와 느리게 움직이는 기준값 차이
    dx = ax - refX;
    dy = ay - refY;
    dz = az - refZ;

    // 가속도 변화량의 크기
    accelMotion = sqrtf(
        dx * dx +
        dy * dy +
        dz * dz
    );

    // 자이로 각속도의 크기
    gyroMotion = sqrtf(
        gx * gx +
        gy * gy +
        gz * gz
    );

    // 가장 크게 반응한 축 표시
    getDominantDirection(dx, dy, dz, accelDirection);
    getDominantDirection(gx, gy, gz, gyroDirection);

    // 가속도 기준값을 천천히 현재 자세에 맞춰 갱신
    refX += FILTER_ALPHA * dx;
    refY += FILTER_ALPHA * dy;
    refZ += FILTER_ALPHA * dz;

    // ----------------------------------------------
    // 감지 시작 조건
    // ----------------------------------------------
    bool moving =
        accelMotion >= ACC_ON_G ||
        gyroMotion >= GYRO_ON_DPS;

    // ----------------------------------------------
    // 감지 종료 조건
    // ----------------------------------------------
    bool stable =
        accelMotion <= ACC_OFF_G &&
        gyroMotion <= GYRO_OFF_DPS;

    uint32_t now = millis();

    if (!detected) {
        if (moving) {
            startDetection();
        }
        return;
    }

    // 이미 감지 중인 경우
    if (stable) {
        if (!stableCounting) {
            stableCounting = true;
            stableSince = now;
        }

        if (now - stableSince >= STABLE_MS) {
            stopDetection();
        }
    } else {
        stableCounting = false;
    }
}

// ====================================================
// Wi-Fi
// ====================================================

void updateWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    uint32_t now = millis();

    if (now - lastWiFiAttempt < WIFI_RETRY_MS) {
        return;
    }

    lastWiFiAttempt = now;

    Serial.println("[WIFI] Connecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ====================================================
// HTTP 전송
// ====================================================

bool sendMotionEvent(float motionG) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    WiFiClient client;
    HTTPClient http;

    http.setConnectTimeout(1000);
    http.setTimeout(1200);

    if (!http.begin(client, SERVER_URL)) {
        return false;
    }

    http.addHeader(
        "Content-Type",
        "application/json"
    );

    String body =
        "{\"deviceId\":\"cardputer-door-01\","
        "\"event\":\"MOTION_DETECTED\","
        "\"motionG\":" +
        String(motionG, 3) + "}";

    int code = http.POST(body);

    http.end();

    Serial.printf("[HTTP] Status: %d\n", code);

    return code >= 200 && code < 300;
}

void updateHTTP() {
    if (!eventPending) {
        return;
    }

    uint32_t now = millis();

    if (now - lastHttpAttempt < HTTP_RETRY_MS) {
        return;
    }

    lastHttpAttempt = now;

    if (WiFi.status() != WL_CONNECTED) {
        serverStatus = "WAIT WIFI";
        return;
    }

    serverStatus = "SENDING";

    if (sendMotionEvent(eventMotionG)) {
        eventPending = false;
        serverStatus = "SENT";
    } else {
        serverStatus = "RETRY";
    }
}

// ====================================================
// LCD
// ====================================================

void drawScreen() {
    auto &d = M5Cardputer.Display;

    d.fillScreen(BLACK);
    d.setCursor(4, 3);
    d.setTextSize(1);

    d.setTextColor(CYAN);
    d.println("DoorDetection v0.3");

    if (detected) {
        d.setTextColor(RED);
        d.println("STATE: DETECTED");
    } else {
        d.setTextColor(GREEN);
        d.println("STATE: STABLE");
    }

    d.setTextColor(WHITE);

    // 가속도 원본값
    d.printf("AX:%7.3f AY:%7.3f\n", ax, ay);
    d.printf("AZ:%7.3f g\n", az);

    // 각속도 원본값
    d.printf("GX:%7.1f GY:%7.1f\n", gx, gy);
    d.printf("GZ:%7.1f dps\n", gz);

    d.println("------------------------");

    // 순간 가속도 및 각속도
    d.printf("ACC DELTA: %.3f g\n", accelMotion);
    d.printf("GYRO MAG : %.1f dps\n", gyroMotion);

    d.printf(
        "DIR A:%s  G:%s\n",
        accelDirection,
        gyroDirection
    );

    d.println("------------------------");

    d.printf(
        "WiFi: %s\n",
        WiFi.status() == WL_CONNECTED ? "ON" : "OFF"
    );

    d.printf("HTTP: %s\n", serverStatus);
}

// ====================================================
// SETUP
// ====================================================

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    Serial.begin(115200);

    auto &d = M5Cardputer.Display;
    d.setRotation(1);
    d.fillScreen(BLACK);
    d.setCursor(10, 10);
    d.setTextColor(WHITE);
    d.println("Starting DoorDetection...");

    if (!M5.Imu.isEnabled()) {
        d.println("IMU NOT FOUND");
        while (true) {
            delay(1000);
        }
    }

    // Wi-Fi 연결은 비동기적으로 진행
    WiFi.mode(WIFI_STA);
    lastWiFiAttempt = millis() - WIFI_RETRY_MS;

    // 첫 번째 IMU 샘플에서 기준값 초기화
    accelReady = false;
}

// ====================================================
// LOOP
// ====================================================

void loop() {
    M5Cardputer.update();

    uint32_t now = millis();

    // 약 20ms마다 IMU 데이터 갱신 시도
    if (now - lastSample >= SAMPLE_MS) {
        lastSample = now;
        updateIMU();
    }

    // 네트워크 관리
    updateWiFi();
    updateHTTP();

    // 약 100ms마다 LCD 갱신
    if (now - lastDisplay >= DISPLAY_MS) {
        lastDisplay = now;
        drawScreen();
    }

    delay(1);
}
