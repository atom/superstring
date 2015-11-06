#include "node.h"
#include "point.h"

Node::Node(Node *parent, Point left_extent) :
  parent {parent}, left_extent {left_extent} {}
