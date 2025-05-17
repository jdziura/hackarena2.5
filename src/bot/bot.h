#pragma once

#include "../processed-packets.h"
#include "utils.h"

const int LIGHT = 0;
const int HEAVY = 1;


class Bot {
 public:
    /// DO NOT DELETE
    /// time in milliseconds after which the NextMove() answer is not sent to server, CAN BE CHANGED WHENEVER YOU WANT
    int skipResponse = 99;
	std::string myId;
    std::string teamName;

	Bot();
	void Init(const LobbyData& lobbyData);
	ResponseVariant NextMove(const GameState& gameState);
	void OnGameEnded(const EndGameLobby& endGameLobby);
    void OnWarningReceived(WarningType warningType, std::optional<std::string>& message);
    void OnGameStarting();
	void PrintMap(const std::vector<std::vector<Tile>>& tiles);

    LobbyData lobbyData;
    int dim;
    KnowledgeMap knowledgeMap;
    std::vector<std::vector<bool>> isWall;
	std::vector<std::vector<int>> wallType;
    std::vector<std::vector<char>> zoneName;
    std::vector<std::vector<bool>> isVisible;

    class TankState {
    public:
        Bot& bot; 
        bool isHeavy;
        std::string myId;
    
        Tank myTank;
        OrientedPosition myPos;
        Direction myTurretDir;
        int myBulletCount = 3;
    
        TankState(Bot& _bot, bool _isHeavy) : bot(_bot), isHeavy(_isHeavy) {
            myId = bot.myId;
        }
    
        bool canSeeEnemy(const GameState& gameState) const;
        bool willFireHitForSure(const GameState& gameState) const;
        bool willBeHitByBullet(const GameState& gameState, const OrientedPosition& pos) const;
        void initMyTankHelper(const GameState& gameState);
        void initMyTank(const GameState& gameState);
    
        // TODO: Rerwrite
        template<class F>
        std::optional<ResponseVariant> shootIf(
                const GameState& gameState, 
                F&& f, 
                bool useLaserIfPossible = true, 
                bool useDoubleBulletIfPossible = true
        ) {
            return std::nullopt;
    
            // bool ignoreBulletCount = (heldItem == SecondaryItemType::DoubleBullet && useDoubleBulletIfPossible);
            // ignoreBulletCount |=     (heldItem == SecondaryItemType::Laser && useLaserIfPossible);
    
            // if (myBulletCount == 0 && !ignoreBulletCount)
            // {
            //     return std::nullopt;
            // }
    
            // if (!f(gameState)) {
            //     return std::nullopt;
            // }
    
            // if (useLaserIfPossible && heldItem == SecondaryItemType::Laser) {
            //     return AbilityUse{AbilityType::useLaser};
            // }
    
            // if (useDoubleBulletIfPossible && heldItem == SecondaryItemType::DoubleBullet) {
            //     return AbilityUse{AbilityType::fireDoubleBullet};
            // }
            
            // return AbilityUse{AbilityType::fireBullet};
        }
    };
    
    TankState tankState[2] = {TankState(*this, false), TankState(*this, true)};

    void onFirstNextMove(const GameState& gameState);
    void initIsWall(const std::vector<std::vector<Tile>>& tiles);
    void initWallType(const std::vector<std::vector<Tile>>& tiles);
    void initZoneName(const std::vector<std::vector<Tile>>& tiles);
    void initMyTanks(const GameState& gameState);

    struct BfsResult {
        MoveOrRotation move;
        OrientedPosition finalPos;
        int eta;
    };

    template<class F>
    std::optional<BfsResult> bfs(const OrientedPosition& start, F&& f) {
        std::queue<std::pair<OrientedPosition, int>> q;
        std::vector<std::vector<std::vector<bool>>> visited(dim, std::vector<std::vector<bool>>(dim, std::vector<bool>(4, false)));
        std::vector<std::vector<std::vector<MoveOrRotation>>> from(dim, std::vector<std::vector<MoveOrRotation>>(dim, std::vector<MoveOrRotation>(4))); // i, j, dir
        q.push({start, 0});
        visited[start.pos.x][start.pos.y][getDirId(start.dir)] = true;

        bool found = false;
        OrientedPosition finish;
        int eta = -1;

        while (!q.empty()) {
            auto [pos, timer] = q.front();
            q.pop();

            if (f(pos, timer)) {
                found = true;
                finish = pos;
                eta = timer;
                break;
            }

            for (const MoveOrRotation &move : ALL_ACTIONS) {
                auto nextPos = afterMove(pos, move);
                if (!isValid(nextPos, dim)) {
                    continue;
                }

                int x = nextPos.pos.x;
                int y = nextPos.pos.y;
                int dir = getDirId(nextPos.dir);

                if (wallType[x][y] > 0) {
                    continue;
                }

                if (knowledgeMap.containsMine(nextPos.pos)) {
                    continue;
                }

                if (knowledgeMap.isOnBulletTraj(x, y)) {
                    continue;
                }

                if (visited[x][y][dir]) {
                    continue;
                }

                visited[x][y][dir] = true;
                from[x][y][dir] = reversed(move);
                q.push({nextPos, timer + 1});
            }
        }

        if (!found) {
            return std::nullopt;
        }

        OrientedPosition cur = finish;
        MoveOrRotation lastMove = from[cur.pos.x][cur.pos.y][getDirId(cur.dir)];

        while (cur != start) {
            lastMove = from[cur.pos.x][cur.pos.y][getDirId(cur.dir)];
            cur.move(lastMove);
        }

        lastMove = reversed(lastMove);
        return BfsResult{lastMove, finish, eta};
    }
};
