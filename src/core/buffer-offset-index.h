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
  void splice(unsigned, unsigned, std::vector<unsigned>&);
  unsigned character_index_for_position(Point) const;
  Point position_for_character_index(unsigned) const;

 private:
  LineNode *find_and_bubble_node_up_to_root(unsigned);
  void bubble_node_down(LineNode *, LineNode *);
  void rotate_node_left(LineNode *, LineNode *, LineNode *);
  void rotate_node_right(LineNode *, LineNode *, LineNode *);
  LineNode *build_node_tree_from_line_lengths(std::vector<unsigned>&, unsigned, unsigned, unsigned);

  LineNode *root;
  std::default_random_engine rng_engine;
};

#endif // BUFFER_OFFSET_INDEX_H_
