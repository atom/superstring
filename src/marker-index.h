#ifndef MARKER_INDEX_H_
#define MARKER_INDEX_H_

#include <map>
#include <memory>
#include <random>
#include "iterator.h"
#include "marker-id.h"
#include "node.h"

class MarkerIndex {
  friend class Iterator;
 public:
  MarkerIndex(unsigned seed);
  int GenerateRandomNumber();
  void Insert(const MarkerId &id, const Point &start, const Point &end);
 private:
  std::default_random_engine random_engine;
  std::uniform_int_distribution<int> random_distribution;
  std::unique_ptr<Node> root;
  std::map<MarkerId, const Node*> start_nodes_by_id;
  std::map<MarkerId, const Node*> end_nodes_by_id;
  Iterator iterator;
  std::set<MarkerId> exclusive_marker_ids;
};

#endif // MARKER_INDEX_H_
