#include "MenuManager.hpp"

std::unordered_map<
    Menu,
    std::unordered_map<int, Action>,
    MenuHash
> MenuManager::actions;

Menu MenuManager::currentMenu{Menu::MAIN};
int MenuManager::optionSelected;
std::unordered_map<Menu, int, MenuHash> MenuManager::optionCounts;

std::unordered_map<Menu, std::vector<std::string>, MenuHash> 
MenuManager::buttonNames=
{
    {
        Menu::MAIN,{
            "PLAY",
            "READ THIS!",
            "CREDITS",
            "QUIT"
        }
    },
    {
        Menu::PAUSE,{
            "RESUME",
            "READ THIS!",
            "BACK TO MENU",
            "QUIT"
        }
    },
    {
        Menu::GAME_LOSE,{
            "BACK TO MENU",
            "QUIT"
        }
    },
    {
        Menu::GAME_WON,{
            "BACK TO MENU",
            "QUIT"
        }
    }
};

std::unordered_map<
        Menu,
        std::unordered_map<int, Action>,
        MenuHash
    > actions;

SDLTexturePtr MenuManager::cursorImage{nullptr, SDL_DestroyTexture};
std::pair<int, int> MenuManager::cursorImageWH;

void MenuManager::Init(SDL_Renderer& r){
    loadCursorImage("Textures/Red_triangle.svg", r);
    currentMenu = Menu::MAIN;
}

std::tuple<SDL_Color, SDL_Color, SDL_Color> MenuManager::menuColors = {
    SDL_Color{14, 23, 126, 255},   // Background
    SDL_Color{6, 11, 81, 255},   // Foregroud
    SDL_Color{255, 255, 255, 255}    // Font
};

std::tuple<int, int> MenuManager::fontSizes = {
    2,  // Buttons
    3   // Title
};

bool MenuManager::handleEvents(){
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            return true;
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
        {
            if (event.key.keysym.scancode == SDL_SCANCODE_UP)
            {
                moveUp();
            }else if (event.key.keysym.scancode == SDL_SCANCODE_DOWN)
            {
                moveDown();
            }else if (event.key.keysym.scancode == SDL_SCANCODE_RETURN ||
                event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER)
            {
                select();
            }
        }
    }
    return false;
}

void MenuManager::loadCursorImage(const char* filePath, SDL_Renderer& renderer)
{
    SDL_Texture* raw = IMG_LoadTexture(&renderer, filePath);
    if (!raw) {
        std::cerr << "Failed to load Cursor texture: "
                  << filePath << " | " << IMG_GetError() << "\n";
        return;
    }

    cursorImage = SDLTexturePtr(raw, SDL_DestroyTexture);

    int width = 0, height = 0;
    if (SDL_QueryTexture(raw, nullptr, nullptr, &width, &height) != 0) {
        std::cerr << "Failed to query texture: "
                  << SDL_GetError() << "\n";
        return;
    }
    cursorImageWH = std::make_pair(width, height);
}

void MenuManager::renderMenu(SDL_Renderer& renderer, const std::pair<int, int>& screenWH){
    auto background = std::get<0>(menuColors);
    auto foreground = std::get<1>(menuColors);
    auto font       = std::get<2>(menuColors);

    // Painting Menu
    UIManager::drawFilledRectWithBorder(renderer,
        {0,0,screenWH.first, screenWH.second},
        background, background, 0
    );


    SDL_RenderPresent(&renderer);

}