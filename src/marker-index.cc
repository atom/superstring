#include "marker-index.h"
#include <climits>
#include <random>
#include <stdlib.h>

using std::default_random_engine;

MarkerIndex::MarkerIndex(unsigned seed)
  : random_engine{static_cast<default_random_engine::result_type>(seed)},
    random_distribution{1, INT_MAX},
    iterator {this} {}

int MarkerIndex::GenerateRandomNumber() {
  return random_distribution(random_engine);
}

void MarkerIndex::Insert(const MarkerId &id, const Point &start, const Point &end) {
  Node *start_node = iterator.InsertMarkerStart(id, start, end);
  Node *end_node = iterator.InsertMarkerEnd(id, start, end);

  start_node->start_marker_ids.insert(id);
  end_node->end_marker_ids.insert(id);

  if (start_node->priority == 0) {
    start_node->priority = GenerateRandomNumber();
    // BubbleNodeUp(start_node);
  }

  if (end_node->priority == 0) {
    end_node->priority = GenerateRandomNumber();
    // BubbleNodeUp(end_node);
  }

  start_nodes_by_id.insert({id, start_node});
  end_nodes_by_id.insert({id, end_node});
}
