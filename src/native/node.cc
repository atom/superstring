#include "node.h"
#include "point.h"

Node::Node(Node *parent, Point left_extent) :
  parent {parent},
  left {nullptr},
  right {nullptr},
  left_extent {left_extent},
  priority {0} {}

bool Node::IsMarkerEndpoint() {
  return (start_marker_ids.size() + end_marker_ids.size()) > 0;
}
