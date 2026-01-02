#include "MenuManager.hpp"

Menu MenuManager::currentMenu{Menu::MAIN};
int MenuManager::optionSelected;
std::unordered_map<Menu, int, MenuHash> MenuManager::optionCounts;

std::unordered_map<
        Menu,
        std::unordered_map<int, Action>,
        MenuHash
    > actions;