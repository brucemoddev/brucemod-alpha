#include "games_menu.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include "core/sd_functions.h"
#include <vector>
#include <Preferences.h>
#include <SD.h>

std::vector<String> gamePathList;

void scanGamesDirectory() {
    gamePathList.clear();
    if (!SD.exists("/Games")) {
        SD.mkdir("/Games");
        return;
    }
    
    File dir = SD.open("/Games");
    if (!dir) return;

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            String name = entry.name();
            name.toLowerCase();
            if (name.endsWith(".nes") || name.endsWith(".gb") || name.endsWith(".ard")) {
                gamePathList.push_back(String(entry.path()));
            }
        }
        entry.close();
    }
    dir.close();
}

void getGamePic(int index) {
    int picW = 100;
    // Clear only the picture area
    tft.fillRect(tftWidth - picW, 26, picW, tftHeight - 31, bruceConfig.bgColor);
    
    if (gamePathList.empty() || index < 0 || index >= gamePathList.size()) {
        tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
        tft.setCursor(tftWidth - picW + 10, tftHeight / 2);
        tft.print("No Games");
        return;
    }

    String gamePath = gamePathList[index];
    int lastDot = gamePath.lastIndexOf('.');
    if (lastDot != -1) {
        String picPath = gamePath.substring(0, lastDot) + ".png";
        if (SD.exists(picPath)) {
            // Draw PNG on the right side if supported
            #if !defined(LITE_VERSION)
            drawPNG(SD, picPath, tftWidth - picW, 30, false);
            #else
            tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
            tft.setCursor(tftWidth - picW + 10, tftHeight / 2);
            tft.print("Preview (Lite)");
            #endif
        } else {
            tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
            tft.setCursor(tftWidth - picW + 10, tftHeight / 2);
            tft.print("No Preview");
        }
    }
}

void games_menu_setup() {
    scanGamesDirectory();
    
    int itemSelectID = 0;
    int cameraY = 0;
    const int distanceBetweenLines = 18;
    int picW = 100;
    int listW = tftWidth - picW;

    drawMainBorderWithTitle("Games");
    getGamePic(itemSelectID);

    bool redrawList = true; // Force redraw on first run

    while (!check(EscPress)) {
        if (check(DownPress)) {
            itemSelectID++;
            if (itemSelectID >= (int)gamePathList.size()) itemSelectID = 0;
            getGamePic(itemSelectID);
            redrawList = true;
        }
        if (check(UpPress)) {
            itemSelectID--;
            if (itemSelectID < 0) itemSelectID = gamePathList.empty() ? 0 : (int)gamePathList.size() - 1;
            getGamePic(itemSelectID);
            redrawList = true;
        }

        if (check(SelPress) && !gamePathList.empty()) {
            Preferences prefs;
            prefs.begin("rom", false);
            prefs.putString("RomToLoadPath", gamePathList[itemSelectID]);
            prefs.end();
            
            displaySuccess("ROM Selected. Rebooting...");
            delay(1000);
            ESP.restart();
        }

        if (redrawList) {
            // Clear only the list area
            tft.fillRect(5, 26, listW - 5, tftHeight - 31, bruceConfig.bgColor);
            drawMainBorder(false); // Update status bar
            printTitle("Games");

            if (gamePathList.empty()) {
                tft.setTextColor(TFT_RED, bruceConfig.bgColor);
                tft.setCursor(10, 40);
                tft.print("Put games in /Games");
            } else {
                int visibleLines = (tftHeight - 40) / distanceBetweenLines;
                
                // Update camera view
                if (itemSelectID * distanceBetweenLines < -cameraY) {
                    cameraY = -itemSelectID * distanceBetweenLines;
                } else if (itemSelectID * distanceBetweenLines > -cameraY + (visibleLines - 1) * distanceBetweenLines) {
                    cameraY = -(itemSelectID - visibleLines + 1) * distanceBetweenLines;
                }

                for (size_t i = 0; i < gamePathList.size(); i++) {
                    int y = 40 + (i * distanceBetweenLines) + cameraY;
                    if (y >= 30 && y < tftHeight - 20) {
                        bool selected = (itemSelectID == (int)i);
                        
                        String name = gamePathList[i].substring(gamePathList[i].lastIndexOf('/') + 1);
                        if (name.length() > 14) name = name.substring(0, 12) + "..";

                        if (selected) {
                            tft.fillRoundRect(8, y - 2, listW - 14, 14, 3, bruceConfig.priColor);
                            tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
                            tft.setCursor(12, y);
                            tft.print("> " + name);
                        } else {
                            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                            tft.setCursor(15, y);
                            tft.print(name);
                        }
                    }
                }
            }
            redrawList = false;
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
