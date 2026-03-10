#include "main_menu.h"
#include "display.h"
#include "utils.h"
#include <globals.h>

MainMenu::MainMenu() {
    _menuItems = {
        &wifiMenu,
        &bleMenu,
#if !defined(LITE_VERSION)
        &ethernetMenu,
#endif
        &rfMenu,
        &rfidMenu,
        &irMenu,
#if defined(FM_SI4713) && !defined(LITE_VERSION)
        &fmMenu,
#endif
        &fileMenu,
        &gpsMenu,
        &nrf24Menu,
#if !defined(LITE_VERSION)
#if !defined(DISABLE_INTERPRETER)
        &scriptsMenu,
#endif
        &loraMenu,
#endif
        &othersMenu,
        &clockMenu,
#if !defined(LITE_VERSION)
        &connectMenu,
#endif
        &configMenu,
    };

    _totalItems = _menuItems.size();
}

MainMenu::~MainMenu() {}

void MainMenu::begin(void) {
    returnToMenu = false;
    options = {};

    std::vector<String> l = bruceConfig.disabledMenus;
    for (int i = 0; i < _totalItems; i++) {
        String itemName = _menuItems[i]->getName();
        if (find(l.begin(), l.end(), itemName) == l.end()) { // If menu item is not disabled
            options.push_back(
                {// selected lambda
                 _menuItems[i]->getName(),
                 [this, i]() { _menuItems[i]->optionsMenu(); },
                 false,                                  // selected = false
                 [](void *menuItem, bool shouldRender) { // render lambda
                     if (!shouldRender) return false;
                     // We need to access the scale from the option itself if possible, 
                     // but loopOptions usually calls this.
                     // The actual scaling will be handled by loopOptions setting the globals or similar.
                     // For now, let's assume we can get it from MenuItemInterface if we pass it.
                     return false; // let loopOptions handle it if it has custom fields
                 },
                 _menuItems[i]
                }
            );
            // Default render lambda for all main menu items
            options.back().hover = [](void *menuItem, bool shouldRender) {
                if (!shouldRender) return false;
                MenuItemInterface *obj = static_cast<MenuItemInterface *>(menuItem);
                
                // Find the option that owns this lambda to get iconX/Y/Scale
                // This is a bit hacky, maybe we should pass it.
                // For BruceMod, we'll use global variables set by drawDsiMenu/drawPs4Menu
                extern float currentIconScale;
                extern int currentIconX, currentIconY;
                extern bool currentIconCustom;

                if (currentIconCustom) {
                    obj->draw(currentIconScale, currentIconX, currentIconY);
                } else {
                    drawMainBorder(false);
                    float scale = float((float)tftWidth / (float)240);
                    if (bruceConfigPins.rotation & 0b01) scale = float((float)tftHeight / (float)135);
                    obj->draw(scale);
                }
#if defined(HAS_TOUCH)
                TouchFooter();
#endif
                return true;
            };
        }
    }
    _currentIndex = loopOptions(options, MENU_TYPE_MAIN, "Main Menu", _currentIndex);
};

/*********************************************************************
**  Function: hideAppsMenu
**  Menu to Hide or show menus
**********************************************************************/

void MainMenu::hideAppsMenu() {
    auto items = this->getItems();
RESTART: // using gotos to avoid stackoverflow after many choices
    options.clear();
    for (auto item : items) {
        String label = item->getName();
        std::vector<String> l = bruceConfig.disabledMenus;
        bool enabled = find(l.begin(), l.end(), label) == l.end();
        options.push_back({label, [this, label]() { bruceConfig.addDisabledMenu(label); }, enabled});
    }
    options.push_back({"Show All", [=]() { bruceConfig.disabledMenus.clear(); }, true});
    addOptionToMainMenu();
    loopOptions(options);
    bruceConfig.saveFile();
    if (!returnToMenu) goto RESTART;
}
