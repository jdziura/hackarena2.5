#include "bot.h"
#include <utility>
#include <random>
#include <iostream>
#include <map>
#include <string>

void Bot::OnWarningReceived(WarningType warningType, std::optional<std::string> &message) {
    std::cout << "Warning received: ";
    switch (warningType) {
        case WarningType::CustomWarning:
            std::cout << "Custom warning";
            if (message.has_value()) {
                std::cout << " - " << message.value();
            }
            break;
        case WarningType::PlayerAlreadyMadeActionWarning:
            std::cout << "Player already made an action";
            break;
        case WarningType::ActionIgnoredDueToDeadWarning:
            std::cout << "Action ignored because tank is dead";
            break;
        case WarningType::SlowResponseWarning:
            std::cout << "Response was too slow";
            break;
        default:
            std::cout << "Unknown warning type";
    }
    std::cout << std::endl;
}

void Bot::OnGameStarting() {
    std::cout << "Game is starting!" << std::endl;
}

void Bot::OnGameEnded(const EndGameLobby& endGameLobby) {
    std::cout << "Game has ended! Final scores:" << std::endl;
    for (const auto& team : endGameLobby.teams) {
        std::cout << "Team " << team.name << ": " << team.score << " points" << std::endl;
        std::cout << "  Players:" << std::endl;
        for (const auto& player : team.players) {
            std::cout << "    - " << player.id
                      << " (Tank type: " << (player.tankType == TankType::Light ? "Light" : "Heavy")
                      << ", Kills: " << player.kills << ")" << std::endl;
        }
    }
    std::cout << "Thank you for playing!" << std::endl;
}

void Bot::PrintMap(const std::vector<std::vector<Tile>>& tiles) {
    auto rows = tiles.size();
    auto cols = tiles[0].size();

    std::vector<std::vector<std::string>> map(rows, std::vector<std::string>(cols, " "));

    // Map for converting wall types to symbols
    std::map<WallType, char> wallSymbols = {
        {WallType::solid, '#'},
        {WallType::penetrable, '='}
    };

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            const Tile& tile = tiles[i][j];

            // Check for tile objects
            bool hasObject = false;
            for (const TileVariant& object : tile.objects) {
                if (std::holds_alternative<Wall>(object)) {
                    const Wall& wall = std::get<Wall>(object);
                    map[i][j] = wallSymbols[wall.type]; // Display different wall types
                    hasObject = true;
                    break; // Found wall, no need to check further
                } else if (std::holds_alternative<Tank>(object)) {
                    if (auto tankPtr = std::get_if<Tank>(&object)) {
                        auto tankId = tankPtr->ownerId;
                        // Use '@' for player's tank, 'T' for enemy tanks
                        std::string tankSymbol = (tankId != myId) ? "T" : "@";
                        tankSymbol += (tankPtr->direction == Direction::up) ? "^" :
                                      (tankPtr->direction == Direction::down) ? "v" :
                                      (tankPtr->direction == Direction::left) ? "<" : ">";
                        // Add tank type indicator
                        tankSymbol += (tankPtr->tankType == TankType::Light) ? "L" : "H";
                        map[i][j] = tankSymbol;
                        hasObject = true;
                        break;
                    }
                } else if (std::holds_alternative<Mine>(object)) {
                    if (auto minePtr = std::get_if<Mine>(&object)) {
                        // Show if mine is about to explode
                        map[i][j] = minePtr->explosionRemainingTicks.has_value() ? '*' : 'X';
                        hasObject = true;
                    }
                } else if (std::holds_alternative<Laser>(object)) {
                    if (auto laserPtr = std::get_if<Laser>(&object)) {
                        map[i][j] = (laserPtr->orientation == LaserOrientation::horizontal) ? '═' : '║';
                        hasObject = true;
                    }
                } else if (std::holds_alternative<Bullet>(object)) {
                    if (auto bulletPtr = std::get_if<Bullet>(&object)) {
                        char bulletChar = '•'; // Default bullet symbol

                        // Different symbols for different bullet types
                        switch (bulletPtr->type) {
                            case BulletType::basic:
                                bulletChar = '•';
                                break;
                            case BulletType::doubleBullet:
                                bulletChar = '◉';
                                break;
                            case BulletType::healing:
                                bulletChar = '+';
                                break;
                            case BulletType::stun:
                                bulletChar = '⚡';
                                break;
                        }

                        map[i][j] = bulletChar;
                        hasObject = true;
                    }
                }
            }

            // If no object found, check zone info
            if (!hasObject) {
                if (tile.zoneName != '?') {
                    map[i][j] = tile.zoneName; // Print zone name
                } else {
                    map[i][j] = ' '; // Empty space
                }
            }
        }
    }

    // Print the map with a border
    std::cout << "┌";
    for (size_t i = 0; i < cols * 2 - 1; ++i) {
        std::cout << "─";
    }
    std::cout << "┐\n";

    // Print the map
    for (const auto& row : map) {
        std::cout << "│";
        for (size_t i = 0; i < row.size(); ++i) {
            std::cout << row[i];
            if (i < row.size() - 1) {
                std::cout << " ";
            }
        }
        std::cout << "│\n";
    }

    std::cout << "└";
    for (size_t i = 0; i < cols * 2 - 1; ++i) {
        std::cout << "─";
    }
    std::cout << "┘\n";
    // Print legend
    std::cout << "Legend: # - Solid Wall, = - Penetrable Wall, @ - Your Tank, T - Enemy Tank\n";
    std::cout << "        • - Basic Bullet, ◉ - Double Bullet, + - Healing Bullet, ⚡ - Stun Bullet\n";
    std::cout << "        X - Mine, * - Exploding Mine, ═ - Horizontal Laser, ║ - Vertical Laser\n";
    std::cout << "---------------------------------------------------" << std::endl;
}

Bot::Bot() = default;

void Bot::Init(const LobbyData& _lobbyData) {
    lobbyData = _lobbyData;
    dim = lobbyData.gridDimension;
    myId = lobbyData.myId;
    teamName = lobbyData.teamName;
    skipResponse = lobbyData.broadcastInterval - 1;
    knowledgeMap.init(dim);

    // Print basic information about the game
    std::cout << "Bot initialized with ID: " << myId << std::endl;
    std::cout << "Team name: " << lobbyData.teamName << std::endl;
    std::cout << "Grid dimension: " << lobbyData.gridDimension << std::endl;
    std::cout << "Number of players: " << lobbyData.numberOfPlayers << std::endl;
    // Print team information
    std::cout << "Teams in game:" << std::endl;
    for (const auto& team : lobbyData.teams) {
        std::cout << "  Team: " << team.name << " (Players: " << team.players.size() << ")" << std::endl;
    }
}

void Bot::initIsWall(const std::vector<std::vector<Tile>>& tiles) {
    isWall = std::vector<std::vector<bool>>(dim, std::vector<bool>(dim, false));
    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j < dim; ++j) {
            isWall[i][j] = std::any_of(tiles[i][j].objects.begin(), tiles[i][j].objects.end(), [](const TileVariant& object) {
                return std::holds_alternative<Wall>(object);
            });
        }
    }
}

void Bot::initWallType(const std::vector<std::vector<Tile>>& tiles) {
    wallType = std::vector<std::vector<int>>(dim, std::vector<int>(dim, 0));
    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j < dim; ++j) {
            for (const TileVariant& object : tiles[i][j].objects) {
                if (std::holds_alternative<Wall>(object)) {
                    const Wall& wall = std::get<Wall>(object);
                    if (wall.type == WallType::solid) {
                        wallType[i][j] = 2; // Solid wall
                    } else if (wall.type == WallType::penetrable) {
                        wallType[i][j] = 1; // Penetrable wall
                    }
                }
            }
        }
    }
}

void Bot::initZoneName(const std::vector<std::vector<Tile>>& tiles) {
    zoneName = std::vector<std::vector<char>>(dim, std::vector<char>(dim, '?'));
    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j < dim; ++j) {
            zoneName[i][j] = tiles[i][j].zoneName;
        }
    }
}

void Bot::onFirstNextMove(const GameState& gameState) {
    initIsWall(gameState.map.tiles);
    initWallType(gameState.map.tiles);
    // print wallType;
    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j < dim; ++j) {
            std::cout << wallType[i][j] << " ";
        }
        std::cout << std::endl;
    }
    initZoneName(gameState.map.tiles);
}

bool Bot::TankState::canSeeEnemy(const GameState& gameState) const {
    const int dim = bot.dim;
    int x = myPos.pos.x;
    int y = myPos.pos.y;
    int dir = getDirId(myTurretDir);

    auto [dx, dy] = Position::DIRECTIONS[dir];

    for (int i = 1; i < dim; ++i) {
        x += dx;
        y += dy;
        if (!isValid(Position(x, y), dim)) {
            break;
        }
        if (bot.wallType[x][y] == 2) {
            break;
        }
        // if (!gameState.map.tiles[x][y].isVisible) {
        if (!bot.knowledgeMap.isVisible[x][y]) {
            break;
        }
        for (const TileVariant& object : gameState.map.tiles[x][y].objects) {
            if (std::holds_alternative<Tank>(object)) {
                const Tank& tank = std::get<Tank>(object);
                if (tank.ownerId != myId) {
                    return true;
                }
            }
        }
    }

    return false;
}

inline bool isOneOfMyTanks(const Tank& tank) {
    return tank.turret.bulletCount.has_value();
}

bool Bot::TankState::willBeHitByBullet(const GameState& gameState, const OrientedPosition& pos) const {
    int x = pos.pos.x;
    int y = pos.pos.y;
    return bot.knowledgeMap.willBeHitByBulletInNextMove(x, y);
}

bool Bot::TankState::willFireHitForSure(const GameState& gameState) const {
    const int dim = bot.dim;
    int x = myPos.pos.x;
    int y = myPos.pos.y;
    int dir = getDirId(myTurretDir);

    auto [dx, dy] = Position::DIRECTIONS[dir];

    int bound = 2;
    // TODO: check if can fire laser and are heavy

    for (int i = 1; i < bound; i++) {
        x += dx;
        y += dy;
        if (!isValid(Position(x, y), dim)) {
            break;
        }
        if (bot.wallType[x][y] == 2) {
            break;
        }
        // if (!gameState.map.tiles[x][y].isVisible) {
        if (!bot.knowledgeMap.isVisible[x][y]) {
            break;
        }
        for (const TileVariant& object : gameState.map.tiles[x][y].objects) {
            if (std::holds_alternative<Tank>(object)) {
                const Tank& tank = std::get<Tank>(object);
                if (tank.ownerId == myId) {
                    continue;
                }
                if (isParallel(myTurretDir, tank.direction)) {
                    return true;
                }
            }
        }
    }

    return false;
}

void Bot::TankState::initMyTankHelper(const GameState& gameState) {
    for (int i = 0; i < bot.dim; i++) {
        for (int j = 0; j < bot.dim; j++) {
            for (const TileVariant& object : gameState.map.tiles[i][j].objects) {
                if (std::holds_alternative<Tank>(object)) {
                    const Tank& tank = std::get<Tank>(object);
                    if (tank.ownerId == myId) {
                        myTank = tank;
                        myPos = OrientedPosition{Position(i, j), tank.direction};
                        return;
                    }
                }
            }
        }
    }
}

void Bot::TankState::initMyTank(const GameState& gameState) {
    myId = bot.myId;
    initMyTankHelper(gameState);
    myTurretDir = myTank.turret.direction;
    assert (myTank.turret.bulletCount.has_value());
    myBulletCount = myTank.turret.bulletCount.value();
}

void Bot::initMyTanks(const GameState& gameState) {
    for (int i = 0; i < 2; i++) {
        tankState[i].initMyTank(gameState);
    }

    myTankIdx = -1;
    for (int i = 0; i < 2; i++) {
        if (tankState[i].myId == myId) {
            myTankIdx = i;
            break;
        }
    }

    assert(myTankIdx != -1);
}

std::optional<ResponseVariant> Bot::TankState::NextMove(const GameState& gameState) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::optional<ResponseVariant> response;
    
    response = shootIfWillFireHitForSure(gameState);
    if (response.has_value()) 
        return response.value();


    // TODO: jakieś gówno XD niech będzie na razie ale potem można wyjebać
    if (lastPos == myPos && gen() % 4 == 0) {
        response = shootIfSeeingEnemy(gameState);
        if (response.has_value())
            return response.value();
        return BeDrunkInsideZone(gameState);
    }

    auto bullet = closestBullet(gameState, myPos.pos);
    auto isOnBulletLine = [&](const OrientedPosition& oPos, int timer) {
        return oPos.pos.x != bullet.x && oPos.pos.y != bullet.y;
    };

    if (bullet.x != 1e9) {
        response = bfsStrategy(gameState, isOnBulletLine);
        if (response.has_value())
            return response.value();
    }

    auto isZone = targetZone(bot.zoneName);
    if (isZone(myPos, 0)) {
        return BeDrunkInsideZone(gameState);
    }
    
    response = bfsStrategy(gameState, isZone);
    if (response.has_value())
        return response.value();
    
    // TODO: dziwny przypadek, jestśmy odizolowanie od strefy, można przemyśleć co z tym zrobić
    return BeDrunkInsideZone(gameState);
}

ResponseVariant Bot::NextMove(const GameState& gameState) {
    std::cout << "Chuj " << gameState.time << ":" << std::endl;
    PrintMap(gameState.map.tiles);

    if (gameState.time == 1) {
        onFirstNextMove(gameState);
    }

    for (int i = 0; i < 2; i++) {
        tankState[i].lastPos = tankState[i].myPos;
    }

    initMyTanks(gameState);
    knowledgeMap.update(gameState);

    std::optional<ResponseVariant> response;
    
    // mój czołg niezależnie robi ruch od reszty świata
    response = tankState[myTankIdx].NextMove(gameState);

    // na razie forcujemy żeby się nie komunikowali i najwyżej losowy ruch robili
    assert(response.has_value());

    return response.value();
}


std::optional<ResponseVariant> Bot::TankState::shootIfSeeingEnemy(
    const GameState& gameState, 
    bool useLaserIfPossible, 
    bool useDoubleBulletIfPossible
) {
    return shootIf(gameState, [&](const GameState& gameState) {
        return canSeeEnemy(gameState);
    }, useLaserIfPossible, useDoubleBulletIfPossible);
}

std::optional<ResponseVariant> Bot::TankState::shootIfWillFireHitForSure(
    const GameState& gameState, 
    bool useLaserIfPossible, 
    bool useDoubleBulletIfPossible
) {
    return shootIf(gameState, [&](const GameState& gameState) {
        return willFireHitForSure(gameState);
    }, useLaserIfPossible, useDoubleBulletIfPossible);
}

std::optional<ResponseVariant> Bot::TankState::rotateToEnemy(const GameState& gameState) {
    auto isVisibleEnemy = [&](const OrientedPosition& pos, int timer) {
        if (!bot.knowledgeMap.isVisible[pos.pos.x][pos.pos.y])
            return false;

        for (const TileVariant& object : gameState.map.tiles[pos.pos.x][pos.pos.y].objects) {
            if (std::holds_alternative<Tank>(object)) {
                const Tank& tank = std::get<Tank>(object);
                if (tank.ownerId != myId) {
                    return true;
                }
            }
        }

        return false;
    };

    auto isPotentialEnemy = [&](const OrientedPosition& pos, int timer) {
        if (bot.knowledgeMap.isVisible[pos.pos.x][pos.pos.y])
            return false;

        for (const KnowledgeTileVariant& object : bot.knowledgeMap.tiles[pos.pos.x][pos.pos.y].objects) {
            if (std::holds_alternative<Tank>(object.object)) {
                const Tank& tank = std::get<Tank>(object.object);
                if (tank.ownerId != myId) {
                    return true;
                }
            }
        }

        return false;
    };

    auto result = bot.bfs(myPos, isVisibleEnemy);
    if (!result.has_value()) {
        result = bot.bfs(myPos, isPotentialEnemy);
        if (!result.has_value()) {
            return std::nullopt;
        }
    }

    auto enemyPos = result.value().finalPos;

    auto dx = enemyPos.pos.x - myPos.pos.x;
    auto dy = enemyPos.pos.y - myPos.pos.y;

    Direction desiredTankDir;
    Direction desiredTurretDir;

    if (dx >= abs(dy)) {
        desiredTurretDir = Direction::down;
        if (dy >= 0) {
            desiredTankDir = Direction::right;
        } else {
            desiredTankDir = Direction::left;
        }
    } else if (dx <= -abs(dy)) {
        desiredTurretDir = Direction::up;
        if (dy >= 0) {
            desiredTankDir = Direction::right;
        } else {
            desiredTankDir = Direction::left;
        }
    } else if (dy >= abs(dx)) {
        desiredTurretDir = Direction::right;
        if (dx >= 0) {
            desiredTankDir = Direction::down;
        } else {
            desiredTankDir = Direction::up;
        }
    } else if (dy <= -abs(dx)) {
        desiredTurretDir = Direction::left;
        if (dx >= 0) {
            desiredTankDir = Direction::down;
        } else {
            desiredTankDir = Direction::up;
        }
    }

    auto tankRot = getRotationTo(myPos.dir, desiredTankDir);
    auto turretRot = getRotationTo(myTurretDir, desiredTurretDir);

    if (tankRot == RotationDirection::none && turretRot == RotationDirection::none) {
        std::random_device rd;
        std::mt19937 gen(rd());

        if (gen() % 4 != 0) {
            auto nxtPos = afterMove(myPos, MoveDirection::forward);
            if (isValid(nxtPos.pos, bot.dim) && !bot.isWall[nxtPos.pos.x][nxtPos.pos.y]) {
                return Move{MoveDirection::forward};
            }
            return BeDrunkInsideZone(gameState);
        }

        auto nxtPos = afterMove(myPos, MoveDirection::backward);
        switch (gen() % 3) {
            case 0:
                if (isValid(nxtPos.pos, bot.dim) && !bot.isWall[nxtPos.pos.x][nxtPos.pos.y]) {
                    return Move{MoveDirection::backward};
                }
                return BeDrunkInsideZone(gameState);
            case 1:
                return Wait{};
            case 2:
                return BeDrunkInsideZone(gameState);
        }
    }

    return Rotate{tankRot, turretRot};
}

ResponseVariant Bot::TankState::BeDrunkInsideZone(const GameState& gameState) {
    std::random_device rd;
    std::mt19937 gen(rd());

    if (gen() % 2 == 0) {
        bool canMoveForward = canMoveForwardInsideZone(myPos);
        bool canMoveBackward = canMoveBackwardInsideZone(myPos);
        if (canMoveForward && canMoveBackward) {
            auto randomMove = static_cast<MoveDirection>(gen() % 2);
            return Move{randomMove};
        } else if (canMoveForward) {
            return Move{MoveDirection::forward};
        } else if (canMoveBackward) {
            return Move{MoveDirection::backward};
        }
    }

    auto randomRotation1 = static_cast<RotationDirection>(gen() % 2);
    auto randomRotation2 = static_cast<RotationDirection>(gen() % 2);
    return Rotate{randomRotation1, randomRotation2};
}

bool Bot::TankState::canMoveForwardInsideZone(const OrientedPosition& pos) const {
    auto [dx, dy] = Position::DIRECTIONS[getDirId(pos.dir)];
    auto nextPos = pos;
    nextPos.pos.x += dx;
    nextPos.pos.y += dy;
    return isValid(nextPos.pos, bot.dim) && !bot.isWall[nextPos.pos.x][nextPos.pos.y] && bot.zoneName[nextPos.pos.x][nextPos.pos.y] != '?';
}

bool Bot::TankState::canMoveBackwardInsideZone(const OrientedPosition& pos) const {
    auto [dx, dy] = Position::DIRECTIONS[getDirId(pos.dir)];
    auto nextPos = pos;
    nextPos.pos.x -= dx;
    nextPos.pos.y -= dy;
    return isValid(nextPos.pos, bot.dim) && !bot.isWall[nextPos.pos.x][nextPos.pos.y] && bot.zoneName[nextPos.pos.x][nextPos.pos.y] != '?';
}
