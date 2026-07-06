#pragma once
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "game_structures.h"

class EntityManager {
 public:
  EntityManager(class RcpService *rcp);
  ~EntityManager();
  void Add(struct Rcp::GameStructures::Entity *);
  void Remove(struct Rcp::GameStructures::Entity *);
  Rcp::GameStructures::Entity *Get(std::string name) const;  // Returns nullptr if not found.
  Rcp::GameStructures::Entity *Get(WORD id) const;           // Note: Equivalent to Game::get_entity_by_id()
  std::vector<std::string> GetPlayerPartialMatches(const std::string &start_of_name) const;
  void Dump() const;

 private:
  std::unordered_map<std::string, struct Rcp::GameStructures::Entity *> entity_map;
};
