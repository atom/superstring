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

 private:
  Node *FindLowerBound(Point target) const;
  Node *FindUpperBound(Point target) const;
  void SplayNode(Node *);
  void RotateNodeRight(Node *);
  void RotateNodeLeft(Node *);
  void DeleteRoot();

  struct PositionStackEntry {
    Point old_end;
    Point new_end;
  };

  mutable std::vector<PositionStackEntry> left_ancestor_stack;
  Node *root;
};
