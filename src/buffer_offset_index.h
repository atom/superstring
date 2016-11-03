#ifndef BUFFER_OFFSET_INDEX_H_
#define BUFFER_OFFSET_INDEX_H_

#include <vector>
#include <random>
#include "point.h"

struct LineNode;

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

#endif // BUFFER_OFFSET_INDEX_H_
