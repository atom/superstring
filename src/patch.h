#include <vector>
#include <memory>
#include <nan.h>
#include "point.h"
#include "hunk.h"
#include "text.h"

struct Node;

class Patch {
 public:
  Patch();
  Patch(bool merges_adjacent_hunks);
  Patch(const std::vector<uint8_t>&);
  ~Patch();
  bool Splice(Point start, Point deletion_extent, Point insertion_extent,
              std::unique_ptr<Text> old_text, std::unique_ptr<Text> new_text);
  bool SpliceOld(Point start, Point deletion_extent, Point insertion_extent);
  std::vector<Hunk> GetHunks() const;
  std::vector<Hunk> GetHunksInNewRange(Point start, Point end);
  std::vector<Hunk> GetHunksInOldRange(Point start, Point end);
  Nan::Maybe<Hunk> HunkForOldPosition(Point position);
  Nan::Maybe<Hunk> HunkForNewPosition(Point position);
  void Serialize(std::vector<uint8_t> *) const;
  void PrintDotGraph() const;
  void Rebalance();
  size_t GetHunkCount() const;

 private:
  template<typename CoordinateSpace>
  std::vector<Hunk> GetHunksInRange(Point, Point);

  template<typename CoordinateSpace>
  Node *SplayNodeEndingBefore(Point);

  template<typename CoordinateSpace>
  Node *SplayNodeStartingBefore(Point);

  template<typename CoordinateSpace>
  Node *SplayNodeEndingAfter(Point, Point);

  template<typename CoordinateSpace>
  Node *SplayNodeStartingAfter(Point, Point);

  template<typename CoordinateSpace>
  Nan::Maybe<Hunk> HunkForPosition(Point position);

  std::unique_ptr<Text> ComputeOldText(std::unique_ptr<Text>, Point, Point);

  void SplayNode(Node *);
  void RotateNodeRight(Node *, Node *, Node *);
  void RotateNodeLeft(Node *, Node *, Node *);
  void DeleteRoot();
  void PerformRebalancingRotations(uint32_t);
  Node *BuildNode(Node *, Node *, Point, Point, Point, Point, std::unique_ptr<Text>, std::unique_ptr<Text>);
  void DeleteNode(Node **);

  struct PositionStackEntry;
  mutable std::vector<PositionStackEntry> left_ancestor_stack;
  mutable std::vector<Node *> node_stack;
  Node *root;
  bool is_frozen;
  bool merges_adjacent_hunks;
  uint32_t hunk_count;
};
