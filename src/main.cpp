#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "game.hpp"
#include "renderer.hpp"

namespace {

using cr::Align;
using cr::Card;
using cr::Color;
using cr::Game;
using namespace cr::palette;

constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 800;

struct Options {
    std::string shotPath;   // --shot <file.bmp>: render one frame, save, exit
    std::uint32_t seed = 1234;
};

struct Layout {
    float handCardW = 152.0f;
    float handCardH = 212.0f;
    float handGap = 22.0f;
    SDL_FRect hand[8]{};
    int handCount = 0;
    SDL_FRect monster{};
};

Color suitColor(cr::Suit suit) {
    switch (suit) {
    case cr::Suit::Sword: return sword;
    case cr::Suit::Shield: return shield;
    case cr::Suit::Potion: return potion;
    case cr::Suit::Curse: return curse;
    }
    return dim;
}

Layout computeLayout(int w, int h, int handCount) {
    Layout l;
    l.handCount = handCount;

    const float totalW = handCount * l.handCardW + (handCount - 1) * l.handGap;
    const float startX = (float(w) - totalW) * 0.5f;
    const float handY = float(h) - l.handCardH - 46.0f;

    for (int i = 0; i < handCount && i < 8; ++i) {
        l.hand[i] = SDL_FRect{startX + i * (l.handCardW + l.handGap), handY, l.handCardW,
                              l.handCardH};
    }

    const float monsterW = 280.0f;
    const float monsterH = 360.0f;
    const float monsterY = 150.0f;
    l.monster = SDL_FRect{(float(w) - monsterW) * 0.5f, monsterY, monsterW, monsterH};
    return l;
}

bool pointIn(const SDL_FRect& r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

void drawSuitIcon(cr::Renderer& r, cr::Suit suit, float cx, float cy, float scale, Color c);

void drawCardFace(cr::Renderer& r, const Card& card, const SDL_FRect& rect, bool hovered) {
    const Color accentColor = suitColor(card.suit);

    // shadow
    r.fillRect(rect.x + 5, rect.y + 7, rect.w, rect.h, Color{0, 0, 0, 120});
    r.fillRect(rect.x, rect.y, rect.w, rect.h, parchment);

    // suit strip
    r.fillRect(rect.x, rect.y, rect.w, 34.0f, accentColor);
    r.text(cr::suitLabel(card.suit), rect.x + 10.0f, rect.y + 9.0f, 14, ink);

    // value
    const std::string value = std::to_string(card.value);
    r.text(value, rect.x + rect.w * 0.5f, rect.y + 66.0f, 52, ink, Align::Center);

    // icon
    drawSuitIcon(r, card.suit, rect.x + rect.w * 0.5f, rect.y + rect.h - 62.0f, 1.0f,
                 accentColor);

    // border
    r.frameRect(rect.x, rect.y, rect.w, rect.h, hovered ? accentColor : ink,
                hovered ? 4.0f : 3.0f);
}

void drawSuitIcon(cr::Renderer& r, cr::Suit suit, float cx, float cy, float scale, Color c) {
    const float s = scale;
    switch (suit) {
    case cr::Suit::Sword:
        r.line(cx, cy - 22 * s, cx, cy + 20 * s, c, 5 * s);
        r.line(cx - 13 * s, cy + 8 * s, cx + 13 * s, cy + 8 * s, c, 5 * s);
        r.filledCircle(cx, cy + 22 * s, 4 * s, c);
        break;
    case cr::Suit::Shield:
        r.filledCircle(cx, cy - 2 * s, 20 * s, c);
        r.fillRect(cx - 20 * s, cy + 4 * s, 40 * s, 14 * s, c);
        r.fillRect(cx - 10 * s, cy + 16 * s, 20 * s, 6 * s, c);
        break;
    case cr::Suit::Potion:
        r.fillRect(cx - 6 * s, cy - 20 * s, 12 * s, 12 * s, c);
        r.filledCircle(cx, cy + 4 * s, 17 * s, c);
        break;
    case cr::Suit::Curse:
        r.line(cx - 14 * s, cy - 14 * s, cx + 14 * s, cy + 14 * s, c, 6 * s);
        r.line(cx + 14 * s, cy - 14 * s, cx - 14 * s, cy + 14 * s, c, 6 * s);
        break;
    }
}

void drawHeader(cr::Renderer& r, const Game& g, int best, int width) {
    r.fillRect(0, 0, float(width), 96.0f, panelDark);
    r.fillRect(0, 96.0f, float(width), 2.0f, Color{0x3a, 0x2b, 0x52, 255});

    // HP
    const Color hpColor = g.hp() <= 5 ? Color{0xff, 0x5b, 0x5b, 255} : potion;
    r.text("HP", 32.0f, 22.0f, 14, dim);
    r.text(std::to_string(g.hp()) + "/" + std::to_string(g.maxHp()), 32.0f, 42.0f, 30,
           hpColor);

    // HP bar
    const float barX = 210.0f;
    const float barY = 46.0f;
    const float barW = 220.0f;
    r.fillRect(barX, barY, barW, 18.0f, Color{0x33, 0x27, 0x47, 255});
    const float fill = barW * (float(g.hp()) / float(std::max(1, g.maxHp())));
    r.fillRect(barX, barY, fill, 18.0f, hpColor);
    r.frameRect(barX, barY, barW, 18.0f, Color{0x4a, 0x39, 0x63, 255}, 2.0f);

    // Block
    r.text("BLOCK", barX + barW + 40.0f, 22.0f, 14, dim);
    r.text(std::to_string(g.block()), barX + barW + 40.0f, 42.0f, 30, shield);

    // Score / best
    r.text("SCORE", float(width) - 300.0f, 22.0f, 14, dim);
    r.text(std::to_string(g.score()), float(width) - 300.0f, 42.0f, 30, accent);
    r.text("BEST " + std::to_string(best), float(width) - 130.0f, 52.0f, 14, dim);
}

void drawMonster(cr::Renderer& r, const Game& g, const SDL_FRect& rect) {
    r.fillRect(rect.x + 6, rect.y + 8, rect.w, rect.h, Color{0, 0, 0, 120});
    r.fillRect(rect.x, rect.y, rect.w, rect.h, monsterBody);
    r.frameRect(rect.x, rect.y, rect.w, rect.h, monsterEdge, 4.0f);

    r.text("MONSTER " + std::to_string(g.monsterNumber()) + "/" +
               std::to_string(g.monsterCount()),
           rect.x + rect.w * 0.5f, rect.y + 16.0f, 14, Color{0xc9, 0x8f, 0x9b, 255},
           Align::Center);

    r.text(g.monster().name, rect.x + rect.w * 0.5f, rect.y + 52.0f, 32, parchment,
           Align::Center);

    // big monster glyph
    const float cx = rect.x + rect.w * 0.5f;
    const float cy = rect.y + 176.0f;
    r.filledCircle(cx, cy, 46.0f, Color{0x5c, 0x22, 0x30, 255});
    r.filledCircle(cx - 16.0f, cy - 10.0f, 6.0f, monsterEdge);
    r.filledCircle(cx + 16.0f, cy - 10.0f, 6.0f, monsterEdge);
    r.line(cx - 18.0f, cy + 16.0f, cx + 18.0f, cy + 16.0f, monsterEdge, 5.0f);

    // hp bar
    const float barW = rect.w - 56.0f;
    const float barX = rect.x + 28.0f;
    const float barY = rect.y + rect.h - 92.0f;
    r.fillRect(barX, barY, barW, 26.0f, Color{0x2a, 0x10, 0x17, 255});
    const float ratio =
        float(std::max(0, g.monsterHp())) / float(std::max(1, g.monsterMaxHp()));
    r.fillRect(barX, barY, barW * ratio, 26.0f, Color{0xb8, 0x3a, 0x4a, 255});
    r.frameRect(barX, barY, barW, 26.0f, monsterEdge, 2.0f);

    const std::string hpText =
        std::to_string(std::max(0, g.monsterHp())) + " / " + std::to_string(g.monsterMaxHp());
    r.text(hpText, rect.x + rect.w * 0.5f, barY + 24.0f, 14, Color{0xc9, 0x8f, 0x9b, 255},
           Align::Center);
}

void drawHand(cr::Renderer& r, const Game& g, const Layout& l, int hoverIndex) {
    for (int i = 0; i < l.handCount; ++i) {
        SDL_FRect rect = l.hand[i];
        const bool hovered = (i == hoverIndex);
        if (hovered) rect.y -= 12.0f;

        const auto& slot = g.hand()[static_cast<std::size_t>(i)];
        if (!slot.has_value()) {
            r.fillRect(rect.x, rect.y, rect.w, rect.h, Color{0x1f, 0x17, 0x2c, 255});
            r.frameRect(rect.x, rect.y, rect.w, rect.h, Color{0x33, 0x27, 0x47, 255}, 2.0f);
            continue;
        }
        drawCardFace(r, *slot, rect, hovered);
    }
}

void drawFooter(cr::Renderer& r, const Game& g, int width, int height) {
    const std::string hint =
        "SWORD hits  -  SHIELD blocks  -  POTION heals  -  CURSE hurts you";
    r.text(hint, float(width) * 0.5f, float(height) - 26.0f, 12, dim, Align::Center);
    r.text("deck " + std::to_string(g.deckCount()), 24.0f, float(height) - 30.0f, 12, dim);
    r.text("R restart   ESC quit", float(width) - 24.0f, float(height) - 30.0f, 12, dim,
           Align::Right);
}

void drawOverlay(cr::Renderer& r, const Game& g, int width, int height) {
    r.fillRect(0, 0, float(width), float(height), Color{0x08, 0x06, 0x0d, 210});

    const bool win = g.won();
    const std::string title = win ? "VICTORY" : "YOU DIED";
    r.text(title, float(width) * 0.5f, float(height) * 0.5f - 90.0f, 56,
           win ? accent : Color{0xff, 0x5b, 0x5b, 255}, Align::Center);
    r.text("score " + std::to_string(g.score()), float(width) * 0.5f,
           float(height) * 0.5f - 10.0f, 26, parchment, Align::Center);
    r.text(win ? "the dungeon is clear" : "the dungeon keeps your bones",
           float(width) * 0.5f, float(height) * 0.5f + 26.0f, 14, dim, Align::Center);
    r.text("press R to run again", float(width) * 0.5f, float(height) * 0.5f + 76.0f, 16,
           parchment, Align::Center);
}

std::string findAssetDir() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        candidates.push_back(std::string(base) + "assets/");
        candidates.push_back(std::string(base) + "../assets/");
        SDL_free(const_cast<char*>(base));
    }
    candidates.emplace_back(std::string(CARDROGUE_SOURCE_ASSETS) + "/");

    for (const auto& dir : candidates) {
        if (std::filesystem::exists(dir + "config.json")) return dir;
    }
    throw std::runtime_error("assets/config.json not found next to the executable");
}

std::filesystem::path savePath() {
    std::filesystem::path dir;
    if (const char* pref = SDL_GetPrefPath("cardrogue", "deckfall")) {
        dir = pref;
        SDL_free(const_cast<char*>(pref));
    } else {
        dir = std::filesystem::temp_directory_path();
    }
    std::filesystem::create_directories(dir);
    return dir / "best.txt";
}

int loadBest() {
    std::ifstream in(savePath());
    int best = 0;
    if (in) in >> best;
    return best;
}

void storeBest(int best) {
    std::ofstream out(savePath(), std::ios::trunc);
    out << best << "\n";
}

Options parseArgs(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--shot" && i + 1 < argc) {
            opts.shotPath = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            opts.seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        }
    }
    return opts;
}

} // namespace

int main(int argc, char** argv) {
    const Options opts = parseArgs(argc, argv);

    int exitCode = 0;
    try {
        const std::string assetDir = findAssetDir();
        const cr::Config config = cr::loadConfig(assetDir + "config.json");

        cr::Renderer renderer;
        if (!renderer.init(assetDir + "fonts/Silkscreen-Regular.ttf", kDefaultWidth,
                           kDefaultHeight)) {
            std::fprintf(stderr, "init failed: %s\n", SDL_GetError());
            return 1;
        }

        Game game(config, opts.seed);
        int best = loadBest();
        int savedScore = -1;
        bool shotSaved = false;

        bool running = true;
        while (running) {
            int width = kDefaultWidth;
            int height = kDefaultHeight;
            SDL_GetWindowSize(renderer.window(), &width, &height);

            float mouseX = 0.0f;
            float mouseY = 0.0f;
            SDL_GetMouseState(&mouseX, &mouseY);

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) running = false;
                    if (event.key.key == SDLK_R) {
                        game.restart();
                        savedScore = -1;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                    if (event.button.button != SDL_BUTTON_LEFT) break;
                    const Layout l =
                        computeLayout(width, height, static_cast<int>(game.hand().size()));
                    for (int i = 0; i < l.handCount; ++i) {
                        SDL_FRect rect = l.hand[i];
                        if (pointIn(rect, event.button.x, event.button.y)) {
                            game.playCard(i);
                            break;
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }

            if (game.over() && savedScore != game.score()) {
                savedScore = game.score();
                if (game.score() > best) {
                    best = game.score();
                    storeBest(best);
                }
            }

            const Layout layout =
                computeLayout(width, height, static_cast<int>(game.hand().size()));

            int hover = -1;
            for (int i = 0; i < layout.handCount; ++i) {
                if (pointIn(layout.hand[i], mouseX, mouseY)) hover = i;
            }

            renderer.beginFrame(bg);
            renderer.fillRect(0, 96.0f, float(width), float(height) - 96.0f, bg);
            drawHeader(renderer, game, best, width);
            drawMonster(renderer, game, layout.monster);
            drawHand(renderer, game, layout, hover);
            drawFooter(renderer, game, width, height);
            if (game.over()) drawOverlay(renderer, game, width, height);
            renderer.endFrame();

            if (!opts.shotPath.empty() && !shotSaved) {
                renderer.screenshot(opts.shotPath);
                shotSaved = true;
                running = false;
            }
        }

        renderer.shutdown();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        exitCode = 1;
    }
    return exitCode;
}
