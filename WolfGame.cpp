#include "WolfGame.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <memory>
#include <utility>

Game::Game(){

}

Game::~Game(){
    clean();
}


auto distSq = [](const std::pair<float, float>& a,
                 const std::pair<float, float>& b)
{
    float dx = a.first  - b.first;
    float dy = a.second - b.second;
    return dx * dx + dy * dy;
};

void Game::init(const char *title, int xpos, int ypos, int width, int height, bool fullscreen)
{
    if(SDL_Init(SDL_INIT_EVERYTHING) == 0){
        int flags = 0;
        if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) {
            SDL_Log("IMG init error: %s", IMG_GetError());
            isRunning = false;
            return;
        }
        if(fullscreen){
            flags = SDL_WINDOW_FULLSCREEN;
        }
        window.reset(SDL_CreateWindow(title, xpos, ypos, width, height, flags));
        ScreenHeightWidth = std::make_pair(width, height);
        if(window){
            renderer.reset(SDL_CreateRenderer(window.get(), -1, 0));
            if(renderer.get()){
                SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
            }
            isRunning = true;
        }
    } else {
        isRunning = false;
    }
    int i=0;
    for(const std::unique_ptr<Enemy>& e : enemies){
        e->init(static_cast<int>(AllSpriteTextures.size()));
        AllSpriteTextures.push_back(Sprite{static_cast<int>(AllSpriteTextures.size()), 
            e->get_position(), nullptr
            , enemyTextureWidth, enemyTextureHeight,
            true
            });
        enemySpriteIDToindex[e->get_spriteID()] = i;
        i++;
    }
    AudioManager::init();
}
void Game::handleEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            isRunning = false;

        // Hide cursor and lock on first click
        if (event.type == SDL_MOUSEBUTTONDOWN  && event.button.button == SDL_BUTTON_LEFT)
        {
            SDL_ShowCursor(SDL_DISABLE);
            SDL_SetRelativeMouseMode(SDL_TRUE);   // capture mouse
            //std::cout << "Mouse captured\n";
            if(!hasShot && weapons.size() > 0){
                if(weapons[currentWeapon].ammo == 0 && weapons[currentWeapon].multiplier > 1){
                    std::cout << "Out of ammo!\n";
                }
                else{
                    shotThisFrame = true;
                    hasShot = true;
                    fireCooldown = 0.0f;
                    if(weapons[currentWeapon].multiplier > 1){
                        weapons[currentWeapon].ammo--;
                    }
                    AudioManager::playSFX(weapons[currentWeapon].soundName, MIX_MAX_VOLUME);
                }
            }
            else{
            //    std::cout << "Weapon still cooling down\n";
            }
        }

        // Mouse movement → rotate player
        if (event.type == SDL_MOUSEMOTION)
        {
            // event.motion.xrel = delta X since last frame
            playerAngle += event.motion.xrel * mouseSensitivity;
            playerAngle = fmod(playerAngle, 2 * PI);
        }
    }

    // Keyboard movement detection
    const Uint8* keystate = SDL_GetKeyboardState(NULL);

    playerMoveDirection = {0.0f, 0.0f};

    // Forward
    if (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP]) {
        playerMoveDirection.first += cos(playerAngle);
        playerMoveDirection.second += sin(playerAngle);
    }

    // Backward
    if (keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN]) {
        playerMoveDirection.first -= cos(playerAngle);
        playerMoveDirection.second -= sin(playerAngle);
    }

    // Strafe Left (A)
    if (keystate[SDL_SCANCODE_A]) {
        playerMoveDirection.first += cos(playerAngle - 3.14159f/2);
        playerMoveDirection.second += sin(playerAngle - 3.14159f/2);
    }

    // Strafe Right (D)
    if (keystate[SDL_SCANCODE_D]) {
        playerMoveDirection.first += cos(playerAngle + 3.14159f/2);
        playerMoveDirection.second += sin(playerAngle + 3.14159f/2);
    }

    // Optional keyboard turning (can keep or remove)
    if (keystate[SDL_SCANCODE_LEFT])
        playerAngle -= rotationSensitivity;

    if (keystate[SDL_SCANCODE_RIGHT])
        playerAngle += rotationSensitivity;

    // Door interaction (Space to open/close)
    // Door interaction (Space to open/close)
    if (keystate[SDL_SCANCODE_SPACE])
    {
        int tx = (int)(playerPosition.first  + cos(playerAngle) * playerSquareSize *1.1f);
        int ty = (int)(playerPosition.second + sin(playerAngle) * playerSquareSize *1.1f);

        auto key = std::make_pair(ty, tx);
        if (doors.count(key)) {
            Door& d = doors[key];

            if (d.locked && !playerHasKey(d.keyType))
                return;

            if (d.openAmount == 0.0f){
                AudioManager::playSFX("door_open", MIX_MAX_VOLUME);
                d.opening = true;
            }
        }
    }

    // Weapon switching (number keys)
    if(keystate[SDL_SCANCODE_1]){
        if(playerHasWeapon(1))
            currentWeapon = 1;
    }
    if(keystate[SDL_SCANCODE_2]){
        if(playerHasWeapon(2))
            currentWeapon = 2;
    }
    if(keystate[SDL_SCANCODE_3]){
        if(playerHasWeapon(3))
            currentWeapon = 3;
    }

}

void Game::update(float deltaTime)
{
    // Normalize movement direction
    float lengthSquared = playerMoveDirection.first * playerMoveDirection.first +
    playerMoveDirection.second * playerMoveDirection.second;

    if (lengthSquared > 0.0f) {
        float length = sqrt(lengthSquared);
        playerMoveDirection.first /= length;
        playerMoveDirection.second /= length;
    }

    // Collision detection and position update
    float newX = playerPosition.first + playerMoveDirection.first * playerSpeed * deltaTime;
    float newY = playerPosition.second + playerMoveDirection.second * playerSpeed * deltaTime;
    int mx = (int)newX;
    int my = (int)playerPosition.second;
    if (my < 0 || my >= Map.size()) return;
    if (mx < 0 || mx >= Map[my].size()) return;

    int tileX = Map[(int)playerPosition.second][(int)(newX + playerSquareSize * (newX>playerPosition.first?1:-1))];
    int tileY = Map[(int)(newY + playerSquareSize * (newY>playerPosition.second?1:-1))][(int)playerPosition.first];
    if (tileX == 0 ||
        (isDoor(tileX) &&
        doors[{(int)playerPosition.second,
                (int)(newX + playerSquareSize * (newX > playerPosition.first ? 1 : -1))}]
            .openAmount > 0.5f))
    {
        if (!collidesWithEnemy(newX, playerPosition.second)) {
            playerPosition.first = newX;
        }
    }

    if (tileY == 0 ||
        (isDoor(tileY) &&
        doors[{(int)(newY + playerSquareSize * (newY > playerPosition.second ? 1 : -1)),
                (int)playerPosition.first}]
            .openAmount > 0.5f))
    {
        if (!collidesWithEnemy(playerPosition.first, newY)) {
            playerPosition.second = newY;
        }
    }

    // Update doors
    for (auto& [pos, d] : doors)
    {
        if (d.opening) {
            d.openAmount += 1.5f * deltaTime;
            if (d.openAmount >= 1.0f) {
                d.openAmount = 1.0f;
                d.opening = false;
            }
        }
    }

    // Update enemies
    for(const std::unique_ptr<Enemy>& e : enemies){
        e->_process(deltaTime, playerPosition, playerAngle);

        // Update canSeePlayer
        bool x = rayCastEnemyToPlayer(*e);
        e->updateCanSeePlayer(x);
        int dmg = e->getDamageThisFrame();
        e->clearDamageThisFrame();
        if(x && dmg > 0){
            health -= dmg;
            if(health < 0) health = 0;

        // Update Alerts
        if(shotThisFrame && weapons[currentWeapon].multiplier > 1 && !e->isAlerted()){
            float dist = distSq(playerPosition, e->get_position());
            dist = pow(dist, 0.5f);
            if(dist <= weapons[currentWeapon].alertRadius)
                e->alert();
        }
    //std::cout << "Player Health: " << health << std::endl;
        }

        // Enemy position relative to player 
        auto [ex, ey] = e->get_position();

        float dx = ex - playerPosition.first;
        float dy = ey - playerPosition.second;

        float enemyDist = sqrt(dx*dx + dy*dy);

        // Angle between player view and enemy 
        float enemyAngle = atan2(dy, dx) - playerAngle;

        while (enemyAngle > PI)  enemyAngle -= 2 * PI;
        while (enemyAngle < -PI) enemyAngle += 2 * PI;

        // Check if enemy is inside FOV 
        if (fabs(enemyAngle) > halfFov){
            //std::cout<<"enemy out of FOV\n";
            continue; 
        }
        int frame = e->get_current_frame(), dir = e->get_dirn_num();
        auto it = enemyTextures.find({frame, dir});
        if (it == enemyTextures.end()) continue;
        AllSpriteTextures[e->get_spriteID()].texture = it->second;
        AllSpriteTextures[e->get_spriteID()].position = e->get_position();

    }
    if(hasShot){
        fireCooldown += deltaTime;
        if(fireCooldown >= weapons[currentWeapon].coolDownTime){
            hasShot = false;
            shotThisFrame = false;
        }
    }

    // Update keys pickup
    for (const auto& [keyType, pos] : keysPositions) {
        if(playerHasKey(keyType))
            continue;
        auto [kx, ky] = pos;
        float keyX = kx + 0.5f;
        float keyY = ky + 0.5f;
        
        float dx = keyX - playerPosition.first;
        float dy = keyY - playerPosition.second;
        float distanceSq = dx * dx + dy * dy;

        if (distanceSq < keyRadius * keyRadius) {
            acquireKey(keyType);
            // Remove key from map
            AllSpriteTextures[keyTypeToSpriteID[keyType]].active = false;
            break; // Exit loop since we modified the map
        }
    }

    // Update weapons pickup
    for (const auto& [weaponType, pos] : weaponsPositions) {
        if(playerHasWeapon(weaponType))
            continue;
        auto [wx, wy] = pos;
        float weaponX = wx + 0.5f;
        float weaponY = wy + 0.5f;

        float dx = weaponX - playerPosition.first;
        float dy = weaponY - playerPosition.second;
        float distanceSq = dx * dx + dy * dy;

        if (distanceSq < weaponRadius * weaponRadius) {
            acquireWeapon(weaponType);
            // Remove weapon from map
            AllSpriteTextures[weaponTypeToSpriteID[weaponType]].active = false;
            break; // Exit loop since we modified the map
        }
    }

        // ------- Update Textures to be rendered ---------
        renderOrder.clear();
        
        for (int i = 0; i < AllSpriteTextures.size(); ++i) {
            if (AllSpriteTextures[i].active){
                renderOrder.push_back(i);
            }
        }

}

void Game::render()
{
    SDL_SetRenderDrawColor(renderer.get(), 40, 40, 40, 255);
    SDL_RenderClear(renderer.get());   
    std::vector<float> zBuffer(ScreenHeightWidth.first);

    // Draw floor
    if (floorTextures.size() == 0) {
        SDL_SetRenderDrawColor(renderer.get(), 100, 100, 100, 255);
        SDL_Rect floorRect = {0, ScreenHeightWidth.second / 2, ScreenHeightWidth.first, ScreenHeightWidth.second / 2};
        SDL_RenderFillRect(renderer.get(), &floorRect);
    }
    // Raycasting for walls
    int raysCount = ScreenHeightWidth.first;
    
    for (int ray = 0; ray < raysCount; ray++)
    {
        // Angle of this ray
        float rayAngle = playerAngle - halfFov + ray * (fovRad / raysCount);

        // Ray direction
        float rayDirX = cos(rayAngle);
        float rayDirY = sin(rayAngle);

        // Map tile the ray starts in
        int mapX = (int)playerPosition.first;
        int mapY = (int)playerPosition.second;

        // Length of ray from one x-side to next x-side
        float deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1.0f / rayDirX);
        // Length of ray from one y-side to next y-side
        float deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1.0f / rayDirY);

        int stepX, stepY;
        float sideDistX, sideDistY;

        // Step direction and initial side distances
        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (playerPosition.first - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0f - playerPosition.first) * deltaDistX;
        }

        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (playerPosition.second - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0f - playerPosition.second) * deltaDistY;
        }

        bool hitWall = false;
        int hitSide = 0; // 0 = vertical hit, 1 = horizontal hit

        while (!hitWall)
        {
            // Jump to next grid square
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                hitSide = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                hitSide = 1;
            }

            // Check if the ray hit a wall
            if (Map[mapY][mapX] > 0 && !isDoor(Map[mapY][mapX])) {
                hitWall = true;
            }
            else if (isDoor(Map[mapY][mapX]))
            {
                float open = 0.0f;
                auto it = doors.find({mapY, mapX});
                if (it != doors.end()) {
                    open = it->second.openAmount;
                } else {
                    std::cout << "Door at "<<mapY<<", "<<mapX<<" not found\n";
                    return;
                }
                
                // --- compute hit distance ---
                float hitDist = (hitSide == 0 ? sideDistX - deltaDistX
                                            : sideDistY - deltaDistY);

                // exact float hit position
                float hitX = playerPosition.first  + rayDirX * hitDist;
                float hitY = playerPosition.second + rayDirY * hitDist;

                // local coords inside tile (0..1)
                float localX = hitX - floor(hitX);
                float localY = hitY - floor(hitY);

                bool blocks = false;

                if (hitSide == 0) {
                    // vertical → opening affects Y movement
                    // door blocks if hit Y is beyond open amount
                    blocks = (localY >= open);
                } else {
                    // horizontal → opening affects X movement
                    blocks = (localX >= open);
                }

                if (blocks)
                    hitWall = true;      // ray stops here
                else
                    hitWall = false;     // ray passes through (door is open)
            }

        }

        // Distance to wall = distance to side where hit happened
        float distanceToWall;
        if (hitSide == 0)
            distanceToWall = sideDistX - deltaDistX;
        else
            distanceToWall = sideDistY - deltaDistY;

        float hitX = playerPosition.first  + rayDirX * distanceToWall;
        float hitY = playerPosition.second + rayDirY * distanceToWall;
        float wallX;
        if (hitSide == 0)
            wallX = hitY - floor(hitY);
        else
            wallX = hitX - floor(hitX);
        float deltaAngle = rayAngle - playerAngle;
        float correctedDistance = distanceToWall * cos(deltaAngle);
        zBuffer[ray] = correctedDistance;

        // Calculate wall height
        int lineHeight = (int)(ScreenHeightWidth.second / correctedDistance);
        int drawStart = -lineHeight / 2 + ScreenHeightWidth.second / 2;
        int drawEnd   =  lineHeight / 2 + ScreenHeightWidth.second / 2;

        if (drawStart < 0) drawStart = 0;
        if (drawEnd >= ScreenHeightWidth.second) drawEnd = ScreenHeightWidth.second - 1;

        // Wall Texture
        int texId = Map[mapY][mapX] - 1;
        int imgWidth = wallTextureWidths[texId], imgHeight = wallTextureHeights[texId];
        SDL_QueryTexture(wallTextures[texId].get(), NULL, NULL, &imgWidth, &imgHeight);

        // -------- distance-based shading --------
        float maxLightDist = 8.0f;
        float shade = 1.0f - std::min(correctedDistance / maxLightDist, 1.0f);
        Uint8 brightness = (Uint8)(40 + shade * 215);

        // Darken horizontal walls (classic Wolf3D trick)
        if (hitSide == 1) {
            brightness = (Uint8)(brightness * 0.7f);
        }
        SDL_SetTextureColorMod(wallTextures[texId].get(),
                            brightness, brightness, brightness);
        // --------------------------------------

        if(!isDoor(texId+1)){
            int texX = (int)(wallX * imgWidth);
            if(hitSide == 0 && rayDirX > 0) texX = imgWidth - texX - 1;
            if(hitSide == 1 && rayDirY < 0) texX = imgWidth - texX - 1;
            texX = std::clamp(texX, 0, imgWidth - 1);

            SDL_Rect srcRect  = { texX, 0, 1, imgHeight };
            SDL_Rect destRect = { ray, drawStart, 1, drawEnd - drawStart };
            SDL_RenderCopy(renderer.get(), wallTextures[texId].get(), &srcRect, &destRect);
        }
        else if (wallX > doors[{mapY, mapX}].openAmount) {

            wallX -= doors[{mapY, mapX}].openAmount;
            int texX = (int)(wallX * imgWidth);
            if(hitSide == 0 && rayDirX > 0) texX = imgWidth - texX - 1;
            if(hitSide == 1 && rayDirY < 0) texX = imgWidth - texX - 1;
            texX = std::clamp(texX, 0, imgWidth - 1);

            SDL_Rect srcRect  = { texX, 0, 1, imgHeight };
            SDL_Rect destRect = { ray, drawStart, 1, drawEnd - drawStart };
            SDL_RenderCopy(renderer.get(), wallTextures[texId].get(), &srcRect, &destRect);
        }

        // Draw floor texture
        if (floorTextures.size() > 0) {
            int floorScreenStart = drawEnd; // start drawing floor below the wall
            imgHeight = floorTextureHeights[0];
            imgWidth = floorTextureWidths[0];
            for (int y = drawEnd; y < ScreenHeightWidth.second; y++) {
                float rowDist = playerHeight / ((float)y / ScreenHeightWidth.second - 0.5f);

                // Interpolate floor coordinates
                float floorX = playerPosition.first + rowDist * rayDirX;
                float floorY = playerPosition.second + rowDist * rayDirY;

                int texX = ((int)(floorX * imgWidth)) % imgWidth;
                int texY = ((int)(floorY * imgHeight)) % imgHeight;

                SDL_Rect srcRect  = { texX, texY, 1, 1 };
                SDL_Rect destRect = { ray, y, 1, 1 };
                SDL_RenderCopy(renderer.get(), floorTextures[0].get(), &srcRect, &destRect);
            }
        }
        
        // Draw ceiling
        for(int y = 0; y < drawStart; y++) {
            if(ceilingTextures.size() > 0) {
                int imgWidth = ceilingTextureWidths[0];
                int imgHeight = ceilingTextureHeights[0];

                float rowDist = playerHeight / (0.5f - (float)y / ScreenHeightWidth.second);

                // Interpolate ceiling coordinates
                float ceilX = playerPosition.first + rowDist * rayDirX;
                float ceilY = playerPosition.second + rowDist * rayDirY;

                int texX = ((int)(ceilX * imgWidth)) % imgWidth;
                int texY = ((int)(ceilY * imgHeight)) % imgHeight;

                SDL_Rect srcRect  = { texX, texY, 1, 1 };
                SDL_Rect destRect = { ray, y, 1, 1 };
                SDL_RenderCopy(renderer.get(), ceilingTextures[0].get(), &srcRect, &destRect);
            } 
        }
    }
    // Rendering Sprites
    // Sort sprites by distance from player (far to near)
    std::sort(renderOrder.begin(), renderOrder.end(),
        [&](int a, int b) {
            return distSq(playerPosition, AllSpriteTextures[a].position) >
           distSq(playerPosition, AllSpriteTextures[b].position);
        });
    int enemyShotIndex = -1;
    for (int i=0; i < renderOrder.size(); i++) {
        int id = renderOrder[i];
        const Sprite& sprite = AllSpriteTextures[id];
        if (!sprite.texture){
            //std::cout << "Skipping sprite ID " << sprite.spriteID << " due to null texture.\n";
            //std::cout << sprite.isEnemy << "\n";
            continue; // skip if texture is null
        }
        // Sprite position relative to player
        auto [sx, sy] = sprite.position;
        float dx = sx - playerPosition.first;
        float dy = sy - playerPosition.second;
        float spriteDist = sqrt(dx*dx + dy*dy);
        // Angle between player view and sprite
        float spriteAngle = atan2(dy, dx) - playerAngle;
        while (spriteAngle > PI)  spriteAngle -= 2 * PI;
        while (spriteAngle < -PI) spriteAngle += 2 * PI;
        // Check if sprite is inside FOV
        if (fabs(spriteAngle) > halfFov)
            continue; // skip sprite if outside FOV
        
        // Project sprite onto screen
        int screenX = (int)((spriteAngle + halfFov) / fovRad * ScreenHeightWidth.first);

        // Perspective scaling
        int spriteHeight = (int)(ScreenHeightWidth.second / spriteDist);
        int spriteWidth  = (int)(spriteHeight * ((float)sprite.textureWidth / sprite.textureHeight));

        int drawStartY = -spriteHeight / 2 + ScreenHeightWidth.second / 2;;
        int drawEndY   =  spriteHeight / 2 + ScreenHeightWidth.second / 2;
        int drawStartX = screenX - spriteWidth / 2;
        int drawEndX   = screenX + spriteWidth / 2;

        int centreX = ScreenHeightWidth.first / 2;
        if(sprite.isEnemy && shotThisFrame && spriteDist < weapons[currentWeapon].range){
            enemyShotIndex = enemySpriteIDToindex.at(sprite.spriteID);
        }

        if (drawStartY < 0) drawStartY = 0;
        if (drawEndY >= ScreenHeightWidth.second) drawEndY = ScreenHeightWidth.second - 1;
        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= ScreenHeightWidth.first) drawEndX = ScreenHeightWidth.first - 1;

        SDL_Texture* texture = sprite.texture.get();

        // Render sprite column by column
        for (int x = drawStartX; x < drawEndX; x++) {
            int texX = (int)((float)(x - (screenX - spriteWidth / 2)) / (float)spriteWidth * sprite.textureWidth);
            if (texX < 0 || texX >= sprite.textureWidth) continue;
            if (spriteDist < zBuffer[x]) {
                SDL_Rect srcRect  = { texX, 0, 1, sprite.textureHeight };
                SDL_Rect destRect = { x, drawStartY, 1, drawEndY - drawStartY };
                SDL_RenderCopy(renderer.get(), texture, &srcRect, &destRect);
            }
        }
    }
    if (enemyShotIndex != -1) {
        float dist = distSq(
            playerPosition,
            enemies[enemyShotIndex]->get_position()
        );
        dist = pow(dist, 0.5f); // sqrt
        int dmg=0;
        if(canShootEnemy(dist))
            dmg = (rand() & 31) * weapons[currentWeapon].multiplier;
        //std::cout << "Enemy at index " << enemyShotIndex << " shot for " << dmg << " damage.\n";
        if (rayCastEnemyToPlayer(*enemies[enemyShotIndex])){
            enemies[enemyShotIndex]->takeDamage(dmg); 
            float relativeAngle = atan2(
                enemies[enemyShotIndex]->get_position().second - playerPosition.second,
                enemies[enemyShotIndex]->get_position().first  - playerPosition.first
            ) - playerAngle;
            while (relativeAngle > PI)  relativeAngle -= 2 * PI;
            while (relativeAngle < -PI) relativeAngle += 2 * PI;
        }
        shotThisFrame = false;
    }
    else if (shotThisFrame) {
        shotThisFrame = false;
    }
    SDL_RenderPresent(renderer.get()); 
}

void Game::loadMapDataFromFile(const char* filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open map data file: " << filename << std::endl;
        return;
    }

    Map.clear();
    doors.clear();

    std::string line;
    size_t rowIndex = 0;

    while (std::getline(file, line)) {
        std::vector<int> row;
        std::istringstream iss(line);
        std::string token;

        while (iss >> token) {                 // space-separated
            // Key handling
            //std::cout<<token<<"\n";
            if(token == "B"){
                keysPositions[1] = {row.size(), rowIndex};
                keyTypeToSpriteID[1] = AllSpriteTextures.size();
                AllSpriteTextures.push_back(Sprite{ static_cast<int>(AllSpriteTextures.size()), std::pair<float, float>{row.size(), rowIndex}, keysTextures[0],
                 keyWidthsHeights[1].first, keyWidthsHeights[1].second});
                row.push_back(0);
                continue;
            }
            else if(token == "R"){
                keysPositions[2] = {row.size(), rowIndex};
                keyTypeToSpriteID[2] = AllSpriteTextures.size();
                AllSpriteTextures.push_back(Sprite{ static_cast<int>(AllSpriteTextures.size()), std::pair<float, float>{row.size(), rowIndex}, keysTextures[1],
                 keyWidthsHeights[2].first, keyWidthsHeights[2].second});
                row.push_back(0);
                continue;
            }
            else if(token == "G"){
                keysPositions[3] = {row.size(), rowIndex};
                keyTypeToSpriteID[3] = AllSpriteTextures.size();
                AllSpriteTextures.push_back(Sprite{ static_cast<int>(AllSpriteTextures.size()), std::pair<float, float>{row.size(), rowIndex}, keysTextures[2], 
                 keyWidthsHeights[3].first, keyWidthsHeights[3].second});
                row.push_back(0);
                continue;
            }

            // Weapon handling
            if(token == "K"){
                weaponsPositions[1] = {row.size(), rowIndex};
                weaponTypeToSpriteID[1] = AllSpriteTextures.size();
                AllSpriteTextures.push_back(Sprite{ static_cast<int>(AllSpriteTextures.size()), std::pair<float, float>{row.size(), rowIndex}, weaponsTextures[0],
                 weaponWidthsHeights[1].first, weaponWidthsHeights[1].second});
                row.push_back(0);
                continue;
            }
            else if(token == "P"){
                weaponsPositions[2] = {row.size(), rowIndex};
                weaponTypeToSpriteID[2] = AllSpriteTextures.size();
                AllSpriteTextures.push_back(Sprite{ static_cast<int>(AllSpriteTextures.size()), std::pair<float, float>{row.size(), rowIndex}, weaponsTextures[1],
                 weaponWidthsHeights[2].first, weaponWidthsHeights[2].second});
                row.push_back(0);
                continue;
            }
            else if(token == "S"){
                weaponsPositions[3] = {row.size(), rowIndex};
                weaponTypeToSpriteID[3] = AllSpriteTextures.size();
                AllSpriteTextures.push_back(Sprite{ static_cast<int>(AllSpriteTextures.size()), std::pair<float, float>{row.size(), rowIndex}, weaponsTextures[2],
                 weaponWidthsHeights[3].first, weaponWidthsHeights[3].second});
                row.push_back(0);
                continue;
            }
            
                
            int value = std::stoi(token);      // parses 10, 12, etc.

            // Door handling
            if (value >= 6 && value <= 9) {
                Door d;
                d.openAmount = 0.0f;
                d.opening = false;

                if (value == 6) { d.locked = false; d.keyType = 0; }
                if (value == 7) { d.locked = true;  d.keyType = 1; }
                if (value == 8) { d.locked = true;  d.keyType = 2; }
                if (value == 9) { d.locked = true;  d.keyType = 3; }

                doors[{ rowIndex, row.size() }] = d;
            }

            row.push_back(value);
        }

        Map.push_back(row);
        rowIndex++;
    }
}

void Game::placePlayerAt(int x, int y, float angle) {
    playerPosition = {static_cast<double>(x), static_cast<double>(y)};
    playerAngle = angle;
}
void Game::addWallTexture(const char* filePath)
{
    SDL_Texture* raw = IMG_LoadTexture(renderer.get(), filePath);
    if (!raw) {
        std::cerr << "Failed to load wall texture: "
                  << filePath << " | " << IMG_GetError() << "\n";
        return;
    }

    wallTextures.emplace_back(raw, SDL_DestroyTexture);

    int width = 0, height = 0;
    if (SDL_QueryTexture(raw, nullptr, nullptr, &width, &height) != 0) {
        std::cerr << "Failed to query texture: "
                  << SDL_GetError() << "\n";
        wallTextures.pop_back();   // keep vectors in sync
        return;
    }

    wallTextureWidths.push_back(width);
    wallTextureHeights.push_back(height);
}

void Game::addFloorTexture(const char* filePath) {
    SDL_Texture* raw = IMG_LoadTexture(renderer.get(), filePath);
    if (!raw) {
        std::cerr << "Failed to load floor texture: "
                  << filePath << " | " << IMG_GetError() << "\n";
        return;
    }

    floorTextures.emplace_back(raw, SDL_DestroyTexture);

    int width = 0, height = 0;
    if (SDL_QueryTexture(raw, nullptr, nullptr, &width, &height) != 0) {
        std::cerr << "Failed to query texture: "
                  << SDL_GetError() << "\n";
        floorTextures.pop_back();   // keep vectors in sync
        return;
    }

    floorTextureWidths.push_back(width);
    floorTextureHeights.push_back(height);
}
void Game::addCeilingTexture(const char* filePath) {
    SDL_Texture* raw = IMG_LoadTexture(renderer.get(), filePath);
    if (!raw) {
        std::cerr << "Failed to load ceiling texture: "
                  << filePath << " | " << IMG_GetError() << "\n";
        return;
    }

    ceilingTextures.emplace_back(raw, SDL_DestroyTexture);

    int width = 0, height = 0;
    if (SDL_QueryTexture(raw, nullptr, nullptr, &width, &height) != 0) {
        std::cerr << "Failed to query texture: "
                  << SDL_GetError() << "\n";
        ceilingTextures.pop_back();   // keep vectors in sync
        return;
    }

    ceilingTextureWidths.push_back(width);
    ceilingTextureHeights.push_back(height);
}

void Game::printPlayerPosition(){
    std::cout << "Player Position: (" << playerPosition.first << ", " << playerPosition.second << ")\n";
}
bool Game::isDoor(int tile) {
    return tile >= 6 && tile <=9;
}
bool Game::playerHasKey(int keyType) {
    if(keyType == 0) return true; // no key needed
    for (int key : keysHeld) {
        if (key == keyType) {
            return true;
        }
    }
    return false;
}
static std::string toLower(const std::string &s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return r;
}
void Game::loadAllTextures(const char* filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open texture list file: " << filePath << "\n";
        return;
    }

    enum Section { NONE, WALLS, FLOORS, CEILS, KEYS, WEAPONS };
    Section currentSection = NONE;

    std::string line;
    while (std::getline(file, line)) {

        // Trim leading/trailing spaces
        auto trim = [](std::string &s) {
            while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
            while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
        };
        trim(line);
        if (line.empty()) continue;       // Skip blank lines

        std::string low = toLower(line);

        // Detect section headers case-insensitively
        if (low == "[walls]") {
            currentSection = WALLS;
            continue;
        }
        if (low == "[floors]") {
            currentSection = FLOORS;
            continue;
        }
        if (low == "[ceils]" || low == "[ceil]" || low == "[ceilings]") {
            currentSection = CEILS;
            continue;
        }
        if (low == "[keys]") {
            currentSection = KEYS;
            continue;
        }
        if (low == "[weapons]") {
            currentSection = WEAPONS;
            continue;
        }

        // If it’s not a section header, it must be a file path
        if (currentSection == WALLS) {
            addWallTexture(line.c_str());
        }
        else if (currentSection == FLOORS) {
            addFloorTexture(line.c_str());
        }
        else if (currentSection == CEILS) {
            addCeilingTexture(line.c_str());
        }
        else if (currentSection == KEYS) {
            loadKeysTexture(line.c_str());
        }
        else if (currentSection == WEAPONS) {
            loadWeaponsTexture(line.c_str());
        }
        else {
            std::cerr << "Warning: Path found outside any valid section: " << line << "\n";
        }
    }
}
void Game::clean()
{
    enemyTextures.clear();
    wallTextures.clear();
    floorTextures.clear();
    ceilingTextures.clear();
    doors.clear();
    enemies.clear();

    renderer.reset();
    window.reset();

    IMG_Quit();
    SDL_Quit();

}

void Game::loadEnemyTextures(const char* filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << "\n";
        return;
    }

    std::string line;

    while (std::getline(file, line)) {

        // Remove UTF-8 BOM if present (important for first line)
        if (!line.empty() && static_cast<unsigned char>(line[0]) == 0xEF)
            line.erase(0, 3);

        // Remove comments
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);

        std::istringstream iss(line);

        int a, b;
        std::string path;

        // Expect: <int> <int> <string>
        if (iss >> a >> b >> path) {
            SDL_Texture* texture = IMG_LoadTexture(renderer.get(), path.c_str());
            if (!texture) {
                std::cerr << "Failed to load texture: " << filePath << " Error: " << IMG_GetError() << std::endl;
                return;
            }
            enemyTextures.insert_or_assign(
                {a, b},
                SDLTexturePtr(texture, SDL_DestroyTexture)
            );
        }
        // else: silently ignore malformed / empty lines
    }
    //std::cout<<enemyTextures.size()<<std::endl;
}

void Game::addEnemy(float x, float y, float angle) {
    enemies.push_back(std::make_unique<Enemy>(x, y, angle));
}

bool aabbIntersect(
    float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh
) {
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

bool Game::collidesWithEnemy(float x, float y) {
    for (const auto& e : enemies)
    {
        if (aabbIntersect(
            x, y,
            playerSquareSize, playerSquareSize,
            e->get_position().first, e->get_position().second,
            e->get_size(), e->get_size()
        )) {
            return true;
        }
    }
    return false;
}

bool Game::rayCastEnemyToPlayer(const Enemy& enemy) {
    float ex = enemy.get_position().first;
    float ey = enemy.get_position().second;

    float px = playerPosition.first;
    float py = playerPosition.second;

    // Direction vector
    float dx = px - ex;
    float dy = py - ey;

    float rayLength = std::hypot(dx, dy);
    if (rayLength < 0.0001f)
        return true;

    dx /= rayLength;
    dy /= rayLength;

    // Current grid square
    int mapX = int(std::floor(ex));
    int mapY = int(std::floor(ey));

    int targetX = int(std::floor(px));
    int targetY = int(std::floor(py));

    // Ray step direction
    int stepX = (dx < 0) ? -1 : 1;
    int stepY = (dy < 0) ? -1 : 1;

    // Distance to first grid boundary
    float sideDistX;
    float sideDistY;

    float deltaDistX = (dx == 0) ? 1e30f : std::abs(1.0f / dx);
    float deltaDistY = (dy == 0) ? 1e30f : std::abs(1.0f / dy);

    if (dx < 0)
        sideDistX = (ex - mapX) * deltaDistX;
    else
        sideDistX = (mapX + 1.0f - ex) * deltaDistX;

    if (dy < 0)
        sideDistY = (ey - mapY) * deltaDistY;
    else
        sideDistY = (mapY + 1.0f - ey) * deltaDistY;

    // DDA loop
    while (true) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
        }

        // Bounds check
        if (mapY < 0 || mapY >= (int)Map.size() ||
            mapX < 0 || mapX >= (int)Map[0].size())
            return false;

        // Hit wall
        if (Map[mapY][mapX] != 0)
            return false;

        // Reached player cell
        if (mapX == targetX && mapY == targetY)
            return true;
    }
}

bool Game::canShootEnemy(float dist){
    const float MIN_DIST = 1.0f;
    const float MAX_DIST = 64.0f;   

    dist = std::clamp(dist, MIN_DIST, MAX_DIST);
    float t = (dist - MIN_DIST) / (MAX_DIST - MIN_DIST);

    // Quadratic falloff (feels very Wolf-like)
    int errorDivisor = ((int) (weapons[currentWeapon].accuracy - 1) * (1.0f - t * t)) + 1;
    return (rand() % errorDivisor) != 0;
}

void Game::loadEnemies(const char* filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open enemy file: " << filePath << '\n';
        return;
    }

    float x, y;
    std::string line;

    while (std::getline(file, line))
    {
        // Skip empty lines or comments
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        if (!(iss >> x >> y))
        {
            std::cerr << "Invalid enemy entry: " << line << '\n';
            continue;
        }

        addEnemy(x, y, 0.0f);
    }

    file.close();
}

void Game::loadKeysTexture(const char* filePath)
{
    SDL_Texture* raw = IMG_LoadTexture(renderer.get(), filePath);
    if (!raw) {
        std::cerr << "Failed to load key texture: "
                  << filePath << " | " << IMG_GetError() << "\n";
        return;
    }
    int keyType = keysTextures.size() + 1;
    int height = 0, width = 0;
    if (SDL_QueryTexture(raw, nullptr, nullptr, &width, &height) != 0) {
        std::cerr << "Failed to query texture: "
                  << SDL_GetError() << "\n";
        SDL_DestroyTexture(raw);
        return;
    }
    keysTextures.emplace_back(raw, SDL_DestroyTexture);
    keyWidthsHeights[keyType] = std::make_pair(width, height);
}

void Game::acquireKey(int keyType) {
    if (!playerHasKey(keyType)) {
        keysHeld.push_back(keyType);
        std::cout << "Acquired key of type: " << keyType << "\n";
        AudioManager::playSFX("pickup", MIX_MAX_VOLUME / 2);
    }
}
void Game::loadWeaponsTexture(const char* filePath)
{
    SDL_Texture* raw = IMG_LoadTexture(renderer.get(), filePath);
    if (!raw) {
        std::cerr << "Failed to load weapon texture: "
                  << filePath << " | " << IMG_GetError() << "\n";
        return;
    }
    int height = 0, width = 0;
    if (SDL_QueryTexture(raw, nullptr, nullptr, &width, &height) != 0) {
        std::cerr << "Failed to query texture: "
                  << SDL_GetError() << "\n";
        SDL_DestroyTexture(raw);
        return;
    }
    int weaponsType = weaponsTextures.size() + 1;
    weaponWidthsHeights[weaponsType] = std::make_pair(width, height);
    weaponsTextures.emplace_back(raw, SDL_DestroyTexture);
}   

void Game::acquireWeapon(int weaponType) {
    if (weaponType < 1 || weaponType > 3) {
        std::cerr << "Invalid weapon type: " << weaponType << "\n";
        return;
    }
    switch(weaponType){
        case 1:
            weapons[1] = weapon({1, 100, 0, 2.0f, 0.0f, 8.0f, "knife"});
            break;
        case 2:
            weapons[2] = weapon({2, 4, 30, 70.0f, 0.2f, 16.0f, "pistol"});
            break;
        case 3:
            weapons[3] = weapon({3, 6, 50, 90.0f, 0.5f, 24.0f, "rifle"});
            break;
        default:
            std::cerr << "Unhandled weapon type: " << weaponType << "\n";
    }
    //{1, 100, 0, true, 2.0f, 0.0f, 8.0f, "knife"},   // knife (K)
    //{2, 4, 0, false, 70.0f, 0.2f, 16.0f, "pistol"},  // pistol (P)
    //{3, 6, 0, false, 90.0f, 0.5f, 24.0f, "rifle"}   // rifle (S)
    currentWeapon = weaponType;
    AudioManager::playSFX("pickup", MIX_MAX_VOLUME / 2);
}

bool Game::playerHasWeapon(int weaponType) {
    for (const auto& [i, w] : weapons) {
        if (i == weaponType) {
            return true;
        }
    }
    return false;
}