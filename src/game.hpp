#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace cr {

enum class Suit { Sword, Shield, Potion, Curse };

struct Card {
    Suit suit = Suit::Sword;
    int value = 0;
    std::uint32_t id = 0;
};

struct Monster {
    std::string name;
    int hp = 1;
};

struct DeckEntry {
    Suit suit = Suit::Sword;
    int count = 0;
    int minValue = 1;
    int maxValue = 1;
};

struct Config {
    int playerHp = 20;
    int handSize = 4;
    std::vector<Monster> monsters;
    std::vector<DeckEntry> deck;
};

// Loads assets/config.json. Throws std::runtime_error on failure.
Config loadConfig(const std::string& path);

const char* suitLabel(Suit suit);

// Pure game rules: no SDL, no I/O. This is the part worth unit-testing and
// the part that add-on packs will eventually feed with new content.
class Game {
public:
    Game(Config config, std::uint32_t seed);

    void restart();

    // Plays the card in the given hand slot. Returns false when the click did
    // nothing (empty slot, run already over).
    bool playCard(int handIndex);

    int hp() const { return hp_; }
    int maxHp() const { return maxHp_; }
    int block() const { return block_; }
    int score() const { return score_; }
    int turn() const { return turn_; }

    bool won() const { return won_; }
    bool lost() const { return lost_; }
    bool over() const { return won_ || lost_; }

    const Monster& monster() const { return monster_; }
    int monsterHp() const { return monsterHp_; }
    int monsterMaxHp() const { return monster_.hp; }
    int monsterNumber() const { return monsterIndex_ + 1; }
    int monsterCount() const { return static_cast<int>(config_.monsters.size()); }

    const std::vector<std::optional<Card>>& hand() const { return hand_; }
    int deckCount() const { return static_cast<int>(deck_.size()); }
    int discardCount() const { return static_cast<int>(discard_.size()); }

private:
    void buildDeck();
    std::optional<Card> drawCard();
    void apply(const Card& card);
    void takeDamage(int amount);
    void nextMonster();

    Config config_;
    std::mt19937 rng_;

    std::vector<Card> deck_;
    std::vector<Card> discard_;
    std::vector<std::optional<Card>> hand_;

    Monster monster_;
    int monsterHp_ = 0;
    int monsterIndex_ = 0;

    int hp_ = 0;
    int maxHp_ = 0;
    int block_ = 0;
    int score_ = 0;
    int turn_ = 0;

    bool won_ = false;
    bool lost_ = false;
};

} // namespace cr
