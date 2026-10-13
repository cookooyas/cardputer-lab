
#include <M5Cardputer.h>

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(BLACK);

    M5Cardputer.Display.setTextColor(GREEN);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(10, 30);

    M5Cardputer.Display.println("HELLO");
    M5Cardputer.Display.println("CARDPUTER!");
}

void loop() {
    delay(100);
}
