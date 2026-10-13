
#include <M5Cardputer.h>

// 현재 선택한 화면 상태
char currentKey = 'A';

void drawScreen(uint32_t color, const char* name) {
    // 화면 배경색 변경
    M5Cardputer.Display.fillScreen(color);

    // 텍스트 출력
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setTextSize(2);

    M5Cardputer.Display.setCursor(15, 15);
    M5Cardputer.Display.println("CARDPUTER DEMO");

    M5Cardputer.Display.setTextSize(3);
    M5Cardputer.Display.setCursor(40, 55);
    M5Cardputer.Display.println(name);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(15, 115);
    M5Cardputer.Display.println("Press A / B / C");
}

void setup() {
    auto cfg = M5.config();

    // 키보드 활성화
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setRotation(1);

    // 처음에는 초록색
    drawScreen(DARKGREEN, "GREEN");
}

void loop() {
    // 키보드 상태 갱신
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed()) {

        if (M5Cardputer.Keyboard.isKeyPressed('a') &&
            currentKey != 'A') {
            currentKey = 'A';
            drawScreen(0x00FF00, "GREEN");
        }

        else if (M5Cardputer.Keyboard.isKeyPressed('b') &&
                 currentKey != 'B') {
            currentKey = 'B';
            drawScreen(0x0000FF, "BLUE");
        }

        else if (M5Cardputer.Keyboard.isKeyPressed('c') &&
                 currentKey != 'C') {
            currentKey = 'C';
            drawScreen(0xFF0000, "RED");
        }
    }

    delay(10);
}
