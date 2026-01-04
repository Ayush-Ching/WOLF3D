#include "Game.hpp"
#include "UIManager.hpp"
#include <unordered_map>
#include <functional>
#include <tuple>

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

using Action = std::function<void(void)>;

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

        oIt->second();
    }
    static void loadCursorImage(const char* filePath, SDL_Renderer& r);
    static void Init(SDL_Renderer&);
    static bool handleEvents();
    static void renderMenu(SDL_Renderer&, const std::pair<int, int>&); // use UIManager here
    // No "update" needed in menus, also no separate textures
    // only plain filled squares and text
    // (not implementing any animations)
private:
    static Menu currentMenu;
    static int optionSelected;
    static SDLTexturePtr cursorImage;
    static std::pair<int, int> cursorImageWH;
    static std::unordered_map<Menu, int, MenuHash> optionCounts;
    static std::unordered_map<Menu, std::vector<std::string>, MenuHash> 
    buttonNames;
    static std::unordered_map<
        Menu,
        std::unordered_map<int, Action>,
        MenuHash
    > actions;

    static std::tuple<SDL_Color, SDL_Color, SDL_Color> menuColors;
    static std::tuple<int, int> fontSizes;
};