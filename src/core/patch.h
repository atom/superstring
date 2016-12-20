#include "hunk.h"
#include "optional.h"
#include "point.h"
#include "text.h"
#include <memory>
#include <vector>

struct PatchNode;

class Patch {
public:
  Patch();
  Patch(bool merges_adjacent_hunks);
  Patch(const std::vector<uint8_t> &);
  Patch(const std::vector<const Patch *> &);
  Patch(PatchNode *root, uint32_t hunk_count, bool merges_adjacent_hunks);
  Patch(Patch &&);
  ~Patch();
  bool Splice(Point start, Point deletion_extent, Point insertion_extent,
              std::unique_ptr<Text> old_text, std::unique_ptr<Text> new_text);
  bool SpliceOld(Point start, Point deletion_extent, Point insertion_extent);
  Patch Invert();
  std::vector<Hunk> GetHunks() const;
  std::vector<Hunk> GetHunksInNewRange(Point start, Point end,
                                       bool inclusive = false);
  std::vector<Hunk> GetHunksInOldRange(Point start, Point end);
  optional<Hunk> HunkForOldPosition(Point position);
  optional<Hunk> HunkForNewPosition(Point position);
  void Serialize(std::vector<uint8_t> *) const;
  void PrintDotGraph() const;
  void Rebalance();
  size_t GetHunkCount() const;

private:
  template <typename CoordinateSpace>
  std::vector<Hunk> GetHunksInRange(Point, Point, bool inclusive = false);

  template <typename CoordinateSpace> PatchNode *SplayNodeEndingBefore(Point);

  template <typename CoordinateSpace> PatchNode *SplayNodeStartingBefore(Point);

  template <typename CoordinateSpace> PatchNode *SplayNodeEndingAfter(Point, Point);

  template <typename CoordinateSpace>
  PatchNode *SplayNodeStartingAfter(Point, Point);

  template <typename CoordinateSpace>
  optional<Hunk> HunkForPosition(Point position);

  std::unique_ptr<Text> ComputeOldText(std::unique_ptr<Text>, Point, Point);

  void SplayNode(PatchNode *);
  void RotateNodeRight(PatchNode *, PatchNode *, PatchNode *);
  void RotateNodeLeft(PatchNode *, PatchNode *, PatchNode *);
  void DeleteRoot();
  void PerformRebalancingRotations(uint32_t);
  PatchNode *BuildNode(PatchNode *, PatchNode *, Point, Point, Point, Point,
                  std::unique_ptr<Text>, std::unique_ptr<Text>);
  void DeleteNode(PatchNode **);
  bool IsFrozen() const;

  struct PositionStackEntry;
  mutable std::vector<PositionStackEntry> left_ancestor_stack;
  mutable std::vector<PatchNode *> node_stack;
  PatchNode *root;
  PatchNode *frozen_node_array;
  bool merges_adjacent_hunks;
  uint32_t hunk_count;
};
