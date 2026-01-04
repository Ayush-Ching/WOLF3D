#include "MenuManager.hpp"

std::unordered_map<
    Menu,
    std::unordered_map<int, Action>,
    MenuHash
> MenuManager::actions;

Menu MenuManager::currentMenu{Menu::MAIN};
int MenuManager::optionSelected;
std::unordered_map<Menu, int, MenuHash> MenuManager::optionCounts;

std::unordered_map<
        Menu,
        std::unordered_map<int, Action>,
        MenuHash
    > actions;

void MenuManager::handleEvents(bool &isRunning){
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            isRunning = false;
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
}