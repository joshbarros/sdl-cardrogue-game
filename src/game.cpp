#include "game.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace cr {
namespace {

Suit parseSuit(const std::string& name) {
    if (name == "sword") return Suit::Sword;
    if (name == "shield") return Suit::Shield;
    if (name == "potion") return Suit::Potion;
    if (name == "curse") return Suit::Curse;
    throw std::runtime_error("config: unknown suit '" + name + "'");
}

} // namespace

const char* suitLabel(Suit suit) {
    switch (suit) {
    case Suit::Sword: return "SWORD";
    case Suit::Shield: return "SHIELD";
    case Suit::Potion: return "POTION";
    case Suit::Curse: return "CURSE";
    }
    return "?";
}

Config loadConfig(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("config: cannot open " + path);

    nlohmann::json j;
    in >> j;

    Config cfg;
    cfg.playerHp = j.value("playerHp", 20);
    cfg.handSize = j.value("handSize", 4);

    for (const auto& m : j.at("monsters")) {
        cfg.monsters.push_back({m.at("name").get<std::string>(), m.at("hp").get<int>()});
    }
    for (const auto& d : j.at("deck")) {
        DeckEntry e;
        e.suit = parseSuit(d.at("suit").get<std::string>());
        e.count = d.at("count").get<int>();
        e.minValue = d.at("min").get<int>();
        e.maxValue = d.at("max").get<int>();
        cfg.deck.push_back(e);
    }

    if (cfg.monsters.empty()) throw std::runtime_error("config: no monsters");
    if (cfg.handSize < 2) throw std::runtime_error("config: handSize must be >= 2");
    return cfg;
}

Game::Game(Config config, std::uint32_t seed)
    : config_(std::move(config)), rng_(seed) {
    restart();
}

void Game::restart() {
    hp_ = config_.playerHp;
    maxHp_ = config_.playerHp;
    block_ = 0;
    score_ = 0;
    turn_ = 0;
    monsterIndex_ = 0;
    won_ = false;
    lost_ = false;

    hand_.assign(static_cast<std::size_t>(config_.handSize), std::nullopt);
    buildDeck();

    monster_ = config_.monsters.front();
    monsterHp_ = monster_.hp;

    for (auto& slot : hand_) slot = drawCard();
}

void Game::buildDeck() {
    deck_.clear();
    discard_.clear();

    std::uint32_t nextId = 1;

    for (const auto& entry : config_.deck) {
        std::uniform_int_distribution<int> value(entry.minValue, entry.maxValue);
        for (int i = 0; i < entry.count; ++i) {
            Card card;
            card.suit = entry.suit;
            card.value = value(rng_);
            card.id = nextId++;
            deck_.push_back(card);
        }
    }
    if (deck_.empty()) throw std::runtime_error("config: empty deck");

    std::shuffle(deck_.begin(), deck_.end(), rng_);
}

std::optional<Card> Game::drawCard() {
    if (deck_.empty()) {
        if (discard_.empty()) return std::nullopt;
        deck_ = discard_;
        discard_.clear();
        std::shuffle(deck_.begin(), deck_.end(), rng_);
    }
    Card card = deck_.back();
    deck_.pop_back();
    return card;
}

bool Game::playCard(int handIndex) {
    if (over()) return false;
    if (handIndex < 0 || handIndex >= static_cast<int>(hand_.size())) return false;
    auto& slot = hand_[static_cast<std::size_t>(handIndex)];
    if (!slot.has_value()) return false;

    const Card card = *slot;
    slot.reset();

    apply(card);
    discard_.push_back(card);
    ++turn_;

    if (!over()) slot = drawCard();
    return true;
}

void Game::apply(const Card& card) {
    switch (card.suit) {
    case Suit::Sword:
        monsterHp_ -= card.value;
        if (monsterHp_ <= 0) {
            // Killing blow: the reward scales with how big the monster was.
            score_ += 50 + monster_.hp * 10;
            nextMonster();
        }
        break;
    case Suit::Shield:
        block_ += card.value;
        break;
    case Suit::Potion:
        hp_ = std::min(maxHp_, hp_ + card.value);
        break;
    case Suit::Curse:
        takeDamage(card.value);
        break;
    }
}

void Game::takeDamage(int amount) {
    int remaining = amount;
    if (block_ > 0) {
        const int absorbed = std::min(block_, remaining);
        block_ -= absorbed;
        remaining -= absorbed;
    }
    hp_ -= remaining;
    if (hp_ <= 0) {
        hp_ = 0;
        lost_ = true;
    }
}

void Game::nextMonster() {
    ++monsterIndex_;
    block_ = 0;
    if (monsterIndex_ >= static_cast<int>(config_.monsters.size())) {
        won_ = true;
        monsterIndex_ = static_cast<int>(config_.monsters.size()) - 1;
        monsterHp_ = 0;
        return;
    }
    monster_ = config_.monsters[static_cast<std::size_t>(monsterIndex_)];
    monsterHp_ = monster_.hp;
}

} // namespace cr
