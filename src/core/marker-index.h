#ifndef MARKER_INDEX_H_
#define MARKER_INDEX_H_

#include <map>
#include <random>
#include <unordered_map>
#include "flat_set.h"
#include "point.h"
#include "range.h"

class MarkerIndex {
public:
  using MarkerId = unsigned;
  using MarkerIdSet = flat_set<MarkerId>;

  struct SpliceResult {
    flat_set<MarkerId> touch;
    flat_set<MarkerId> inside;
    flat_set<MarkerId> overlap;
    flat_set<MarkerId> surround;
  };

  MarkerIndex(unsigned seed);
  int GenerateRandomNumber();
  void Insert(MarkerId id, Point start, Point end);
  void SetExclusive(MarkerId id, bool exclusive);
  void Delete(MarkerId id);
  SpliceResult Splice(Point start, Point old_extent, Point new_extent);
  Point GetStart(MarkerId id) const;
  Point GetEnd(MarkerId id) const;
  Range GetRange(MarkerId id) const;

  int Compare(MarkerId id1, MarkerId id2) const;
  flat_set<MarkerId> FindIntersecting(Point start, Point end);
  flat_set<MarkerId> FindContaining(Point start, Point end);
  flat_set<MarkerId> FindContainedIn(Point start, Point end);
  flat_set<MarkerId> FindStartingIn(Point start, Point end);
  flat_set<MarkerId> FindStartingAt(Point position);
  flat_set<MarkerId> FindEndingIn(Point start, Point end);
  flat_set<MarkerId> FindEndingAt(Point position);

  std::unordered_map<MarkerId, Range> Dump();

private:
  friend class Iterator;

  struct Node {
    Node *parent;
    Node *left;
    Node *right;
    Point left_extent;
    flat_set<MarkerId> left_marker_ids;
    flat_set<MarkerId> right_marker_ids;
    flat_set<MarkerId> start_marker_ids;
    flat_set<MarkerId> end_marker_ids;
    int priority;

    Node(Node *parent, Point left_extent);
    bool IsMarkerEndpoint();
  };

  class Iterator {
  public:
    Iterator(MarkerIndex *marker_index);
    void Reset();
    Node* InsertMarkerStart(const MarkerId &id, const Point &start_position, const Point &end_position);
    Node* InsertMarkerEnd(const MarkerId &id, const Point &start_position, const Point &end_position);
    Node* InsertSpliceBoundary(const Point &position, bool is_insertion_end);
    void FindIntersecting(const Point &start, const Point &end, flat_set<MarkerId> *result);
    void FindContainedIn(const Point &start, const Point &end, flat_set<MarkerId> *result);
    void FindStartingIn(const Point &start, const Point &end, flat_set<MarkerId> *result);
    void FindEndingIn(const Point &start, const Point &end, flat_set<MarkerId> *result);
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
    void CheckIntersection(const Point &start, const Point &end, flat_set<MarkerId> *results);
    void CacheNodePosition() const;

    MarkerIndex *marker_index;
    Node *current_node;
    Point current_node_position;
    Point left_ancestor_position;
    Point right_ancestor_position;
    std::vector<Point> left_ancestor_position_stack;
    std::vector<Point> right_ancestor_position_stack;
  };

  Point GetNodePosition(const Node *node) const;
  void DeleteNode(Node *node);
  void DeleteSubtree(Node *node);
  void BubbleNodeUp(Node *node);
  void BubbleNodeDown(Node *node);
  void RotateNodeLeft(Node *pivot);
  void RotateNodeRight(Node *pivot);
  void GetStartingAndEndingMarkersWithinSubtree(const Node *node, flat_set<MarkerId> *starting, flat_set<MarkerId> *ending);
  void PopulateSpliceInvalidationSets(SpliceResult *invalidated, const Node *start_node, const Node *end_node, const flat_set<MarkerId> &starting_inside_splice, const flat_set<MarkerId> &ending_inside_splice);

  std::default_random_engine random_engine;
  std::uniform_int_distribution<int> random_distribution;
   Node *root;
  std::map<MarkerId, Node*> start_nodes_by_id;
  std::map<MarkerId, Node*> end_nodes_by_id;
  Iterator iterator;
  flat_set<MarkerId> exclusive_marker_ids;
  mutable std::unordered_map<const Node*, Point> node_position_cache;
};

#endif // MARKER_INDEX_H_
