#include "registry.h"

#include <cstring>

#include "vampires.h"

namespace games {

namespace {
const GameInfo* const kGames[] = {&Vampires::kInfo};
constexpr size_t kCount = sizeof kGames / sizeof kGames[0];
}  // namespace

size_t count() { return kCount; }

const GameInfo& info(size_t index) { return *kGames[index < kCount ? index : 0]; }

const GameInfo* find(const char* id) {
  if (id == nullptr) return nullptr;
  for (const GameInfo* g : kGames)
    if (strcmp(g->id, id) == 0) return g;
  return nullptr;
}

std::unique_ptr<Game> create(const char* id, GameHost& host) {
  const GameInfo* g = find(id);
  if (g == &Vampires::kInfo) return std::unique_ptr<Game>(new Vampires(host));
  return nullptr;
}

}  // namespace games
