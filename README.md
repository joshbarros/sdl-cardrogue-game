# DECKFALL (working title)

A small, data-driven card roguelike written from scratch in C++20 with SDL3 and a
hand-rolled immediate-mode renderer. No engine, no UI framework.

The point of the project is the architecture as much as the game:

- **`src/game.*` is pure rules** — no SDL, no I/O, no globals. Easy to unit test,
  easy to replay, easy to run on a server later.
- **`src/renderer.*` is a thin immediate-mode layer** over `SDL_Renderer` (rects,
  lines, circles, text). Every frame draws the whole screen.
- **`assets/config.json` is the content** — monsters, deck composition, values.
  This is the seam where add-on packs plug in later.

## Build

Requirements: CMake 3.25+, a C++20 compiler, Git. Dependencies (SDL3, SDL3_ttf,
nlohmann/json) are fetched by CMake.

```sh
cmake -S . -B build
cmake --build build --config Release
```

The executable lands in `build/<config>/` with `assets/` copied next to it.

Windows (Visual Studio generator):

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Run

```sh
build/Release/cardrogue.exe            # play
build/Release/cardrogue.exe --shot a.bmp   # render one frame to a BMP and quit
build/Release/cardrogue.exe --seed 42      # deterministic deck
```

## Rules

Four monsters, one deck. Each turn you play one card from your hand and the slot
refills:

| Card | Effect |
| --- | --- |
| SWORD | damage the current monster |
| SHIELD | add block, absorbed before HP |
| POTION | heal, capped at max HP |
| CURSE | the monster hits you for that value (block applies) |

Kill a monster for `50 + hp * 10` points. Clear all four to win. The deck
reshuffles its discard pile when it runs dry, so the run ends when you do.

Tune everything in `assets/config.json`.

## Business model

The game itself is **100% free** — no paywall, no ads, source under MIT. What is
sold are **in-app purchases / add-ons**: extra decks, monsters, art packs,
soundtracks.

Add-ons are content, not code, so the open-source license and the store coexist:
the engine stays MIT, packs stay data, and anyone can publish free packs too.

## Roadmap

- [ ] pack loader (`assets/packs/*.json`) + pack picker screen
- [ ] acts: more monsters, escalating difficulty, relics
- [ ] Web build (Emscripten) for itch.io
- [ ] sprite-based cards (Art from the Sorceress pipeline)
- [ ] unit tests for `Game` (deterministic seeds)

## License

Code: MIT (see `LICENSE`). Font: Silkscreen, SIL Open Font License 1.1
(`assets/fonts/OFL.txt`).
