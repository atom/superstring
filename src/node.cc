#include "node.h"
#include "point.h"

Node::Node(Node *parent, Point left_extent) :
  parent {parent},
  left {nullptr},
  right {nullptr},
  left_extent {left_extent} {}
