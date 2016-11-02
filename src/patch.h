#include <vector>
#include "point.h"
#include "hunk.h"

struct Node;

class Patch {
 public:
  Patch();
  ~Patch();
  void Splice(Point start, Point deletion_extent, Point insertion_extent);
  std::vector<Hunk> GetHunks() const;
  std::vector<Hunk> GetHunksInNewRange(Point start, Point end);
  std::vector<Hunk> GetHunksInOldRange(Point start, Point end);
  Point TranslateOldPosition(Point position);
  Point TranslateNewPosition(Point position);
  void PrintDotGraph() const;

 private:
  template<typename CoordinateSpace>
  friend std::vector<Hunk> GetHunksInRange(Patch &, Point, Point);

  template<typename CoordinateSpace>
  friend Node *SplayLowerBound(Patch &, Point);

  template<typename CoordinateSpace>
  friend Node *SplayUpperBound(Patch &, Point);

  template<typename InputSpace, typename OutputSpace>
  friend Point TranslatePosition(Patch &, Point);

  void SplayNode(Node *);
  void RotateNodeRight(Node *);
  void RotateNodeLeft(Node *);
  void DeleteRoot();

  struct PositionStackEntry;
  mutable std::vector<PositionStackEntry> left_ancestor_stack;
  Node *root;
};
