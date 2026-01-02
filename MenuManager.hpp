#include "Game.hpp"
#include "UIManager.hpp"
#include <unordered_map>
#include <functional>

enum class Menu {
    MAIN,
    PAUSE,
    GAME_LOSE,
    GAME_WON
};

struct MenuHash {
    std::size_t operator()(Menu m) const {
        return static_cast<std::size_t>(m);
    }
};

using Action = std::function<void(int)>;

class MenuManager {
public:

    static void setMenu(Menu menu) {
        // WHICH MENU TO SHOW?
        currentMenu = menu;
        optionSelected = 0;
    }

    static void bind(Menu menu, int option, Action action) {
        // WHAT ACTION TO TAKE ON SELECTING AN OPTION?
        actions[menu][option] = std::move(action);
    }

    static void moveUp() {
        if (optionSelected > 0) optionSelected--;
    }

    static void moveDown() {
        int max = optionCounts[currentMenu];
        if (optionSelected + 1 < max) {
            optionSelected++;
        }
    }

    static void select() {
        auto mIt = actions.find(currentMenu);
        if (mIt == actions.end()) return;

        auto oIt = mIt->second.find(optionSelected);
        if (oIt == mIt->second.end()) return;

        oIt->second(optionSelected);
    }

    static void handleEvents();
    static void renderMenu(); // use UIManager here
    // No "update" needed in menus 
    // (not implementing any animations)
private:
    static Menu currentMenu;
    static int optionSelected;
    static std::unordered_map<Menu, int, MenuHash> optionCounts;

    static std::unordered_map<
        Menu,
        std::unordered_map<int, Action>,
        MenuHash
    > actions;
};