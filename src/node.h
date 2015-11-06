#ifndef NODE_H_
#define NODE_H_

#include <memory>
#include <set>
#include "marker-id.h"
#include "point.h"

struct Node {
  Node(Node *parent, Point left_extent);

  Node *parent;
  std::unique_ptr<Node> left;
  std::unique_ptr<Node> right;
  Point left_extent;
  std::set<MarkerId> left_marker_ids;
  std::set<MarkerId> right_marker_ids;
  std::set<MarkerId> start_marker_ids;
  std::set<MarkerId> end_marker_ids;
  int priority;
};

#endif // NODE_H_
