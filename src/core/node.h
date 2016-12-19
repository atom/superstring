#ifndef NODE_H_
#define NODE_H_

#include <unordered_set>
#include "marker-id.h"
#include "point.h"

struct Node {
  Node(Node *parent, Point left_extent);

  bool IsMarkerEndpoint();

  Node *parent;
  Node *left;
  Node *right;
  Point left_extent;
  std::unordered_set<MarkerId> left_marker_ids;
  std::unordered_set<MarkerId> right_marker_ids;
  std::unordered_set<MarkerId> start_marker_ids;
  std::unordered_set<MarkerId> end_marker_ids;
  int priority;
};

#endif // NODE_H_
