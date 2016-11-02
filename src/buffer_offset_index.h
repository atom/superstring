#include <vector>
#include <random>

struct LineNode;

struct Point {
  unsigned int row;
  unsigned int column;
};

class BufferOffsetIndex {
 public:
  BufferOffsetIndex();
  ~BufferOffsetIndex();
  void Splice(unsigned int, unsigned int, std::vector<unsigned int>&);
  unsigned int CharacterIndexForPosition(Point) const;
  Point PositionForCharacterIndex(unsigned int) const;

 private:
  LineNode *FindAndBubbleNodeUpToRoot(unsigned int);
  void BubbleNodeDown(LineNode *, LineNode *);
  void RotateNodeLeft(LineNode *, LineNode *, LineNode *);
  void RotateNodeRight(LineNode *, LineNode *, LineNode *);
  LineNode *BuildLineNodeTreeFromLineLengths(std::vector<unsigned int>&, unsigned int, unsigned int, unsigned int);

  LineNode *root;
  std::default_random_engine rng_engine;
};
