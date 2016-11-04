#include <vector>
#include <nan.h>
#include "point.h"
#include "hunk.h"

struct Node;

class Patch {
 public:
  Patch();
  Patch(const std::vector<uint8_t>&);
  ~Patch();
  bool Splice(Point start, Point deletion_extent, Point insertion_extent);
  std::vector<Hunk> GetHunks() const;
  std::vector<Hunk> GetHunksInNewRange(Point start, Point end);
  std::vector<Hunk> GetHunksInOldRange(Point start, Point end);
  Nan::Maybe<Hunk> HunkForOldPosition(Point position);
  Nan::Maybe<Hunk> HunkForNewPosition(Point position);
  void Serialize(std::vector<uint8_t> *) const;
  void PrintDotGraph() const;

 private:
  template<typename CoordinateSpace>
  std::vector<Hunk> GetHunksInRange(Point, Point);

  template<typename CoordinateSpace>
  Node *SplayLowerBound(Point);

  template<typename CoordinateSpace>
  Node *SplayUpperBound(Point);

  template<typename CoordinateSpace>
  Nan::Maybe<Hunk> HunkForPosition(Point position);

  void SplayNode(Node *);
  void RotateNodeRight(Node *);
  void RotateNodeLeft(Node *);
  void DeleteRoot();

  struct PositionStackEntry;
  mutable std::vector<PositionStackEntry> left_ancestor_stack;
  Node *root;
  bool is_frozen;
};
