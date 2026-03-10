#include "music_player_v2.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"

// Basic stub to show porting the interface
// Real ESP8266Audio FFT integration is complex to do cleanly in a single file without disrupting other audio tasks.
// Here we port the UI and give a visual placeholder.

void music_player_v2_setup() {
    drawMainBorderWithTitle("Advance Music Player");
    tft.setTextSize(FP);
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawCentreString("FFT Visualizer (Ported UI)", tftWidth / 2, 40, 1);
    
    tft.drawRect(20, 60, tftWidth - 40, 50, TFT_WHITE);
    
    int barWidth = (tftWidth - 42) / 16;
    for (int i = 0; i < 16; i++) {
        int h = random(5, 45);
        tft.fillRect(21 + i * barWidth, 110 - h, barWidth - 1, h, TFT_GREEN);
    }
    
    tft.setCursor(20, 115);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Playing: /music/song.mp3 (Demo)");

    while (!check(EscPress)) {
        // animate fake fft
        for (int i = 0; i < 16; i++) {
            int h = random(5, 45);
            tft.fillRect(21 + i * barWidth, 61, barWidth - 1, 49, TFT_BLACK);
            tft.fillRect(21 + i * barWidth, 110 - h, barWidth - 1, h, TFT_GREEN);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
