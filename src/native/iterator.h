#ifndef ITERATOR_H_
#define ITERATOR_H_

#include <unordered_map>
#include <unordered_set>
#include "marker-id.h"
#include "point.h"

class MarkerIndex;
struct Node;
struct Range;

class Iterator {
 public:
  Iterator(MarkerIndex *marker_index);
  void Reset();
  Node* InsertMarkerStart(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  Node* InsertMarkerEnd(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  Node* InsertSpliceBoundary(const Point &offset, bool is_insertion_end);
  void FindIntersecting(const Point &start, const Point &end, std::unordered_set<MarkerId> *result);
  void FindContainedIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result);
  void FindStartingIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result);
  void FindEndingIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result);
  std::unordered_map<MarkerId, Range> Dump();

 private:
  void Ascend();
  void DescendLeft();
  void DescendRight();
  void MoveToSuccessor();
  void SeekToFirstNodeGreaterThanOrEqualTo(const Point &offset);
  void MarkRight(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  void MarkLeft(const MarkerId &id, const Point &start_offset, const Point &end_offset);
  Node* InsertLeftChild(const Point &offset);
  Node* InsertRightChild(const Point &offset);
  void CheckIntersection(const Point &start, const Point &end, std::unordered_set<MarkerId> *results);

  MarkerIndex *marker_index;
  Node *node;
  Point node_offset;
  Point left_ancestor_offset;
  Point right_ancestor_offset;
  std::vector<Point> left_ancestor_offset_stack;
  std::vector<Point> right_ancestor_offset_stack;
};

#endif // ITERATOR_H_
