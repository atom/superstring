#include "optional.h"
#include "point.h"
#include "text.h"
#include <memory>
#include <vector>
#include <ostream>

class Patch {
  struct Node;
  struct OldCoordinates;
  struct NewCoordinates;
  struct PositionStackEntry;

  mutable std::vector<PositionStackEntry> left_ancestor_stack;
  mutable std::vector<Node *> node_stack;
  Node *root;
  Node *frozen_node_array;
  bool merges_adjacent_hunks;
  uint32_t hunk_count;

public:
  struct Hunk {
    Point old_start;
    Point old_end;
    Point new_start;
    Point new_end;
    Text *old_text;
    Text *new_text;
  };

  Patch();
  Patch(bool merges_adjacent_hunks);
  Patch(const std::vector<uint8_t> &);
  Patch(const std::vector<const Patch *> &);
  Patch(Node *root, uint32_t hunk_count, bool merges_adjacent_hunks);
  Patch(Patch &&);
  ~Patch();
  bool Splice(Point start, Point deletion_extent, Point insertion_extent,
              std::unique_ptr<Text> old_text, std::unique_ptr<Text> new_text);
  bool SpliceOld(Point start, Point deletion_extent, Point insertion_extent);
  Patch Copy();
  Patch Invert();
  std::vector<Hunk> GetHunks() const;
  std::vector<Hunk> GetHunksInNewRange(Point start, Point end,
                                       bool inclusive = false);
  std::vector<Hunk> GetHunksInOldRange(Point start, Point end);
  optional<Hunk> HunkForOldPosition(Point position);
  optional<Hunk> HunkForNewPosition(Point position);
  void Serialize(std::vector<uint8_t> *) const;
  std::string GetDotGraph() const;
  std::string GetJSON() const;
  void Rebalance();
  size_t GetHunkCount() const;

private:
  template <typename CoordinateSpace>
  std::vector<Hunk> GetHunksInRange(Point, Point, bool inclusive = false);

  template <typename CoordinateSpace> Node *SplayNodeEndingBefore(Point);

  template <typename CoordinateSpace> Node *SplayNodeStartingBefore(Point);

  template <typename CoordinateSpace> Node *SplayNodeEndingAfter(Point, Point);

  template <typename CoordinateSpace>
  Node *SplayNodeStartingAfter(Point, Point);

  template <typename CoordinateSpace>
  optional<Hunk> HunkForPosition(Point position);

  std::unique_ptr<Text> ComputeOldText(std::unique_ptr<Text>, Point, Point);

  void SplayNode(Node *);
  void RotateNodeRight(Node *, Node *, Node *);
  void RotateNodeLeft(Node *, Node *, Node *);
  void DeleteRoot();
  void PerformRebalancingRotations(uint32_t);
  Node *BuildNode(Node *, Node *, Point, Point, Point, Point,
                  std::unique_ptr<Text>, std::unique_ptr<Text>);
  void DeleteNode(Node **);
  bool IsFrozen() const;

  friend void GetNodeFromBuffer(const uint8_t **data, const uint8_t *end, Node *node);
  friend void AppendNodeToBuffer(std::vector<uint8_t> *output, const Node &node);
};

std::ostream &operator<<(std::ostream &, const Patch::Hunk &);
