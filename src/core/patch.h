#ifndef PATCH_H_
#define PATCH_H_

#include "optional.h"
#include "point.h"
#include "serializer.h"
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
  bool merges_adjacent_changes;
  uint32_t change_count;

public:
  struct Change {
    Point old_start;
    Point old_end;
    Point new_start;
    Point new_end;
    Text *old_text;
    Text *new_text;
    uint32_t preceding_old_text_size;
    uint32_t preceding_new_text_size;
    uint32_t old_text_size;

    Change() = default;

    Change(Point old_start,
           Point old_end,
           Point new_start,
           Point new_end,
           Text *old_text,
           Text *new_text,
           uint32_t preceding_old_text_size,
           uint32_t preceding_new_text_size,
           uint32_t old_text_size = 0) :
      old_start {old_start},
      old_end {old_end},
      new_start {new_start},
      new_end {new_end},
      old_text {old_text},
      new_text {new_text},
      preceding_old_text_size {preceding_old_text_size},
      preceding_new_text_size {preceding_new_text_size},
      old_text_size {old_text_size} {}
  };

  Patch();
  Patch(bool merges_adjacent_changes);
  Patch(Deserializer &input);
  Patch(const std::vector<const Patch *> &);
  Patch(Node *root, uint32_t change_count, bool merges_adjacent_changes);
  Patch(Patch &&);
  ~Patch();

  bool splice(Point new_splice_start,
              Point new_deletion_extent, Point new_insertion_extent,
              optional<Text> &&deleted_text = optional<Text> {},
              optional<Text> &&inserted_text = optional<Text> {},
              uint32_t deleted_text_size = 0);
  bool splice_old(Point start, Point deletion_extent, Point insertion_extent);
  Patch copy();
  Patch invert();
  std::vector<Change> get_changes() const;
  std::vector<Change> get_changes_in_new_range(Point start, Point end);
  std::vector<Change> get_changes_in_old_range(Point start, Point end);
  optional<Change> change_for_old_position(Point position);
  optional<Change> change_for_new_position(Point position);
  optional<Change> change_ending_after_new_position(Point position, bool exclusive = false);

  void serialize(Serializer &serializer) const;
  std::string get_dot_graph() const;
  std::string get_json() const;
  void rebalance();
  size_t get_change_count() const;

private:
  template <typename CoordinateSpace>
  std::vector<Change> get_changes_in_range(Point, Point, bool inclusive = false);

  template <typename CoordinateSpace>
  Node *splay_node_ending_before(Point target);

  template <typename CoordinateSpace>
  Node *splay_node_starting_before(Point target);

  template <typename CoordinateSpace>
  Node *splay_node_ending_after(Point target, optional<Point> exclusive_lower_bound);

  template <typename CoordinateSpace>
  Node *splay_node_starting_after(Point target, optional<Point> exclusive_lower_bound);

  template <typename CoordinateSpace>
  optional<Change> change_for_position(Point position);

  Change change_for_root_node();

  optional<Text> compute_old_text(optional<Text> &&, Point, Point);
  uint32_t compute_old_text_size(uint32_t, Point, Point);

  void splay_node(Node *);
  void rotate_node_right(Node *, Node *, Node *);
  void rotate_node_left(Node *, Node *, Node *);
  void delete_root();
  void perform_rebalancing_rotations(uint32_t);
  Node *build_node(Node *, Node *, Point, Point, Point, Point,
                  optional<Text> &&, optional<Text> &&, uint32_t old_text_size);
  void delete_node(Node **);
  bool is_frozen() const;
};

std::ostream &operator<<(std::ostream &, const Patch::Change &);

#endif // PATCH_H_
