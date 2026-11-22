#pragma once

#include <cstddef>
#include <memory>

#include "core/game.h"

// Every game the box offers. Adding a game: implement Game, add it here
// (registry.cpp), add its sources to src/CMakeLists.txt, add its screens and
// strings to web/ (see docs/add-a-game.md).
namespace games {

size_t count();
const GameInfo& info(size_t index);
const GameInfo* find(const char* id);
std::unique_ptr<Game> create(const char* id, GameHost& host);

}  // namespace games
