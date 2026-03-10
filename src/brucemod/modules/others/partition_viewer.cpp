#include "partition_viewer.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include "esp_partition.h"
#include <vector>

struct PartitionInfo {
    char label[17];
    size_t size;
    uint32_t address;
    uint16_t color;
};

uint16_t getPartitionColor(int index) {
    uint16_t palette[] = {TFT_BLUE, TFT_GREEN, TFT_RED, TFT_YELLOW, TFT_MAGENTA, TFT_CYAN, TFT_ORANGE, TFT_PURPLE};
    return palette[index % 8];
}

void partition_viewer_setup() {
    drawMainBorderWithTitle("Flash Partition Viewer");
    
    std::vector<PartitionInfo> partitions;
    size_t totalFlash = 0;

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    int i = 0;

    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        PartitionInfo info;
        strncpy(info.label, p->label, 16);
        info.label[16] = '\0';
        info.size = p->size;
        info.address = p->address;
        info.color = getPartitionColor(i);
        partitions.push_back(info);
        totalFlash += p->size;
        i++;
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);

    tft.setTextSize(FP);
    
    int sw = tftWidth - 20;
    int barH = 12;
    int barY = tftHeight - 30;
    int currentX = 10;
    int legendX = 5;
    int legendY = 30;
    int lineHeight = 11;

    for (int idx = 0; idx < partitions.size(); idx++) {
        PartitionInfo &p = partitions[idx];
        
        // Draw Bar
        int pWidth = (p.size * sw) / totalFlash;
        if (pWidth < 1) pWidth = 1;
        tft.fillRect(currentX, barY, pWidth, barH, p.color);
        tft.drawRect(currentX, barY, pWidth, barH, TFT_DARKGREY);
        
        // Draw Legend
        int col = (idx >= 9) ? tftWidth / 2 : 0;
        int row = (idx >= 9) ? idx - 9 : idx;
        int curY = legendY + (row * lineHeight);
        
        tft.fillRect(legendX + col, curY + 2, 6, 6, p.color);
        tft.setTextColor(p.color, bruceConfig.bgColor);
        tft.setCursor(legendX + col + 10, curY);
        
        if (p.size >= 1024 * 1024) {
            tft.printf("%s %.1fM @%06X", p.label, (float)p.size / 1048576.0, p.address);
        } else {
            tft.printf("%s %dK @%06X", p.label, p.size / 1024, p.address);
        }
        
        currentX += pWidth;
    }
    
    while(!check(EscPress)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
