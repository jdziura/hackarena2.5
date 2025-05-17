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
        if (bot.isWall[x][y]) {
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
    // if (heldItem == SecondaryItemType::Laser) {
    //     bound = dim;
    // }

    for (int i = 1; i < bound; i++) {
        x += dx;
        y += dy;
        if (!isValid(Position(x, y), dim)) {
            break;
        }
        if (bot.isWall[x][y]) {
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
    initMyTankHelper(gameState);
    myTurretDir = myTank.turret.direction;
    assert (myTank.turret.bulletCount.has_value());
    myBulletCount = myTank.turret.bulletCount.value();
}

void Bot::initMyTanks(const GameState& gameState) {
    for (int i = 0; i < 2; i++) {
        tankState[i].initMyTank(gameState);
    }
}

ResponseVariant Bot::NextMove(const GameState& gameState) {
    std::cout << "Chuj " << gameState.time << ":" << std::endl;
    PrintMap(gameState.map.tiles);

    // Find our tank and gather information
    std::optional<Tank> ourTank;
    std::optional<std::pair<int, int>> ourPosition;
    for (size_t x = 0; x < gameState.map.tiles.size(); ++x) {
        for (size_t y = 0; y < gameState.map.tiles[x].size(); ++y) {
            const Tile& tile = gameState.map.tiles[x][y];
            for (const TileVariant& object : tile.objects) {
                if (std::holds_alternative<Tank>(object)) {
                    const Tank& tank = std::get<Tank>(object);
                    if (tank.ownerId == myId) {
                        ourTank = tank;
                        ourPosition = std::make_pair(x, y);
                        break;
                    }
                }
            }
            if (ourTank.has_value()) break;
        }
        if (ourTank.has_value()) break;
    }

    // Create a random device and generator with a better seed
    std::random_device rd;
    std::mt19937 gen(rd() + gameState.time); // Add time to ensure different results each turn

    // Choose an action with weighted probabilities
    std::discrete_distribution<int> actionDist({25, 30, 35, 10}); // Weights for actions: Rotate, Move, Ability, Wait
    int action = actionDist(gen);

    // Print what we're about to do
    const char* actionNames[] = {"Rotate", "Move", "Use Ability", "Wait"};
    std::cout << "Choosing action: " << actionNames[action] << std::endl;

    switch (action) {
        case 0: { // Rotate
            std::uniform_int_distribution<int> rotationDist(0, 2); // Include 'none' rotation
            RotationDirection tankRot = static_cast<RotationDirection>(rotationDist(gen));
            RotationDirection turretRot = static_cast<RotationDirection>(rotationDist(gen));
            std::cout << "  Tank rotation: " <<
                (tankRot == RotationDirection::left ? "left" :
                 tankRot == RotationDirection::right ? "right" : "none") << std::endl;
            std::cout << "  Turret rotation: " <<
                (turretRot == RotationDirection::left ? "left" :
                 turretRot == RotationDirection::right ? "right" : "none") << std::endl;
            return Rotate{tankRot, turretRot};
        }
        case 1: { // Move
            std::uniform_int_distribution<int> moveDist(0, 1);
            MoveDirection moveDir = static_cast<MoveDirection>(moveDist(gen));
            std::cout << "  Moving: " << (moveDir == MoveDirection::forward ? "forward" : "backward") << std::endl;
            return Move{moveDir};
        }
        case 2: { // AbilityUse
            // If we have tank info, determine which abilities we can use
            if (ourTank.has_value()) {
                std::vector<AbilityType> availableAbilities;
                // Basic abilities any tank can use
                availableAbilities.push_back(AbilityType::fireBullet);
                // Check tank type for specific abilities
                if (ourTank->tankType == TankType::Heavy) {
                    availableAbilities.push_back(AbilityType::useLaser);
                    availableAbilities.push_back(AbilityType::dropMine);
                } else if (ourTank->tankType == TankType::Light) {
                    availableAbilities.push_back(AbilityType::fireDoubleBullet);
                    availableAbilities.push_back(AbilityType::useRadar);
                }
                // Add healing and stun bullets for variety
                availableAbilities.push_back(AbilityType::fireHealingBullet);
                availableAbilities.push_back(AbilityType::fireStunBullet);
                // Randomly select from available abilities
                std::uniform_int_distribution<int> abilityDist(0, availableAbilities.size() - 1);
                AbilityType ability = availableAbilities[abilityDist(gen)];
                // Print the ability being used
                std::string abilityName;
                switch(ability) {
                    case AbilityType::fireBullet: abilityName = "fire bullet"; break;
                    case AbilityType::useLaser: abilityName = "use laser"; break;
                    case AbilityType::fireDoubleBullet: abilityName = "fire double bullet"; break;
                    case AbilityType::useRadar: abilityName = "use radar"; break;
                    case AbilityType::dropMine: abilityName = "drop mine"; break;
                    case AbilityType::fireHealingBullet: abilityName = "fire healing bullet"; break;
                    case AbilityType::fireStunBullet: abilityName = "fire stun bullet"; break;
                }
                std::cout << "  Using ability: " << abilityName << std::endl;
                return AbilityUse{ability};
            } else {
                // Fallback if we don't have tank info
                std::uniform_int_distribution<int> abilityDist(0, 6);
                AbilityType ability = static_cast<AbilityType>(abilityDist(gen));
                return AbilityUse{ability};
            }
        }
        case 3: // Wait
        default: {
            std::cout << "  Waiting this turn" << std::endl;
            // Occasionally try to capture a zone instead of just waiting
            std::uniform_int_distribution<int> captureOrWaitDist(0, 1);
            bool shouldCapture = captureOrWaitDist(gen) == 1;
            if (shouldCapture) {
                std::cout << "  Actually, trying to capture a zone" << std::endl;
                return CaptureZone{};
            } else {
                return Wait{};
            }
        }
    }
}
