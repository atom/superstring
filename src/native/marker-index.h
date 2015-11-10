#ifndef MARKER_INDEX_H_
#define MARKER_INDEX_H_

#include <map>
#include <random>
#include "iterator.h"
#include "marker-id.h"
#include "node.h"

class MarkerIndex {
  friend class Iterator;
 public:
  MarkerIndex(unsigned seed);
  int GenerateRandomNumber();
  void Insert(MarkerId id, Point start, Point end);
  Point GetStart(MarkerId id) const;
  Point GetEnd(MarkerId id) const;

 private:
  void BubbleNodeUp(Node *node);
  void RotateNodeLeft(Node *pivot);
  void RotateNodeRight(Node *pivot);
  Point GetNodeOffset(const Node *node) const;

  std::default_random_engine random_engine;
  std::uniform_int_distribution<int> random_distribution;
   Node *root;
  std::map<MarkerId, const Node*> start_nodes_by_id;
  std::map<MarkerId, const Node*> end_nodes_by_id;
  Iterator iterator;
  std::set<MarkerId> exclusive_marker_ids;
};

#endif // MARKER_INDEX_H_
