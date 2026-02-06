#include "Game.hpp"
#include "AudioManager.hpp"
#include "UIManager.hpp"
#include "MenuManager.hpp"
#include <iostream>
#include "path_utils.h"

Game* game = nullptr;

int main(int argc, char* argv[]) {
    std::string base = getExeDir();
    // Initialisation
    game = new Game();
    // Loading Enemies
    game->loadEnemies(base + "/config/enemies.txt");

    // Initialize Game (player and enemies)
    game->init("ESCAPE", 100, 100, 800, 600, true);
    
    // Load Map, Textures and Audio
    game->loadAllTextures(base + "/config/textureMapping.txt");
    game->loadEnemyTextures(base + "/config/enemyFrames.txt");
    game->loadDecorationTextures(base + "/config/Decorations.txt");
    AudioManager::loadAllAudios(base + "/config/audioConfig.txt");
    UIManager::loadTextures(base + "/config/HUD.txt", game->getRenderer());
    game->loadMapDataFromFile(base + "/config/map.txt");

    // Place Player
    game->placePlayerAt(1.5f, 1.5f, 0.0f);

    // Start music 
    AudioManager::playMusic("Menu", -1);

    const int FPS = 60;
    const float frameDelay = 1000.0f / FPS;

    Uint32 lastTicks = SDL_GetTicks();

    while (game->running()) {
        Uint32 frameStart = SDL_GetTicks();

        // Delta Time calculation
        Uint32 currentTicks = frameStart;
        float deltaTime = (currentTicks - lastTicks) / 1000.0f;  
        lastTicks = currentTicks;

        // Game Loop 
        switch (game->getState())
        {
        case GameState::GAMEPLAY:
            game->handleEvents();
            game->update(deltaTime);
            game->render();
            break;
        case GameState::RESET:
            game->restart();
            break;
        default:
            if (MenuManager::handleEvents(game->state))
                game->quit();
            MenuManager::renderMenu(game->getRenderer(), {800, 600});
            game->freeMouse();
            break;
        }
        
        // Frame Limiter 
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        
        if (frameTime < frameDelay) {
            SDL_Delay(frameDelay - frameTime);
        }
    }

    delete game;
    return 0;
}
