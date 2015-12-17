#ifndef ITERATOR_H_
#define ITERATOR_H_

#include <vector>
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
  Node* InsertMarkerStart(const MarkerId &id, const Point &start_position, const Point &end_position);
  Node* InsertMarkerEnd(const MarkerId &id, const Point &start_position, const Point &end_position);
  Node* InsertSpliceBoundary(const Point &position, bool is_insertion_end);
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
  void SeekToFirstNodeGreaterThanOrEqualTo(const Point &position);
  void MarkRight(const MarkerId &id, const Point &start_position, const Point &end_position);
  void MarkLeft(const MarkerId &id, const Point &start_position, const Point &end_position);
  Node* InsertLeftChild(const Point &position);
  Node* InsertRightChild(const Point &position);
  void CheckIntersection(const Point &start, const Point &end, std::unordered_set<MarkerId> *results);
  void CacheNodePosition() const;

  MarkerIndex *marker_index;
  Node *current_node;
  Point current_node_position;
  Point left_ancestor_position;
  Point right_ancestor_position;
  std::vector<Point> left_ancestor_position_stack;
  std::vector<Point> right_ancestor_position_stack;
};

#endif // ITERATOR_H_
