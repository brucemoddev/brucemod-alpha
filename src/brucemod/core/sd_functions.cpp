#include "sd_functions.h"
#include "display.h" // using displayRedStripe as error msg
#include "modules/badusb_ble/ducky_typer.h"
#include "modules/bjs_interpreter/interpreter.h"
#include "modules/gps/wigle.h"
#include "modules/ir/TV-B-Gone.h"
#include "modules/ir/custom_ir.h"
#include "modules/others/audio.h"
#if defined(HAS_NS4168_SPKR)
#include "modules/others/audio_player.h"
#endif
#include "modules/others/qrcode_menu.h"
#include "modules/rf/rf_send.h"
#include "mykeyboard.h" // using keyboard when calling rename
#include "passwords.h"
#include "scrollableTextArea.h"
#include <globals.h>

#include <MD5Builder.h>
#include <algorithm> // for std::sort
#include <esp_rom_crc.h>

// SPIClass sdcardSPI;
String fileToCopy;
std::vector<FileList> fileList;

// Progress tracking globals
static uint64_t totalProgressBytes = 0;
static uint64_t currentProgressBytes = 0;

/***************************************************************************************
** Function name: setupSdCard
** Description:   Start SD Card
***************************************************************************************/
bool setupSdCard() {
#ifndef USE_SD_MMC
    if (bruceConfigPins.SDCARD_bus.sck < 0) {
        sdcardMounted = false;
        return false;
    }
#endif
    // avoid unnecessary remounting
    if (sdcardMounted) return true;
    bool result = true;
    bool task = false; // devices that doesn't use InputHandler task
#ifdef USE_TFT_eSPI_TOUCH
    task = true;
#endif
#ifdef USE_SD_MMC
    if (!SD.begin("/sdcard", true)) {
        sdcardMounted = false;
        result = false;
    }
#else
    // Not using InputHandler (SdCard on default &SPI bus)
    if (task) {
        if (!SD.begin((int8_t)bruceConfigPins.SDCARD_bus.cs)) result = false;
        // Serial.println("Task not activated");
    }
    // SDCard in the same Bus as TFT, in this case we call the SPI TFT Instance
    else if (bruceConfigPins.SDCARD_bus.mosi == (gpio_num_t)TFT_MOSI &&
             bruceConfigPins.SDCARD_bus.mosi != GPIO_NUM_NC) {
        Serial.println("SDCard in the same Bus as TFT, using TFT SPI instance");
#if TFT_MOSI > 0 // condition for Headless and 8bit displays (no SPI bus)
        if (!SD.begin(bruceConfigPins.SDCARD_bus.cs, tft.getSPIinstance())) {
            result = false;
            Serial.println("SDCard in the same Bus as TFT, but failed to mount");
        }
#else
        goto NEXT; // destination for Headless and 8bit displays (no SPI bus)
#endif

    }
    // If not using TFT Bus, use a specific bus
    else {
    NEXT:
        sdcardSPI.begin(
            (int8_t)bruceConfigPins.SDCARD_bus.sck,
            (int8_t)bruceConfigPins.SDCARD_bus.miso,
            (int8_t)bruceConfigPins.SDCARD_bus.mosi,
            (int8_t)bruceConfigPins.SDCARD_bus.cs
        ); // start SPI communications
        delay(10);
        if (!SD.begin((int8_t)bruceConfigPins.SDCARD_bus.cs, sdcardSPI)) {
            result = false;
#if defined(ARDUINO_M5STICK_C_PLUS) || defined(ARDUINO_M5STICK_C_PLUS2)
            // If using Shared SPI, do not stop the bus if SDCard is not present
            // If using Legacy, release the pins from this SPI Bus
            if (bruceConfigPins.SDCARD_bus.miso != bruceConfigPins.CC1101_bus.miso) sdcardSPI.end();
#endif
        }
        Serial.println("SDCard in a different Bus, using sdcardSPI instance");
    }
#endif

    if (result == false) {
        Serial.println("SDCARD NOT mounted, check wiring and format");
        sdcardMounted = false;
        return false;
    } else {
        Serial.println("SDCARD mounted successfully");
        sdcardMounted = true;
        return true;
    }
}

/***************************************************************************************
** Function name: closeSdCard
** Description:   Turn Off SDCard, set sdcardMounted state to false
***************************************************************************************/
void closeSdCard() {
    SD.end();
    Serial.println("SD Card Unmounted...");
    sdcardMounted = false;
}

/***************************************************************************************
** Function name: ToggleSDCard
** Description:   Turn Off or On the SDCard, return sdcardMounted state
***************************************************************************************/
bool ToggleSDCard() {
    if (sdcardMounted == true) {
        closeSdCard();
        sdcardMounted = false;
        return false;
    } else {
        sdcardMounted = setupSdCard();
        return sdcardMounted;
    }
}

static uint64_t getTotalSize(FS &fs, String path) {
    File dir = fs.open(path);
    if (!dir) return 0;
    if (!dir.isDirectory()) {
        uint64_t s = dir.size();
        dir.close();
        return s;
    }
    uint64_t total = 0;
    bool isDir;
    String fileName = dir.getNextFileName(&isDir);
    while (fileName != "") {
        total += getTotalSize(fs, fileName);
        fileName = dir.getNextFileName(&isDir);
    }
    dir.close();
    return total;
}

/***************************************************************************************
** Function name: deleteFromSd
** Description:   delete file or folder
***************************************************************************************/
bool deleteFromSd(FS fs, String path) {
    File dir = fs.open(path);
    Serial.printf("Deleting: %s\n", path.c_str());
    if (!dir.isDirectory()) {
        dir.close();
        return fs.remove(path.c_str());
    }

    dir.rewindDirectory();
    bool success = true;

    bool isDir;
    String fileName = dir.getNextFileName(&isDir);
    while (fileName != "") {
        if (isDir) {
            success &= deleteFromSd(fs, fileName);
        } else {
            success &= fs.remove(fileName.c_str());
        }
        fileName = dir.getNextFileName(&isDir);
    }

    dir.close();
    success &= fs.rmdir(path.c_str());
    return success;
}

/***************************************************************************************
** Function name: renameFile
** Description:   rename file or folder
***************************************************************************************/
bool renameFile(FS fs, String path, String filename) {
    String newName = keyboard(filename, 76, "Type the new Name:");
    // Rename the file of folder
    if (fs.rename(path, path.substring(0, path.lastIndexOf('/')) + "/" + newName)) {
        // Serial.println("Renamed from " + filename + " to " + newName);
        return true;
    } else {
        // Serial.println("Fail on rename.");
        return false;
    }
}
/***************************************************************************************
** Function name: copyToFs
** Description:   copy file from SD or LittleFS to LittleFS or SD
***************************************************************************************/
bool copyToFs(FS from, FS to, String path, bool draw) {
    bool result = false;
    File source = from.open(path, FILE_READ);
    if (!source) return false;

    String destPath = path;
    if (!destPath.startsWith("/")) destPath = "/" + destPath;
    
    File dest = to.open(destPath, FILE_WRITE);
    if (!dest) {
        source.close();
        return false;
    }

    size_t bytesRead;
    int tot = source.size();
    const int bufSize = 1024;
    static uint8_t buff[bufSize];

    while ((bytesRead = source.read(buff, bufSize)) > 0) {
        if (dest.write(buff, bytesRead) != bytesRead) {
            source.close();
            dest.close();
            return false;
        }
        currentProgressBytes += bytesRead;
        if (draw && totalProgressBytes > 0) {
            progressHandler(currentProgressBytes / 1024, totalProgressBytes / 1024, "Copying...");
        }
    }
    source.close();
    dest.close();
    return true;
}

/***************************************************************************************
** Function name: copyFile
** Description:   copy file address to memory
***************************************************************************************/
bool copyFile(FS fs, String path) {
    File file = fs.open(path, FILE_READ);
    if (!file.isDirectory()) {
        fileToCopy = path;
        file.close();
        return true;
    } else {
        // Folder copy also supported now via fileToCopy being a path
        fileToCopy = path;
        file.close();
        return true;
    }
}

/***************************************************************************************
** Function name: pasteFile
** Description:   paste file to new folder
***************************************************************************************/
bool pasteFile(FS fs, String path) {
    File sourceFile = fs.open(fileToCopy, FILE_READ);
    if (!sourceFile) return false;

    File destFile = fs.open(path + "/" + fileToCopy.substring(fileToCopy.lastIndexOf('/') + 1), FILE_WRITE);
    if (!destFile) {
        sourceFile.close();
        return false;
    }

    size_t bytesRead;
    int tot = sourceFile.size();
    const int bufSize = 1024;
    static uint8_t buff[1024];
    while ((bytesRead = sourceFile.read(buff, bufSize)) > 0) {
        if (destFile.write(buff, bytesRead) != bytesRead) {
            sourceFile.close();
            destFile.close();
            return false;
        }
        currentProgressBytes += bytesRead;
        progressHandler(currentProgressBytes / 1024, totalProgressBytes / 1024, "Pasting...");
    }

    sourceFile.close();
    destFile.close();
    return true;
}

/***************************************************************************************
** Function name: createFolder
** Description:   create new folder
***************************************************************************************/
bool createFolder(FS fs, String path) {
    String foldername = keyboard("", 76, "Folder Name: ");
    if (!fs.mkdir(path + "/" + foldername)) {
        displayRedStripe("Couldn't create folder");
        return false;
    }
    return true;
}

/**********************************************************************
**  Function: readLineFromFile
**  Read the line of the config file until the ';'
**********************************************************************/
String readLineFromFile(File myFile) {
    String line = "";
    char character;

    while (myFile.available()) {
        character = myFile.read();
        if (character == ';') { break; }
        line += character;
    }
    return line;
}

/***************************************************************************************
** Function name: readSmallFile
** Description:   read a small (<3KB) file and return its contents as a single string
**                on any error returns an empty string
***************************************************************************************/
String readSmallFile(FS &fs, String filepath) {
    String fileContent = "";
    File file;

    file = fs.open(filepath, FILE_READ);
    if (!file) return "";

    size_t fileSize = file.size();
    if (fileSize > SAFE_STACK_BUFFER_SIZE || fileSize > ESP.getFreeHeap()) {
        displayError("File is too big", true);
        return "";
    }

    fileContent = file.readString();

    file.close();
    return fileContent;
}

/***************************************************************************************
** Function name: readFile
** Description:   read file and return its contents as a char*
**                caller needs to call free()
***************************************************************************************/
char *readBigFile(FS *fs, String filepath, bool binary, size_t *fileSize) {
    File file = fs->open(filepath);
    if (!file) {
        Serial.printf("Could not open file: %s\n", filepath.c_str());
        return NULL;
    }

    size_t fileLen = file.size();
    char *buf = (char *)(psramFound() ? ps_malloc(fileLen + 1) : malloc(fileLen + 1));
    if (fileSize != NULL) { *fileSize = file.size(); }

    if (!buf) {
        Serial.printf("Could not allocate memory for file: %s\n", filepath.c_str());
        return NULL;
    }

    size_t bytesRead = 0;
    while (bytesRead < fileLen && file.available()) {
        size_t toRead = fileLen - bytesRead;
        if (toRead > 512) { toRead = 512; }
        file.read((uint8_t *)(buf + bytesRead), toRead);
        bytesRead += toRead;
    }
    buf[bytesRead] = '\0';
    file.close();

    return buf;
}

/***************************************************************************************
** Function name: getFileSize
** Description:   get a file size without opening
***************************************************************************************/
size_t getFileSize(FS &fs, String filepath) {
    File file = fs.open(filepath, FILE_READ);
    if (!file) return 0;
    size_t fileSize = file.size();
    file.close();
    return fileSize;
}

String md5File(FS &fs, String filepath) {
    if (!fs.exists(filepath)) return "";
    String txt = readSmallFile(fs, filepath);
    MD5Builder md5;
    md5.begin();
    md5.add(txt);
    md5.calculate();
    return (md5.toString());
}

String crc32File(FS &fs, String filepath) {
    if (!fs.exists(filepath)) return "";
    String txt = readSmallFile(fs, filepath);
    uint32_t romCRC =
        (~esp_rom_crc32_le((uint32_t)~(0xffffffff), (const uint8_t *)txt.c_str(), txt.length())) ^ 0xffffffff;

    char s[18] = {0};
    char crcBytes[4] = {0};
    memcpy(crcBytes, &romCRC, sizeof(uint32_t));
    snprintf(s, sizeof(s), "%02X%02X%02X%02X\n", crcBytes[3], crcBytes[2], crcBytes[1], crcBytes[0]);
    return (String(s));
}

/***************************************************************************************
** Function name: sortList
** Description:   sort files/folders by name
***************************************************************************************/
bool sortList(const FileList &a, const FileList &b) {
    if (a.folder != b.folder) {
        return a.folder > b.folder; // true if a is a folder and b is not
    }
    // Order items alphabetically
    String fa = a.filename.c_str();
    fa.toUpperCase();
    String fb = b.filename.c_str();
    fb.toUpperCase();
    return fa < fb;
}

/***************************************************************************************
** Function name: checkExt
** Description:   check file extension
***************************************************************************************/
bool checkExt(String ext, String pattern) {
    ext.toUpperCase();
    pattern.toUpperCase();
    if (ext == pattern) return true;

    int start = 0;
    int end = pattern.indexOf('|');
    while (end != -1) {
        String currentExt = pattern.substring(start, end);
        if (ext == currentExt) { return true; }
        start = end + 1;
        end = pattern.indexOf('|', start);
    }

    String lastExt = pattern.substring(start);
    return ext == lastExt;
}

/***************************************************************************************
** Function name: readFs
** Description:   read files/folders from a folder
***************************************************************************************/
void readFs(FS fs, String folder, String allowed_ext) {
    fileList.clear();
    FileList object;

    File root = fs.open(folder);
    if (!root || !root.isDirectory()) { return; }

    while (true) {
        bool isDir;
        String fullPath = root.getNextFileName(&isDir);
        String nameOnly = fullPath.substring(fullPath.lastIndexOf("/") + 1);
        if (fullPath == "") { break; }

        if (isDir) {
            object.filename = nameOnly;
            object.folder = true;
            object.operation = false;
            fileList.push_back(object);
        } else {
            int dotIndex = nameOnly.lastIndexOf(".");
            String ext = dotIndex >= 0 ? nameOnly.substring(dotIndex + 1) : "";
            if (allowed_ext == "*" || checkExt(ext, allowed_ext)) {
                object.filename = nameOnly;
                object.folder = false;
                object.operation = false;
                fileList.push_back(object);
            }
        }
    }
    root.close();

    std::sort(fileList.begin(), fileList.end(), sortList);

    object.filename = "> Back";
    object.folder = false;
    object.operation = true;
    fileList.push_back(object);
}

/*********************************************************************
**  Function: loopSD
**  Where you choose what to do with your SD Files
**********************************************************************/
String loopSD(FS &fs, bool filePicker, String allowed_ext, String rootPath) {
    delay(10);
    if (!fs.exists(rootPath)) {
        rootPath = "/";
        if (!fs.exists(rootPath)) {
            if (&fs == &SD) sdcardMounted = false;
            return "";
        }
    }

    Opt_Coord coord;
    String result = "";
    const short PAGE_JUMP_SIZE = (tftHeight - 20) / (LH * FM);
    bool reload = false;
    bool redraw = true;
    int index = 0;
    int maxFiles = 0;
    String Folder = rootPath;
    String PreFolder = rootPath;
    tft.fillScreen(bruceConfig.bgColor);
    tft.drawRoundRect(5, 5, tftWidth - 10, tftHeight - 10, 5, bruceConfig.priColor);
    if (&fs == &SD) {
        if (!setupSdCard()) {
            displayError("Fail Mounting SD", true);
            return "";
        }
    }
    bool exit = false;

    readFs(fs, Folder, allowed_ext);
    maxFiles = fileList.size() - 1; 
    
    while (!exit) {
        delay(10);
        if (redraw) {
            if (strcmp(PreFolder.c_str(), Folder.c_str()) != 0 || reload) {
                index = 0;
                tft.fillScreen(bruceConfig.bgColor);
                tft.drawRoundRect(5, 5, tftWidth - 10, tftHeight - 10, 5, bruceConfig.priColor);
                readFs(fs, Folder, allowed_ext);
                PreFolder = Folder;
                maxFiles = fileList.size() - 1;
                if (strcmp(PreFolder.c_str(), Folder.c_str()) != 0 || index > maxFiles) index = 0;
                reload = false;
            }
            if (fileList.size() < 2) readFs(fs, Folder, allowed_ext);
            coord = listFiles(index, fileList);
            redraw = false;
        }
        displayScrollingText(fileList[index].filename, coord);

        if (EscPress && PrevPress) EscPress = false;
        if (check(EscPress)) {
            if (Folder == "/") break;
            Folder = Folder.substring(0, Folder.lastIndexOf('/'));
            if (Folder == "") Folder = "/";
            index = 0;
            redraw = true;
            continue;
        }

        if (check(PrevPress) || check(UpPress)) {
            if (index == 0) index = maxFiles;
            else if (index > 0) index--;
            redraw = true;
        }
        if (check(NextPress) || check(DownPress)) {
            if (index == maxFiles) index = 0;
            else index++;
            redraw = true;
        }

        if (check(SelPress)) {
            if (fileList[index].folder == true && fileList[index].operation == false) {
                Folder = Folder + (Folder == "/" ? "" : "/") + fileList[index].filename;
                redraw = true;
            } else if (fileList[index].folder == false && fileList[index].operation == false) {
                String filepath = Folder + (Folder == "/" ? "" : "/") + fileList[index].filename;
                result = filepath;
                exit = true;
            } else { // Back
                if (Folder == "/") break;
                Folder = Folder.substring(0, Folder.lastIndexOf('/'));
                if (Folder == "") Folder = "/";
                index = 0;
                redraw = true;
            }
        }
    }
    fileList.clear();
    return result;
}

/***************************************************************************************
** Function name: recursiveCopy
** Description:   Copy folders and files recursively
***************************************************************************************/
bool recursiveCopyInternal(FS &from, FS &to, String path, bool draw) {
    File src = from.open(path);
    if (!src) return false;

    if (!src.isDirectory()) {
        src.close();
        return copyToFs(from, to, path, draw);
    }

    if (!to.exists(path)) {
        if (!to.mkdir(path)) {
            src.close();
            return false;
        }
    }

    bool success = true;
    bool isDir;
    String fileName = src.getNextFileName(&isDir);
    while (fileName != "") {
        success &= recursiveCopyInternal(from, to, fileName, draw);
        fileName = src.getNextFileName(&isDir);
    }
    src.close();
    return success;
}

bool recursiveCopy(FS from, FS to, String path, bool draw) {
    totalProgressBytes = getTotalSize(from, path);
    currentProgressBytes = 0;
    return recursiveCopyInternal(from, to, path, draw);
}

/*********************************************************************
**  Enhanced File Explorer UI Helpers
**********************************************************************/
static void drawFolderIcon(int x, int y, uint16_t color) {
    tft.fillRect(x, y + 2, 12, 8, color);
    tft.fillRect(x, y, 5, 2, color);
}

static void drawFileIcon(int x, int y, uint16_t color) {
    tft.drawRect(x, y, 10, 12, color);
    tft.drawLine(x + 7, y, x + 7, y + 3, color);
    tft.drawLine(x + 7, y + 3, x + 10, y + 3, color);
}

static void drawEnhancedDetails(FS &fs, String path, bool isFolder) {
    // Area at the bottom but inside the main border
    tft.fillRect(10, tftHeight - 22, tftWidth - 20, 12, bruceConfig.bgColor);
    tft.drawFastHLine(10, tftHeight - 23, tftWidth - 20, bruceConfig.secColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    String detail;
    if (isFolder) {
        detail = "Folder: " + path.substring(path.lastIndexOf('/') + 1);
    } else {
        size_t size = getFileSize(fs, path);
        String sizeStr;
        if (size > 1024 * 1024) sizeStr = String(size / (1024.0 * 1024.0), 2) + " MB";
        else if (size > 1024) sizeStr = String(size / 1024.0, 1) + " KB";
        else sizeStr = String(size) + " B";
        
        String ext = path.substring(path.lastIndexOf('.') + 1);
        ext.toUpperCase();
        detail = sizeStr + " | " + ext;
    }
    
    if (detail.length() > 35) detail = detail.substring(0, 32) + "...";
    tft.drawCentreString(detail, tftWidth / 2, tftHeight - 20, 1);
    
    if (showHelpHint) {
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString("[1]:Help", 12, tftHeight - 20);
    }
}

/*********************************************************************
**  Function: enhancedLoopSD
**  Visually pleasing file explorer for BruceMod
**********************************************************************/
String enhancedLoopSD(FS &fs, bool filePicker, String allowed_ext, String rootPath) {
    showHelpHint = true; // Ensure help hint is shown when entering explorer
    if (&fs == &SD && !sdcardMounted) {
        if (!setupSdCard()) {
            displayError("SD Card Error", true);
            return "";
        }
    }

    String currentFolder = rootPath;
    if (!fs.exists(currentFolder)) currentFolder = "/";

    int index = 0;
    bool redraw = true;
    bool exit = false;
    String result = "";
    const int itemsPerPage = 6;

    while (!exit) {
        if (redraw) {
            readFs(fs, currentFolder, allowed_ext);
            int maxFiles = fileList.size() - 1;
            if (index > maxFiles) index = maxFiles >= 0 ? maxFiles : 0;
            if (index < 0) index = 0;

            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorderWithTitle("Explorer: " + currentFolder);

            int startIdx = (index / itemsPerPage) * itemsPerPage;

            for (int i = 0; i < itemsPerPage && (startIdx + i) < fileList.size(); i++) {
                int itemIdx = startIdx + i;
                int y = 35 + (i * 15);
                bool selected = (itemIdx == index);

                if (selected) {
                    tft.fillRoundRect(8, y - 2, tftWidth - 16, 14, 3, bruceConfig.priColor);
                    tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
                } else {
                    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                }

                if (fileList[itemIdx].operation) {
                    tft.setCursor(15, y);
                    tft.print(fileList[itemIdx].filename);
                } else if (fileList[itemIdx].folder) {
                    drawFolderIcon(15, y - 1, selected ? bruceConfig.bgColor : bruceConfig.secColor);
                    tft.setCursor(32, y);
                    String name = fileList[itemIdx].filename;
                    if (name.length() > 25) name = name.substring(0, 22) + "...";
                    tft.print(name);
                } else {
                    drawFileIcon(15, y - 1, selected ? bruceConfig.bgColor : bruceConfig.secColor);
                    tft.setCursor(32, y);
                    String name = fileList[itemIdx].filename;
                    if (name.length() > 25) name = name.substring(0, 22) + "...";
                    tft.print(name);
                }
            }

            if (fileList.size() > 0 && !fileList[index].operation) {
                drawEnhancedDetails(
                    fs, currentFolder + (currentFolder == "/" ? "" : "/") + fileList[index].filename,
                    fileList[index].folder
                );
            }
            redraw = false;
        }

        // Help popup [1]
        keyStroke _key = _getKeyPress();
        if (_key.pressed && !_key.word.empty() && _key.word[0] == '1') {
            showHelpPopup("Explorer");
            redraw = true;
        }

        if (check(UpPress) || check(PrevPress)) {
            showHelpHint = false;
            index--;
            if (index < 0) index = fileList.size() - 1;
            redraw = true;
        }
        if (check(DownPress) || check(NextPress)) {
            showHelpHint = false;
            index++;
            if (index >= (int)fileList.size()) index = 0;
            redraw = true;
        }

        if (check(OptPress)) {
            showHelpHint = false;
            if (!fileList[index].operation) {
                String fullPath = currentFolder + (currentFolder == "/" ? "" : "/") + fileList[index].filename;
                String filename = fileList[index].filename;
                bool isFolder = fileList[index].folder;
                fileList.clear();

                std::vector<Option> fileOptions;
                if (!isFolder) {
                    fileOptions.push_back({"View File", [=, &fs]() { viewFile(fs, fullPath); }});
                    fileOptions.push_back({"File Info", [=, &fs]() { fileInfo(fs, fullPath); }});
                } else {
                    fileOptions.push_back({"Folder Info", [=, &fs]() { displayInfo("Path: " + fullPath, true); }});
                }
                
                fileOptions.push_back({"Rename", [=, &fs]() { renameFile(fs, fullPath, filename); }});
                fileOptions.push_back({"Copy", [=, &fs]() { 
                    fileToCopy = fullPath; 
                    displaySuccess("Copied", true);
                }});
                fileOptions.push_back({"Delete", [=, &fs]() { 
                    totalProgressBytes = isFolder ? getTotalSize(fs, fullPath) : getFileSize(fs, fullPath);
                    currentProgressBytes = 0;
                    if (deleteFromSd(fs, fullPath)) displaySuccess("Deleted", true);
                    else displayError("Delete failed", true);
                }});
                fileOptions.push_back({"New Folder", [=, &fs]() { createFolder(fs, currentFolder); }});
                
                if (fileToCopy != "") {
                    fileOptions.push_back({"Paste", [=, &fs]() { 
                        FS& from = fileToCopy.startsWith("/sdcard") ? (FS&)SD : (FS&)LittleFS;
                        if (recursiveCopy(from, fs, fileToCopy, true)) displaySuccess("Pasted", true);
                        else displayError("Paste failed", true);
                    }});
                }

                if (&fs == &SD)
                    fileOptions.push_back({"Copy->LittleFS", [=]() { recursiveCopy((FS&)SD, (FS&)LittleFS, fullPath, true); }});
                if (&fs == &LittleFS && sdcardMounted)
                    fileOptions.push_back({"Copy->SD", [=]() { recursiveCopy((FS&)LittleFS, (FS&)SD, fullPath, true); }});

                if (!isFolder) {
                    if (fullPath.endsWith(".jpg") || fullPath.endsWith(".gif") || fullPath.endsWith(".bmp") ||
                        fullPath.endsWith(".png"))
                        fileOptions.insert(fileOptions.begin(), {"View Image", [&, fullPath]() {
                                               drawImg(fs, fullPath, 0, 0, true, -1);
                                               delay(750);
                                               while (!check(AnyKeyPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
                                           }});
                    if (fullPath.endsWith(".ir")) {
                        fileOptions.insert(fileOptions.begin(), {"IR Choose cmd", [&, fullPath]() {
                                               delay(200);
                                               chooseCmdIrFile(&fs, fullPath);
                                           }});
                        fileOptions.insert(fileOptions.begin(), {"IR Tx SpamAll", [&, fullPath]() {
                                               delay(200);
                                               txIrFile(&fs, fullPath);
                                           }});
                    }
#if defined(HAS_NS4168_SPKR)
                    if (isAudioFile(fullPath))
                        fileOptions.insert(fileOptions.begin(), {"Play Audio", [&, fullPath]() {
                                               delay(200);
                                               check(AnyKeyPress);
                                               musicPlayerUI(&fs, fullPath);
                                           }});
#endif
                }

                fileOptions.push_back({"Close Menu", [](){}});
                loopOptions(fileOptions, MENU_TYPE_SUBMENU, filename.c_str());
                redraw = true;
            }
        }

        if (check(EscPress)) {
            showHelpHint = false;
            if (currentFolder == "/") {
                exit = true;
            } else {
                currentFolder = currentFolder.substring(0, currentFolder.lastIndexOf('/'));
                if (currentFolder == "") currentFolder = "/";
                index = 0;
                redraw = true;
            }
        }

        if (check(SelPress)) {
            showHelpHint = false;
            if (fileList[index].operation) { // Back
                if (currentFolder == "/") exit = true;
                else {
                    currentFolder = currentFolder.substring(0, currentFolder.lastIndexOf('/'));
                    if (currentFolder == "") currentFolder = "/";
                    index = 0;
                    redraw = true;
                }
            } else if (fileList[index].folder) {
                currentFolder = currentFolder + (currentFolder == "/" ? "" : "/") + fileList[index].filename;
                index = 0;
                redraw = true;
            } else if (filePicker) {
                result = currentFolder + (currentFolder == "/" ? "" : "/") + fileList[index].filename;
                exit = true;
            } else {
                OptPress = true;
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    fileList.clear();
    return result;
}

/*********************************************************************
**  Function: viewFile
**  Display file content
**********************************************************************/
void viewFile(FS fs, String filepath) {
    File file = fs.open(filepath, FILE_READ);
    if (!file) return;

    ScrollableTextArea area = ScrollableTextArea("VIEW FILE");
    area.fromFile(file);

    file.close();

    area.show();
}

/*********************************************************************
**  Function: checkLittleFsSize
**  Check if there are more then 4096 bytes available for storage
**********************************************************************/
bool checkLittleFsSize() {
    if ((LittleFS.totalBytes() - LittleFS.usedBytes()) < 4096) {
        displayError("LittleFS is Full", true);
        return false;
    } else return true;
}

/*********************************************************************
**  Function: checkLittleFsSize
**  Check if there are more then 4096 bytes available for storage
**********************************************************************/
bool checkLittleFsSizeNM() { return (LittleFS.totalBytes() - LittleFS.usedBytes()) >= 4096; }

/*********************************************************************
**  Function: getFsStorage
**  Function will return true and FS will point to SDFS if available
**  and LittleFS otherwise. If LittleFS is full it wil return false.
**********************************************************************/
bool getFsStorage(FS *&fs) {
    // don't try to mount SD Card if not previously mounted
    if (sdcardMounted) fs = &SD;
    else if (checkLittleFsSize()) fs = &LittleFS;
    else return false;

    return true;
}

/*********************************************************************
**  Function: fileInfo
**  Display file info
**********************************************************************/
void fileInfo(FS fs, String filepath) {
    File file = fs.open(filepath, FILE_READ);
    if (!file) return;

    int bytesize = file.size();
    float filesize = bytesize;
    String unit = "B";

    time_t modifiedTime = file.getLastWrite();

    if (filesize >= 1000000) {
        filesize /= 1000000.0;
        unit = "MB";
    } else if (filesize >= 1000) {
        filesize /= 1000.0;
        unit = "kB";
    }

    drawMainBorderWithTitle("FILE INFO");
    padprintln("");
    padprintln("Path: " + filepath);
    padprintln("");
    padprintf("Bytes: %d\n", bytesize);
    padprintln("");
    padprintf("Size: %.02f %s\n", filesize, unit.c_str());
    padprintln("");
    padprintf("Modified: %s\n", ctime(&modifiedTime));

    file.close();
    delay(100);

    while (!check(EscPress) && !check(SelPress)) { delay(100); }

    return;
}

/*********************************************************************
**  Function: createNewFile
**  Function will save a file into FS. If file already exists it will
**  append a version number to the file name.
**********************************************************************/
File createNewFile(FS *&fs, String filepath, String filename) {
    int extIndex = filename.lastIndexOf('.');
    String name = filename.substring(0, extIndex);
    String ext = filename.substring(extIndex);

    if (filepath.endsWith("/")) filepath = filepath.substring(0, filepath.length() - 1);
    if (!(*fs).exists(filepath)) (*fs).mkdir(filepath);

    name = filepath + "/" + name;

    if ((*fs).exists(name + ext)) {
        int i = 1;
        name += "_";
        while ((*fs).exists(name + String(i) + ext)) i++;
        name += String(i);
    }

    Serial.println("Creating file: " + name + ext);
    File file = (*fs).open(name + ext, FILE_WRITE);
    return file;
}
