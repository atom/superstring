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
  bool splice(Point start, Point deletion_extent, Point insertion_extent,
              std::unique_ptr<Text> old_text, std::unique_ptr<Text> new_text);
  bool splice_old(Point start, Point deletion_extent, Point insertion_extent);
  Patch copy();
  Patch invert();
  std::vector<Hunk> get_hunks() const;
  std::vector<Hunk> get_hunks_in_new_range(Point start, Point end,
                                       bool inclusive = false);
  std::vector<Hunk> get_hunks_in_old_range(Point start, Point end);
  optional<Hunk> hunk_for_old_position(Point position);
  optional<Hunk> hunk_for_new_position(Point position);
  void serialize(std::vector<uint8_t> *) const;
  std::string get_dot_graph() const;
  std::string get_json() const;
  void rebalance();
  size_t get_hunk_count() const;

private:
  template <typename CoordinateSpace>
  std::vector<Hunk> get_hunks_in_range(Point, Point, bool inclusive = false);

  template <typename CoordinateSpace> Node *splay_node_ending_before(Point);

  template <typename CoordinateSpace> Node *splay_node_starting_before(Point);

  template <typename CoordinateSpace> Node *splay_node_ending_after(Point, Point);

  template <typename CoordinateSpace>
  Node *splay_node_starting_after(Point, Point);

  template <typename CoordinateSpace>
  optional<Hunk> hunk_for_position(Point position);

  std::unique_ptr<Text> compute_old_text(std::unique_ptr<Text>, Point, Point);

  void splay_node(Node *);
  void rotate_node_right(Node *, Node *, Node *);
  void rotate_node_left(Node *, Node *, Node *);
  void delete_root();
  void perform_rebalancing_rotations(uint32_t);
  Node *build_node(Node *, Node *, Point, Point, Point, Point,
                  std::unique_ptr<Text>, std::unique_ptr<Text>);
  void delete_node(Node **);
  bool is_frozen() const;

  friend void get_node_from_buffer(const uint8_t **data, const uint8_t *end, Node *node);
  friend void append_node_to_buffer(std::vector<uint8_t> *output, const Node &node);
};

std::ostream &operator<<(std::ostream &, const Patch::Hunk &);
