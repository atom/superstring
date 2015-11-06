#ifndef ITERATOR_H_
#define ITERATOR_H_

#include <vector>
#include "marker-id.h"
#include "point.h"

class MarkerIndex;
struct Node;

class Iterator {
 public:
  Iterator(MarkerIndex *marker_index);
  void Reset();
  Node* InsertMarkerStart(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  Node* InsertMarkerEnd(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  void DescendLeft();
  void DescendRight();
  void MarkRight(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  void MarkLeft(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  Node* InsertLeftChild(const Point &offset);
  Node* InsertRightChild(const Point &offset);
 private:
  MarkerIndex *marker_index;
  Node *node;
  Point node_offset;
  Point left_ancestor_offset;
  Point right_ancestor_offset;
  std::vector<Point> left_ancestor_offset_stack;
  std::vector<Point> right_ancestor_offset_stack;
};

#endif // ITERATOR_H_
