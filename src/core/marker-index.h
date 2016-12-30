#ifndef MARKER_INDEX_H_
#define MARKER_INDEX_H_

#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include "point.h"
#include "range.h"
#include "flat_set.h"

class MarkerIndex {
public:
  using MarkerId = unsigned;
  using MarkerIdSet = flat_set<MarkerId>;
  struct SpliceResult {
    MarkerIdSet touch;
    MarkerIdSet inside;
    MarkerIdSet overlap;
    MarkerIdSet surround;
  };

  MarkerIndex();
  void Insert(MarkerId id, Point start, Point end);
  void SetExclusive(MarkerId id, bool exclusive);
  void Delete(MarkerId id);
  SpliceResult Splice(Point start, Point old_extent, Point new_extent);
  Point GetStart(MarkerId id);
  Point GetEnd(MarkerId id);
  Range GetRange(MarkerId id);

  int Compare(MarkerId id1, MarkerId id2);
  MarkerIdSet FindIntersecting(Point start, Point end);
  MarkerIdSet FindContaining(Point start, Point end);
  MarkerIdSet FindContainedIn(Point start, Point end);
  MarkerIdSet FindStartingIn(Point start, Point end);
  MarkerIdSet FindStartingAt(Point position);
  MarkerIdSet FindEndingIn(Point start, Point end);
  MarkerIdSet FindEndingAt(Point position);

  std::unordered_map<MarkerId, Range> Dump();
  std::string GetDotGraph();

private:
  struct Node {
    Node *parent;
    Node *left;
    Node *right;
    Point left_extent;
    MarkerIdSet left_marker_ids;
    MarkerIdSet right_marker_ids;
    MarkerIdSet start_marker_ids;
    MarkerIdSet end_marker_ids;

    Node(Node *parent, Point left_extent);
    bool IsMarkerEndpoint();
    void WriteDotGraph(std::stringstream &result, Point left_ancestor_position);
  };

  Node *InsertNode(Point position, bool return_existing = true);
  Node* SplayGreatestLowerBound(Point target_position, bool inclusive = false);
  Node* SplayLeastUpperBound(Point target_position, bool inclusive = false);

  void SplayNode(Node *node);
  void RotateNodeLeft(Node *pivot);
  void RotateNodeRight(Node *pivot);
  Point GetNodePosition(Node *node);
  Node *BuildNode(Node *parent, Point left_extent);
  void DeleteSubtree(Node **node);
  void DeleteSingleNode(Node *node);
  void GetStartingAndEndingMarkersWithinSubtree(Node *node, MarkerIdSet *starting, MarkerIdSet *ending);
  void PopulateSpliceInvalidationSets(SpliceResult *invalidated, const Node *start_node, const Node *end_node, const MarkerIdSet &starting_inside_splice, const MarkerIdSet &ending_inside_splice);

  std::vector<Node *> node_stack;
  Node *root;
  uint32_t node_count;
  std::map<MarkerId, Node*> start_nodes_by_id;
  std::map<MarkerId, Node*> end_nodes_by_id;
  std::unordered_set<MarkerId> exclusive_marker_ids;
  mutable std::unordered_map<const Node*, Point> node_position_cache;
};

#endif // MARKER_INDEX_H_
