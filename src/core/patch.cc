#include "patch.h"
#include "optional.h"
#include "text.h"
#include "text-slice.h"
#include <assert.h>
#include <cmath>
#include <memory>
#include <stdio.h>
#include <sstream>
#include <vector>

using std::function;
using std::move;
using std::vector;
using std::unique_ptr;
using std::ostream;
using std::endl;
using Change = Patch::Change;

static const uint32_t SERIALIZATION_VERSION = 1;

struct Patch::Node {
  Node *left;
  Node *right;

  Point old_extent;
  Point new_extent;

  Point old_distance_from_left_ancestor;
  Point new_distance_from_left_ancestor;

  unique_ptr<Text> old_text;
  unique_ptr<Text> new_text;
  uint32_t old_text_size_;

  uint32_t old_subtree_text_size;
  uint32_t new_subtree_text_size;

  explicit Node(
    Node *left,
    Node *right,
    Point old_extent,
    Point new_extent,
    Point old_distance_from_left_ancestor,
    Point new_distance_from_left_ancestor,
    unique_ptr<Text> &&old_text,
    unique_ptr<Text> &&new_text,
    uint32_t old_text_size
  ) :
    left{left},
    right{right},
    old_extent{old_extent},
    new_extent{new_extent},
    old_distance_from_left_ancestor{old_distance_from_left_ancestor},
    new_distance_from_left_ancestor{new_distance_from_left_ancestor},
    old_text{std::move(old_text)},
    new_text{std::move(new_text)},
    old_text_size_{old_text_size} {
    compute_subtree_text_sizes();
  }

  explicit Node(Deserializer &input) :
    left{nullptr},
    right{nullptr},
    old_extent{input},
    new_extent{input},
    old_distance_from_left_ancestor{input},
    new_distance_from_left_ancestor{input} {

    if (input.read<uint32_t>()) {
      old_text = unique_ptr<Text>{new Text{input}};
      old_text_size_ = 0;
    } else {
      old_text = nullptr;
      old_text_size_ = input.read<uint32_t>();
    }

    if (input.read<uint32_t>()) {
      new_text = unique_ptr<Text>{new Text{input}};
    } else {
      new_text = nullptr;
    }
  }

  void compute_subtree_text_sizes() {
    old_subtree_text_size =
      old_text_size() + left_subtree_old_text_size() + right_subtree_old_text_size();
    new_subtree_text_size =
      new_text_size() + left_subtree_new_text_size() + right_subtree_new_text_size();
  }

  void set_old_text(optional<Text> &&text, uint32_t old_text_size) {
    if (text) {
      old_text = unique_ptr<Text>{new Text{move(*text)}};
      old_text_size_ = 0;
    } else {
      old_text = nullptr;
      old_text_size_ = old_text_size;
    }
  }

  uint32_t old_text_size() const {
    return old_text ? old_text->size() : old_text_size_;
  }

  uint32_t left_subtree_old_text_size() const {
    return left ? left->old_subtree_text_size : 0;
  }

  uint32_t right_subtree_old_text_size() const {
    return right ? right->old_subtree_text_size : 0;
  }

  void set_new_text(optional<Text> &&text) {
    if (text) {
      new_text = unique_ptr<Text>{new Text{move(*text)}};
    } else {
      new_text = nullptr;
    }
  }

  uint32_t new_text_size() const {
    return new_text ? new_text->size() : 0;
  }

  uint32_t left_subtree_new_text_size() const {
    return left ? left->new_subtree_text_size : 0;
  }

  uint32_t right_subtree_new_text_size() const {
    return right ? right->new_subtree_text_size : 0;
  }

  void get_subtree_end(Point *old_end, Point *new_end) {
    Node *node = this;
    *old_end = Point();
    *new_end = Point();
    while (node) {
      *old_end = old_end->traverse(node->old_distance_from_left_ancestor)
                     .traverse(node->old_extent);
      *new_end = new_end->traverse(node->new_distance_from_left_ancestor)
                     .traverse(node->new_extent);
      node = node->right;
    }
  }

  Node *copy() {
    auto result = new Node{
      left,
      right,
      old_extent,
      new_extent,
      old_distance_from_left_ancestor,
      new_distance_from_left_ancestor,
      old_text ? unique_ptr<Text>(new Text(*old_text)) : nullptr,
      new_text ? unique_ptr<Text>(new Text(*new_text)) : nullptr,
      old_text_size_,
    };
    result->old_subtree_text_size = old_subtree_text_size;
    result->new_subtree_text_size = new_subtree_text_size;
    return result;
  }

  Node *invert() {
    auto result = new Node{
      left,
      right,
      new_extent,
      old_extent,
      new_distance_from_left_ancestor,
      old_distance_from_left_ancestor,
      new_text ? unique_ptr<Text>(new Text(*new_text)) : nullptr,
      old_text ? unique_ptr<Text>(new Text(*old_text)) : nullptr,
      new_text ? new_text->size() : 0
    };
    result->old_subtree_text_size = new_subtree_text_size;
    result->new_subtree_text_size = old_subtree_text_size;
    return result;
  }

  void serialize(Serializer &output) const {
    old_extent.serialize(output);
    new_extent.serialize(output);
    old_distance_from_left_ancestor.serialize(output);
    new_distance_from_left_ancestor.serialize(output);
    if (old_text) {
      output.append<uint32_t>(1);
      old_text->serialize(output);
    } else {
      output.append<uint32_t>(0);
      output.append<uint32_t>(old_text_size_);
    }
    if (new_text) {
      output.append<uint32_t>(1);
      new_text->serialize(output);
    } else {
      output.append<uint32_t>(0);
    }
  }

  void write_dot_graph(std::stringstream &result, Point left_ancestor_old_end, Point left_ancestor_new_end) {
    Point node_old_start = left_ancestor_old_end.traverse(old_distance_from_left_ancestor);
    Point node_new_start = left_ancestor_new_end.traverse(new_distance_from_left_ancestor);
    Point node_old_end = node_old_start.traverse(old_extent);
    Point node_new_end = node_new_start.traverse(new_extent);

    result << "node_" << this << " ["
      << "label=\""
      << "new range: " << node_new_start << " - " << node_new_end << ", " << endl
      << "old range: " << node_old_start << " - " << node_old_end << ", " << endl
      << "new text: ";

    if (new_text) {
      result << "\\\"" << *new_text << "\\\"" << endl;
    } else {
      result << "null" << endl;
    }

    result << "old text: ";
    if (old_text) {
      result << "\\\"" << *old_text <<  "\\\""  << endl;
    } else {
      result << "null" << endl;
      result << "old text size:" << old_text_size_ << endl;
    }

    result << "old_subtree_text_size: " << old_subtree_text_size << endl;
    result << "new_subtree_text_size: " << new_subtree_text_size << endl;

    result << "\""
      << "tooltip=\"" << this
      << "\"]" << endl;

    result << "node_" << this << " -> ";
    if (left) {
      result << "node_" << left << endl;
      left->write_dot_graph(result, left_ancestor_old_end, left_ancestor_new_end);
    } else {
      result << "node_" << this << "_left_null" << endl;
      result << "node_" << this << "_left_null [label=\"\" shape=point]" << endl;
    }

    result << "node_" << this << " -> ";
    if (right) {
      result << "node_" << right << endl;
      right->write_dot_graph(result, node_old_end, node_new_end);
    } else {
      result << "node_" << this << "_right_null" << endl;
      result << "node_" << this << "_right_null [label=\"\" shape=point]" << endl;
    }
  }

  void write_json(std::stringstream &result, Point left_ancestor_old_end, Point left_ancestor_new_end) {
    Point node_old_start = left_ancestor_old_end.traverse(old_distance_from_left_ancestor);
    Point node_new_start = left_ancestor_new_end.traverse(new_distance_from_left_ancestor);
    Point node_old_end = node_old_start.traverse(old_extent);
    Point node_new_end = node_new_start.traverse(new_extent);

    result << "{";
    result << "\"id\": \"" << this << "\", ";
    result << "\"oldStart\": {\"row\": " << node_old_start.row << ", \"column\": " << node_old_start.column << "}, ";
    result << "\"oldEnd\": {\"row\": " << node_old_end.row << ", \"column\": " << node_old_end.column << "}, ";
    result << "\"newStart\": {\"row\": " << node_new_start.row << ", \"column\": " << node_new_start.column << "}, ";
    result << "\"newEnd\": {\"row\": " << node_new_end.row << ", \"column\": " << node_new_end.column << "}, ";
    result << "\"left\": ";
    if (left) {
      left->write_json(result, left_ancestor_old_end, left_ancestor_new_end);
    } else {
      result << "null";
    }
    result << ", ";
    result << "\"right\": ";
    if (right) {
      right->write_json(result, node_old_end, node_new_end);
    } else {
      result << "null";
    }
    result << "}";
  }
};

struct Patch::PositionStackEntry {
  Point old_end;
  Point new_end;
  uint32_t total_old_text_size;
  uint32_t total_new_text_size;

  PositionStackEntry() : total_old_text_size{0}, total_new_text_size{0} {}
  PositionStackEntry(Point old_end, Point new_end, uint32_t total_old_text_size, uint32_t total_new_text_size) :
    old_end{old_end},
    new_end{new_end},
    total_old_text_size{total_old_text_size},
    total_new_text_size{total_new_text_size} {}
};

struct Patch::OldCoordinates {
  static Point distance_from_left_ancestor(const Node *node) {
    return node->old_distance_from_left_ancestor;
  }
  static Point extent(const Node *node) { return node->old_extent; }
  static Point start(const Change &change) { return change.old_start; }
  static Point end(const Change &change) { return change.old_end; }
  static Point choose(Point old, Point new_) { return old; }
};

struct Patch::NewCoordinates {
  static Point distance_from_left_ancestor(const Node *node) {
    return node->new_distance_from_left_ancestor;
  }
  static Point extent(const Node *node) { return node->new_extent; }
  static Point start(const Change &change) { return change.new_start; }
  static Point end(const Change &change) { return change.new_end; }
  static Point choose(Point old, Point new_) { return new_; }
};

// Construction and destruction

Patch::Patch(bool merges_adjacent_changes)
  : root{nullptr}, change_count{0}, merges_adjacent_changes{merges_adjacent_changes} {}

Patch::Patch(Patch &&other)
  : root{nullptr}, change_count{other.change_count},
    merges_adjacent_changes{other.merges_adjacent_changes} {
  *this = move(other);
}

Patch::Patch(Node *root, uint32_t change_count, bool merges_adjacent_changes)
  : root{root}, change_count{change_count},
    merges_adjacent_changes{merges_adjacent_changes} {}

enum Transition : uint32_t { None, Left, Right, Up };

Patch::Patch(Deserializer &input) :
  root{nullptr},
  change_count{0},
  merges_adjacent_changes{true} {
  uint32_t serialization_version = input.read<uint32_t>();
  if (serialization_version != SERIALIZATION_VERSION) return;

  change_count = input.read<uint32_t>();
  if (change_count == 0) return;

  node_stack.reserve(change_count);
  root = new Node(input);
  Node *node = root, *next_node = nullptr;

  for (uint32_t i = 1; i < change_count;) {
    switch (input.read<uint32_t>()) {
    case Left:
      next_node = new Node(input);
      node->left = next_node;
      node_stack.push_back(node);
      node = next_node;
      i++;
      break;
    case Right:
      next_node = new Node(input);
      node->right = next_node;
      node_stack.push_back(node);
      node = next_node;
      i++;
      break;
    case Up:
      node->compute_subtree_text_sizes();
      node = node_stack.back();
      node_stack.pop_back();
      break;
    default:
      delete root;
      return;
    }
  }

  node->compute_subtree_text_sizes();
  for (auto iter = node_stack.rbegin(); iter != node_stack.rend(); ++iter) {
    (*iter)->compute_subtree_text_sizes();
  }
}

Patch &Patch::operator=(Patch &&other) {
  std::swap(root, other.root);
  std::swap(left_ancestor_stack, other.left_ancestor_stack);
  std::swap(node_stack, other.node_stack);
  std::swap(change_count, other.change_count);
  merges_adjacent_changes = other.merges_adjacent_changes;
  return *this;
}

Patch::~Patch() {
  if (root) delete_node(&root);
}

void Patch::serialize(Serializer &output) {
  output.append(SERIALIZATION_VERSION);
  output.append(change_count);

  if (!root) return;
  root->serialize(output);

  Node *node = root;
  node_stack.clear();
  int previous_node_child_index = -1;

  while (node) {
    if (node->left && previous_node_child_index < 0) {
      output.append<uint32_t>(Left);
      node->left->serialize(output);
      node_stack.push_back(node);
      node = node->left;
      previous_node_child_index = -1;
    } else if (node->right && previous_node_child_index < 1) {
      output.append<uint32_t>(Right);
      node->right->serialize(output);
      node_stack.push_back(node);
      node = node->right;
      previous_node_child_index = -1;
    } else if (!node_stack.empty()) {
      output.append<uint32_t>(Up);
      Node *parent = node_stack.back();
      node_stack.pop_back();
      previous_node_child_index = (node == parent->left) ? 0 : 1;
      node = parent;
    } else {
      break;
    }
  }
}

Patch Patch::copy() {
  Node *new_root = nullptr;
  if (root) {
    new_root = root->copy();
    node_stack.clear();
    node_stack.push_back(new_root);

    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left) {
        node->left = node->left->copy();
        node_stack.push_back(node->left);
      }
      if (node->right) {
        node->right = node->right->copy();
        node_stack.push_back(node->right);
      }
    }
  }

  return Patch{new_root, change_count, merges_adjacent_changes};
}

Patch Patch::invert() {
  Node *inverted_root = nullptr;
  if (root) {
    inverted_root = root->invert();
    node_stack.clear();
    node_stack.push_back(inverted_root);

    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left) {
        node->left = node->left->invert();
        node_stack.push_back(node->left);
      }
      if (node->right) {
        node->right = node->right->invert();
        node_stack.push_back(node->right);
      }
    }
  }

  return Patch{inverted_root, change_count, merges_adjacent_changes};
}

// Mutations

bool Patch::splice(Point new_splice_start,
                   Point new_deletion_extent, Point new_insertion_extent,
                   optional<Text> &&deleted_text, optional<Text> &&inserted_text,
                   uint32_t deleted_text_size) {
  if (new_deletion_extent.is_zero() && new_insertion_extent.is_zero()) return true;

  if (!root) {
    root = build_node(nullptr, nullptr, new_splice_start, new_splice_start,
                     new_deletion_extent, new_insertion_extent,
                     move(deleted_text), move(inserted_text),
                     deleted_text_size);
    return true;
  }

  Point new_deletion_end = new_splice_start.traverse(new_deletion_extent);
  Point new_insertion_end = new_splice_start.traverse(new_insertion_extent);

  Node *lower_bound = splay_node_starting_before<NewCoordinates>(new_splice_start);

  auto old_text_result = compute_old_text(move(deleted_text), new_splice_start, new_deletion_end);
  if (!old_text_result.second) return false;
  optional<Text> old_text = move(old_text_result.first);

  uint32_t old_text_size = 0;
  if (!old_text) {
    old_text_size = compute_old_text_size(deleted_text_size, new_splice_start, new_deletion_end);
  }

  Node *upper_bound = splay_node_ending_after<NewCoordinates>(new_deletion_end, new_splice_start);
  if (upper_bound && lower_bound && lower_bound != upper_bound) {
    if (lower_bound != upper_bound->left) {
      rotate_node_right(lower_bound, upper_bound->left, upper_bound);
    }
  }

  if (lower_bound && upper_bound) {
    Point lower_bound_old_start = lower_bound->old_distance_from_left_ancestor;
    Point lower_bound_new_start = lower_bound->new_distance_from_left_ancestor;
    Point upper_bound_old_start = upper_bound->old_distance_from_left_ancestor;
    Point upper_bound_new_start = upper_bound->new_distance_from_left_ancestor;
    Point lower_bound_old_end = lower_bound_old_start.traverse(lower_bound->old_extent);
    Point lower_bound_new_end = lower_bound_new_start.traverse(lower_bound->new_extent);
    Point upper_bound_old_end = upper_bound_old_start.traverse(upper_bound->old_extent);
    Point upper_bound_new_end = upper_bound_new_start.traverse(upper_bound->new_extent);

    bool overlaps_lower_bound, overlaps_upper_bound;
    if (merges_adjacent_changes) {
      overlaps_lower_bound = new_splice_start <= lower_bound_new_end;
      overlaps_upper_bound = new_deletion_end >= upper_bound_new_start;
    } else {
      overlaps_lower_bound =
        new_splice_start < lower_bound_new_end && new_deletion_end > lower_bound_new_start;
      overlaps_upper_bound =
        new_splice_start < upper_bound_new_end && new_deletion_end > upper_bound_new_start;
    }

    if (overlaps_lower_bound && overlaps_upper_bound) {
      Point new_extent_prefix = new_splice_start.traversal(lower_bound_new_start);
      Point new_extent_suffix = upper_bound_new_end.traversal(new_deletion_end);

      if (inserted_text && lower_bound->new_text && upper_bound->new_text) {
        TextSlice new_text_prefix = TextSlice(*lower_bound->new_text).prefix(new_extent_prefix);
        TextSlice new_text_suffix = TextSlice(*upper_bound->new_text).suffix(
          new_deletion_end.traversal(upper_bound_new_start)
        );
        if (!new_text_suffix.is_valid()) return false;
        upper_bound->set_new_text(
          Text::concat(new_text_prefix, *inserted_text, new_text_suffix)
        );
      } else {
        upper_bound->set_new_text(optional<Text>{});
      }

      upper_bound->set_old_text(move(old_text), old_text_size);
      upper_bound->old_extent =
        upper_bound_old_end.traversal(lower_bound_old_start);
      upper_bound->new_extent =
        new_extent_prefix.traverse(new_insertion_extent).traverse(new_extent_suffix);
      upper_bound->old_distance_from_left_ancestor = lower_bound_old_start;
      upper_bound->new_distance_from_left_ancestor = lower_bound_new_start;

      if (lower_bound == upper_bound) {
        if (root->old_extent.is_zero() && root->new_extent.is_zero()) {
          delete_root();
          lower_bound = upper_bound = nullptr;
        }
      } else {
        upper_bound->left = lower_bound->left;
        lower_bound->left = nullptr;
        delete_node(&lower_bound);
      }
    } else if (overlaps_upper_bound) {
      Point old_splice_start =
        lower_bound_old_end.traverse(new_splice_start.traversal(lower_bound_new_end));
      Point new_extent_suffix =
        upper_bound_new_end.traversal(new_deletion_end);

      if (inserted_text && upper_bound->new_text) {
        TextSlice new_text_suffix = TextSlice(*upper_bound->new_text).suffix(
          new_deletion_end.traversal(upper_bound_new_start)
        );
        if (!new_text_suffix.is_valid()) return false;
        upper_bound->set_new_text(Text::concat(*inserted_text, new_text_suffix));
      } else {
        upper_bound->set_new_text(optional<Text>{});
      }

      upper_bound->set_old_text(move(old_text), old_text_size);
      upper_bound->old_distance_from_left_ancestor = old_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent = upper_bound_old_end.traversal(old_splice_start);
      upper_bound->new_extent = new_insertion_extent.traverse(new_extent_suffix);

      delete_node(&lower_bound->right);
      if (upper_bound->left != lower_bound) {
        delete_node(&upper_bound->left);
      }
    } else if (overlaps_lower_bound) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      lower_bound->get_subtree_end(&rightmost_child_old_end, &rightmost_child_new_end);
      Point old_deletion_end =
        rightmost_child_old_end.traverse(new_deletion_end.traversal(rightmost_child_new_end));
      Point new_extent_prefix =
        new_splice_start.traversal(lower_bound_new_start);

      upper_bound->new_distance_from_left_ancestor =
        new_insertion_end.traverse(upper_bound_new_start.traversal(new_deletion_end));
      lower_bound->old_extent =
          old_deletion_end.traversal(lower_bound_old_start);
      lower_bound->new_extent =
          new_extent_prefix.traverse(new_insertion_extent);
      if (inserted_text && lower_bound->new_text) {
        TextSlice new_text_prefix = TextSlice(*lower_bound->new_text).prefix(new_extent_prefix);
        lower_bound->set_new_text(Text::concat(new_text_prefix, *inserted_text));
      } else {
        lower_bound->set_new_text(optional<Text>{});
      }

      lower_bound->set_old_text(move(old_text), old_text_size);
      delete_node(&lower_bound->right);
      rotate_node_right(lower_bound, upper_bound, nullptr);
    } else {
      // If bounds are the same node, this is an insertion at the beginning of
      // that node with merges_adjacent_changes set to false.
      if (lower_bound == upper_bound) {
        assert(!merges_adjacent_changes);
        assert(new_deletion_extent.is_zero());
        assert(new_splice_start == upper_bound_new_start);

        root = build_node(
          upper_bound->left, upper_bound,
          upper_bound_old_start, upper_bound_new_start,
          Point(), new_insertion_extent,
          move(old_text), move(inserted_text),
          old_text_size
        );

        upper_bound->left = nullptr;
        upper_bound->old_distance_from_left_ancestor = Point();
        upper_bound->new_distance_from_left_ancestor = Point();
      } else {
        Point rightmost_child_old_end, rightmost_child_new_end;
        lower_bound->get_subtree_end(&rightmost_child_old_end, &rightmost_child_new_end);
        Point old_splice_start =
          lower_bound_old_end.traverse(new_splice_start.traversal(lower_bound_new_end));
        Point old_deletion_end =
          rightmost_child_old_end.traverse(new_deletion_end.traversal(rightmost_child_new_end));
        root = build_node(
          lower_bound, upper_bound,
          old_splice_start, new_splice_start,
          old_deletion_end.traversal(old_splice_start), new_insertion_extent,
          move(old_text), move(inserted_text),
          old_text_size
        );
        delete_node(&lower_bound->right);
        upper_bound->left = nullptr;
        upper_bound->old_distance_from_left_ancestor =
            upper_bound_old_start.traversal(old_deletion_end);
        upper_bound->new_distance_from_left_ancestor =
            upper_bound_new_start.traversal(new_deletion_end);
      }
    }
  } else if (lower_bound) {
    Point lower_bound_old_start = lower_bound->old_distance_from_left_ancestor;
    Point lower_bound_new_start = lower_bound->new_distance_from_left_ancestor;
    Point lower_bound_new_end =
      lower_bound_new_start.traverse(lower_bound->new_extent);
    Point lower_bound_old_end =
      lower_bound_old_start.traverse(lower_bound->old_extent);
    Point rightmost_child_old_end, rightmost_child_new_end;
    lower_bound->get_subtree_end(&rightmost_child_old_end, &rightmost_child_new_end);
    Point old_deletion_end =
      rightmost_child_old_end.traverse(new_deletion_end.traversal(rightmost_child_new_end));
    bool overlaps_lower_bound =
      new_splice_start < lower_bound_new_end ||
      (merges_adjacent_changes && new_splice_start == lower_bound_new_end);

    if (overlaps_lower_bound) {
      if (inserted_text && lower_bound->new_text) {
        TextSlice new_text_prefix = TextSlice(*lower_bound->new_text).prefix(
          new_splice_start.traversal(lower_bound_new_start)
        );
        if (!new_text_prefix.is_valid()) return false;
        lower_bound->set_new_text(Text::concat(new_text_prefix, *inserted_text));
      } else {
        lower_bound->set_new_text(optional<Text>{});
      }

      lower_bound->set_old_text(move(old_text), old_text_size);
      lower_bound->old_extent = old_deletion_end.traversal(lower_bound_old_start);
      lower_bound->new_extent = new_insertion_end.traversal(lower_bound_new_start);
    } else {
      Point old_splice_start = lower_bound_old_end.traverse(
        new_splice_start.traversal(lower_bound_new_end)
      );
      root = build_node(
        lower_bound, nullptr,
        old_splice_start, new_splice_start,
        old_deletion_end.traversal(old_splice_start), new_insertion_extent,
        move(old_text), move(inserted_text),
        old_text_size
      );
    }

    delete_node(&lower_bound->right);
  } else if (upper_bound) {
    Point upper_bound_new_start = upper_bound->new_distance_from_left_ancestor;
    Point upper_bound_old_start = upper_bound->old_distance_from_left_ancestor;
    Point upper_bound_new_end =
      upper_bound_new_start.traverse(upper_bound->new_extent);
    bool overlaps_upper_bound =
      new_deletion_end > upper_bound_new_start ||
      (merges_adjacent_changes && new_deletion_end == upper_bound_new_start);

    Point old_deletion_end;
    if (upper_bound->left) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      upper_bound->left->get_subtree_end(&rightmost_child_old_end, &rightmost_child_new_end);
      old_deletion_end =
        rightmost_child_old_end.traverse(new_deletion_end.traversal(rightmost_child_new_end));
    } else {
      old_deletion_end = new_deletion_end;
    }

    if (overlaps_upper_bound) {
      if (inserted_text && upper_bound->new_text) {
        TextSlice new_text_suffix = TextSlice(*upper_bound->new_text).suffix(
          new_deletion_end.traversal(upper_bound_new_start)
        );
        if (!new_text_suffix.is_valid()) return false;
        upper_bound->set_new_text(Text::concat(*inserted_text, new_text_suffix));
      } else {
        upper_bound->set_new_text(optional<Text>{});
      }

      upper_bound->set_old_text(move(old_text), old_text_size);
      upper_bound->old_distance_from_left_ancestor = new_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent =
        upper_bound_old_start.traversal(new_splice_start).traverse(upper_bound->old_extent);
      upper_bound->new_extent =
        new_insertion_extent.traverse(upper_bound_new_end.traversal(new_deletion_end));
    } else {
      root = build_node(
        nullptr, upper_bound,
        new_splice_start, new_splice_start,
        old_deletion_end.traversal(new_splice_start), new_insertion_extent,
        move(old_text), move(inserted_text),
        old_text_size
      );
      Point distance_from_end_of_root_to_start_of_upper_bound =
        upper_bound_new_start.traversal(new_deletion_end);
      upper_bound->old_distance_from_left_ancestor =
        distance_from_end_of_root_to_start_of_upper_bound;
      upper_bound->new_distance_from_left_ancestor =
        distance_from_end_of_root_to_start_of_upper_bound;
    }

    delete_node(&upper_bound->left);
  } else {
    Point rightmost_child_old_end, rightmost_child_new_end;
    root->get_subtree_end(&rightmost_child_old_end, &rightmost_child_new_end);
    Point old_deletion_end = rightmost_child_old_end.traverse(
      new_deletion_end.traversal(rightmost_child_new_end));
    delete_node(&root);
    root = build_node(
      nullptr, nullptr,
      new_splice_start, new_splice_start,
      old_deletion_end.traversal(new_splice_start), new_insertion_extent,
      move(old_text), move(inserted_text),
      old_text_size
    );
  }

  if (lower_bound) lower_bound->compute_subtree_text_sizes();
  if (upper_bound) upper_bound->compute_subtree_text_sizes();

  return true;
}

void Patch::splice_old(Point old_splice_start, Point old_deletion_extent,
                      Point old_insertion_extent) {
  if (!root) return;

  Point old_deletion_end = old_splice_start.traverse(old_deletion_extent);
  Point old_insertion_end = old_splice_start.traverse(old_insertion_extent);

  Node *lower_bound = splay_node_ending_before<OldCoordinates>(old_splice_start);
  Node *upper_bound = splay_node_starting_after<OldCoordinates>(old_deletion_end, old_splice_start);

  if (!lower_bound && !upper_bound) {
    delete_node(&root);
    return;
  }

  if (upper_bound == lower_bound) {
    assert(upper_bound->old_extent.is_zero());
    assert(old_deletion_extent.is_zero());
    root->old_distance_from_left_ancestor =
        root->old_distance_from_left_ancestor.traverse(old_insertion_extent);
    root->new_distance_from_left_ancestor =
        root->new_distance_from_left_ancestor.traverse(old_insertion_extent);
    return;
  }

  if (upper_bound && lower_bound) {
    if (lower_bound != upper_bound->left) {
      rotate_node_right(lower_bound, upper_bound->left, upper_bound);
    }
  }

  Point new_deletion_end, new_insertion_end;

  if (lower_bound) {
    Point lower_bound_old_start = lower_bound->old_distance_from_left_ancestor;
    Point lower_bound_new_start = lower_bound->new_distance_from_left_ancestor;
    Point lower_bound_old_end =
        lower_bound_old_start.traverse(lower_bound->old_extent);
    Point lower_bound_new_end =
        lower_bound_new_start.traverse(lower_bound->new_extent);
    new_deletion_end = lower_bound_new_end.traverse(
        old_deletion_end.traversal(lower_bound_old_end));
    new_insertion_end = lower_bound_new_end.traverse(
        old_insertion_end.traversal(lower_bound_old_end));

    delete_node(&lower_bound->right);
  } else {
    new_deletion_end = old_deletion_end;
    new_insertion_end = old_insertion_end;
  }

  if (upper_bound) {
    Point distance_between_splice_and_upper_bound =
        upper_bound->old_distance_from_left_ancestor.traversal(
            old_deletion_end);
    upper_bound->old_distance_from_left_ancestor =
        old_insertion_end.traverse(distance_between_splice_and_upper_bound);
    upper_bound->new_distance_from_left_ancestor =
        new_insertion_end.traverse(distance_between_splice_and_upper_bound);

    if (lower_bound) {
      if (lower_bound->old_distance_from_left_ancestor.traverse(
              lower_bound->old_extent) ==
          upper_bound->old_distance_from_left_ancestor) {
        upper_bound->old_distance_from_left_ancestor =
            lower_bound->old_distance_from_left_ancestor;
        upper_bound->new_distance_from_left_ancestor =
            lower_bound->new_distance_from_left_ancestor;

        upper_bound->old_extent =
            lower_bound->old_extent.traverse(upper_bound->old_extent);
        if (lower_bound->old_text && upper_bound->old_text) {
          lower_bound->old_text->append(*upper_bound->old_text);
          std::swap(upper_bound->old_text, lower_bound->old_text);
        } else {
          upper_bound->old_text = nullptr;
          upper_bound->old_text_size_ += lower_bound->old_text_size_;
        }

        upper_bound->new_extent =
            lower_bound->new_extent.traverse(upper_bound->new_extent);
        if (lower_bound->new_text && upper_bound->new_text) {
          lower_bound->new_text->append(*upper_bound->new_text);
          std::swap(upper_bound->new_text, lower_bound->new_text);
        } else {
          upper_bound->new_text = nullptr;
        }

        upper_bound->left = lower_bound->left;
        lower_bound->left = nullptr;

        delete_node(&lower_bound);
      }
    } else {
      delete_node(&upper_bound->left);
    }
  }

  if (lower_bound) lower_bound->compute_subtree_text_sizes();
  if (upper_bound) upper_bound->compute_subtree_text_sizes();
}

bool Patch::combine(const Patch &other, bool left_to_right) {
  auto changes = other.get_changes();
  if (left_to_right) {
    for (auto iter = changes.begin(), end = changes.end(); iter != end; ++iter) {
      if (!splice(iter->new_start, iter->old_end.traversal(iter->old_start),
                  iter->new_end.traversal(iter->new_start),
                  iter->old_text ? *iter->old_text : optional<Text>{},
                  iter->new_text ? *iter->new_text : optional<Text>{},
                  iter->old_text_size)) return false;
      remove_noop_change();
    }
  } else {
    for (auto iter = changes.rbegin(), end = changes.rend(); iter != end;
         ++iter) {
      if (!splice(iter->old_start, iter->old_end.traversal(iter->old_start),
                  iter->new_end.traversal(iter->new_start),
                  iter->old_text ? *iter->old_text : optional<Text>{},
                  iter->new_text ? *iter->new_text : optional<Text>{},
                  iter->old_text_size)) return false;
      remove_noop_change();
    }
  }
  return true;
}

void Patch::clear() {
  if (root) delete_node(&root);
}

void Patch::rebalance() {
  if (!root)
    return;

  // Transform tree to vine
  Node *pseudo_root = root, *pseudo_root_parent = nullptr;
  while (pseudo_root) {
    Node *left = pseudo_root->left;
    Node *right = pseudo_root->right;
    if (left) {
      rotate_node_right(left, pseudo_root, pseudo_root_parent);
      pseudo_root = left;
    } else {
      pseudo_root_parent = pseudo_root;
      pseudo_root = right;
    }
  }

  // Transform vine to balanced tree
  uint32_t n = change_count;
  uint32_t m = std::pow(2, std::floor(std::log2(n + 1))) - 1;
  perform_rebalancing_rotations(n - m);
  while (m > 1) {
    m = m / 2;
    perform_rebalancing_rotations(m);
  }
}

// Non-splaying reads

vector<Change> Patch::get_changes() const {
  return get_changes_in_range<NewCoordinates>(
    Point(),
    Point(UINT32_MAX, UINT32_MAX),
    true
  );
}

size_t Patch::get_change_count() const { return change_count; }

optional<Change> Patch::get_bounds() const {
  if (!root) return optional<Change>{};

  Node *node = root;
  while (node->left) {
    node = node->left;
  }

  Point old_start = node->old_distance_from_left_ancestor;
  Point new_start = node->new_distance_from_left_ancestor;

  node = root;
  Point old_end, new_end;
  while (node) {
    old_end = old_end.traverse(node->old_distance_from_left_ancestor.traverse(node->old_extent));
    new_end = new_end.traverse(node->new_distance_from_left_ancestor.traverse(node->new_extent));
    node = node->right;
  }

  return Change{
    old_start, old_end,
    new_start, new_end,
    nullptr, nullptr,
    0, 0, 0
  };
}

vector<Change> Patch::get_changes_in_old_range(Point start, Point end) const {
  return get_changes_in_range<OldCoordinates>(start, end, false);
}

vector<Change> Patch::get_changes_in_new_range(Point start, Point end) const {
  return get_changes_in_range<NewCoordinates>(start, end, false);
}

optional<Change> Patch::get_change_starting_before_old_position(Point target) const {
  return get_change_starting_before_position<OldCoordinates>(target);
}

optional<Change> Patch::get_change_starting_before_new_position(Point target) const {
  return get_change_starting_before_position<NewCoordinates>(target);
}

optional<Change> Patch::get_change_ending_after_new_position(Point target) const {
  return get_change_ending_after_position<NewCoordinates>(target);
}

Point Patch::new_position_for_new_offset(uint32_t target_offset,
                                         function<uint32_t(Point)> old_offset_for_old_position,
                                         function<Point(uint32_t)> old_position_for_old_offset) const {
  const Node *node = root;
  Patch::PositionStackEntry left_ancestor_info;
  Point preceding_new_position, preceding_old_position;
  uint32_t preceding_old_offset = 0, preceding_new_offset = 0;

  while (node) {
    Point node_old_start = left_ancestor_info.old_end.traverse(node->old_distance_from_left_ancestor);
    Point node_new_start = left_ancestor_info.new_end.traverse(node->new_distance_from_left_ancestor);
    uint32_t node_old_start_offset = old_offset_for_old_position(node_old_start);
    uint32_t node_new_start_offset = node_old_start_offset -
      left_ancestor_info.total_old_text_size +
      left_ancestor_info.total_new_text_size -
      node->left_subtree_old_text_size() +
      node->left_subtree_new_text_size();
    uint32_t node_new_end_offset = node_new_start_offset + node->new_text_size();
    uint32_t node_old_end_offset = node_old_start_offset + node->old_text_size();

    if (node_new_end_offset <= target_offset) {
      preceding_old_position = node_old_start.traverse(node->old_extent);
      preceding_new_position = node_new_start.traverse(node->new_extent);
      preceding_old_offset = node_old_end_offset;
      preceding_new_offset = node_new_end_offset;
      if (node->right) {
        left_ancestor_info.old_end = preceding_old_position;
        left_ancestor_info.new_end = preceding_new_position;
        left_ancestor_info.total_old_text_size += node->left_subtree_old_text_size() + node->old_text_size();
        left_ancestor_info.total_new_text_size += node->left_subtree_new_text_size() + node->new_text_size();
        node = node->right;
      } else {
        break;
      }
    } else if (node_new_start_offset <= target_offset) {
      return node_new_start
        .traverse(node->new_text->position_for_offset(target_offset - node_new_start_offset));
    } else {
      if (node->left) {
        node = node->left;
      } else {
        break;
      }
    }
  }

  return preceding_new_position.traverse(
    old_position_for_old_offset(
      preceding_old_offset + (target_offset - preceding_new_offset)
    ).traversal(preceding_old_position)
  );
}

// Splaying reads

vector<Change> Patch::grab_changes_in_old_range(Point start, Point end) {
  return grab_changes_in_range<OldCoordinates>(start, end);
}

vector<Change> Patch::grab_changes_in_new_range(Point start, Point end) {
  return grab_changes_in_range<NewCoordinates>(start, end);
}

optional<Change> Patch::grab_change_starting_before_old_position(Point target) {
  return grab_change_starting_before_position<OldCoordinates>(target);
}

optional<Change> Patch::grab_change_starting_before_new_position(Point target) {
  return grab_change_starting_before_position<NewCoordinates>(target);
}

optional<Change> Patch::grab_change_ending_after_new_position(Point target, bool exclusive) {
  optional<Point> exclusive_lower_bound;
  if (exclusive) exclusive_lower_bound = target;
  if (splay_node_ending_after<NewCoordinates>(target, exclusive_lower_bound)) {
    return change_for_root_node();
  } else {
    return optional<Change>{};
  }
}

// Debugging

std::string Patch::get_dot_graph() const {
  std::stringstream result;
  result << "digraph patch {" << endl;
  if (root) root->write_dot_graph(result, Point(), Point());
  result << "}" << endl;
  return result.str();
}

std::string Patch::get_json() const {
  std::stringstream result;
  if (root) root->write_json(result, Point(), Point());
  return result.str();
}

// Private - mutations

void Patch::splay_node(Node *node) {
  while (!node_stack.empty()) {
    Node *parent = node_stack.back();
    node_stack.pop_back();

    Node *grandparent = nullptr;
    if (!node_stack.empty()) {
      grandparent = node_stack.back();
      node_stack.pop_back();
    }

    if (grandparent) {
      Node *great_grandparent = nullptr;
      if (!node_stack.empty()) {
        great_grandparent = node_stack.back();
      }

      if (grandparent->left == parent && parent->right == node) {
        rotate_node_left(node, parent, grandparent);
        rotate_node_right(node, grandparent, great_grandparent);
      } else if (grandparent->right == parent && parent->left == node) {
        rotate_node_right(node, parent, grandparent);
        rotate_node_left(node, grandparent, great_grandparent);
      } else if (grandparent->left == parent && parent->left == node) {
        rotate_node_right(parent, grandparent, great_grandparent);
        rotate_node_right(node, parent, great_grandparent);
      } else if (grandparent->right == parent && parent->right == node) {
        rotate_node_left(parent, grandparent, great_grandparent);
        rotate_node_left(node, parent, great_grandparent);
      }
    } else {
      if (parent->left == node) {
        rotate_node_right(node, parent, nullptr);
      } else if (parent->right == node) {
        rotate_node_left(node, parent, nullptr);
      } else {
        assert(!"Unexpected state");
      }
    }
  }
}

void Patch::rotate_node_left(Node *pivot, Node *root, Node *root_parent) {
  if (root_parent) {
    if (root_parent->left == root) {
      root_parent->left = pivot;
    } else {
      root_parent->right = pivot;
    }
  } else {
    this->root = pivot;
  }

  root->right = pivot->left;
  pivot->left = root;

  pivot->old_distance_from_left_ancestor =
      root->old_distance_from_left_ancestor.traverse(root->old_extent)
          .traverse(pivot->old_distance_from_left_ancestor);
  pivot->new_distance_from_left_ancestor =
      root->new_distance_from_left_ancestor.traverse(root->new_extent)
          .traverse(pivot->new_distance_from_left_ancestor);

  root->compute_subtree_text_sizes();
  pivot->compute_subtree_text_sizes();
}

void Patch::rotate_node_right(Node *pivot, Node *root, Node *root_parent) {
  if (root_parent) {
    if (root_parent->left == root) {
      root_parent->left = pivot;
    } else {
      root_parent->right = pivot;
    }
  } else {
    this->root = pivot;
  }

  root->left = pivot->right;
  pivot->right = root;

  root->old_distance_from_left_ancestor =
      root->old_distance_from_left_ancestor.traversal(
          pivot->old_distance_from_left_ancestor.traverse(pivot->old_extent));
  root->new_distance_from_left_ancestor =
      root->new_distance_from_left_ancestor.traversal(
          pivot->new_distance_from_left_ancestor.traverse(pivot->new_extent));

  root->compute_subtree_text_sizes();
  pivot->compute_subtree_text_sizes();
}

void Patch::delete_root() {
  Node *node = root, *parent = nullptr;
  while (true) {
    if (node->left) {
      Node *left = node->left;
      rotate_node_right(node->left, node, parent);
      parent = left;
    } else if (node->right) {
      Node *right = node->right;
      rotate_node_left(node->right, node, parent);
      parent = right;
    } else if (parent) {
      if (parent->left == node) {
        delete_node(&parent->left);
        break;
      } else {
        delete_node(&parent->right);
        break;
      }
    } else {
      delete_node(&root);
      break;
    }

    parent->old_subtree_text_size -= node->old_text_size();
    parent->new_subtree_text_size -= node->new_text_size();
  }
}

void Patch::perform_rebalancing_rotations(uint32_t count) {
  Node *pseudo_root = root, *pseudo_root_parent = nullptr;
  for (uint32_t i = 0; i < count; i++) {
    if (!pseudo_root)
      return;
    Node *right_child = pseudo_root->right;
    if (!right_child)
      return;
    rotate_node_left(right_child, pseudo_root, pseudo_root_parent);
    pseudo_root = right_child->right;
    pseudo_root_parent = right_child;
  }
}

std::pair<optional<Text>, bool> Patch::compute_old_text(
  optional<Text> &&deleted_text, Point new_splice_start, Point new_deletion_end
) {
  if (!deleted_text) return {optional<Text>{}, true};

  Text result;

  auto overlapping_changes = grab_changes_in_range<NewCoordinates>(
    new_splice_start,
    new_deletion_end,
    merges_adjacent_changes
  );

  TextSlice deleted_text_slice = TextSlice(*deleted_text);
  Point deleted_text_slice_start = new_splice_start;

  for (const Change &change : overlapping_changes) {
    if (!change.old_text) return {optional<Text>{}, true};

    if (change.new_start > deleted_text_slice_start) {
      auto split_result = deleted_text_slice.split(
        change.new_start.traversal(deleted_text_slice_start)
      );
      if (!split_result.first.is_valid()) return {optional<Text>{}, false};
      deleted_text_slice_start = change.new_start;
      deleted_text_slice = split_result.second;
      result.append(split_result.first);
    }

    result.append(*change.old_text);
    deleted_text_slice = deleted_text_slice.suffix(Point::min(
      deleted_text_slice.extent(),
      change.new_end.traversal(deleted_text_slice_start)
    ));
    deleted_text_slice_start = change.new_end;

    if (!deleted_text_slice.is_valid()) return {optional<Text>{}, false};
  }

  result.append(deleted_text_slice);
  return {result, true};
}

uint32_t Patch::compute_old_text_size(uint32_t deleted_text_size,
                                      Point new_splice_start,
                                      Point new_deletion_end) {
  uint32_t old_text_size = deleted_text_size;
  auto overlapping_changes = grab_changes_in_range<NewCoordinates>(
    new_splice_start,
    new_deletion_end,
    merges_adjacent_changes
  );

  for (const Change &change : overlapping_changes) {
    if (!change.new_text) return 0;

    TextSlice overlapping_new_text = TextSlice(*change.new_text);
    if (new_deletion_end < change.new_end) {
      overlapping_new_text = overlapping_new_text.prefix(new_deletion_end.traversal(change.new_start));
    }
    if (new_splice_start > change.new_start) {
      overlapping_new_text = overlapping_new_text.suffix(new_splice_start.traversal(change.new_start));
    }

    old_text_size -= overlapping_new_text.size();
    old_text_size += change.old_text_size;
  }

  return old_text_size;
}

Patch::Node *Patch::build_node(Node *left, Node *right,
                       Point old_distance_from_left_ancestor,
                       Point new_distance_from_left_ancestor,
                       Point old_extent, Point new_extent,
                       optional<Text> &&old_text, optional<Text> &&new_text,
                       uint32_t old_text_size) {
  change_count++;
  return new Node{
    left,
    right,
    old_extent,
    new_extent,
    old_distance_from_left_ancestor,
    new_distance_from_left_ancestor,
    old_text ? unique_ptr<Text>{new Text(*old_text)} : nullptr,
    new_text ? unique_ptr<Text>{new Text(*new_text)} : nullptr,
    old_text_size
  };
}

void Patch::delete_node(Node **node_to_delete) {
  if (*node_to_delete) {
    node_stack.clear();
    node_stack.push_back(*node_to_delete);

    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left)
        node_stack.push_back(node->left);
      if (node->right)
        node_stack.push_back(node->right);
      delete node;
      change_count--;
    }

    *node_to_delete = nullptr;
  }
}

template <typename CoordinateSpace>
Patch::Node *Patch::splay_node_ending_before(Point target) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.traverse(
        CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.traverse(CoordinateSpace::extent(node));
    if (node_end <= target) {
      splayed_node = node;
      splayed_node_ancestor_count = node_stack.size();
      if (node->right) {
        left_ancestor_end = node_start.traverse(CoordinateSpace::extent(node));
        node_stack.push_back(node);
        node = node->right;
      } else {
        break;
      }
    } else {
      if (node->left) {
        node_stack.push_back(node);
        node = node->left;
      } else {
        break;
      }
    }
  }

  if (splayed_node) {
    node_stack.resize(splayed_node_ancestor_count);
    splay_node(splayed_node);
  }

  return splayed_node;
}

template <typename CoordinateSpace>
Patch::Node *Patch::splay_node_starting_before(Point target) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.traverse(
        CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.traverse(CoordinateSpace::extent(node));
    if (node_start <= target) {
      splayed_node = node;
      splayed_node_ancestor_count = node_stack.size();
      if (node->right) {
        left_ancestor_end = node_end;
        node_stack.push_back(node);
        node = node->right;
      } else {
        break;
      }
    } else {
      if (node->left) {
        node_stack.push_back(node);
        node = node->left;
      } else {
        break;
      }
    }
  }

  if (splayed_node) {
    node_stack.resize(splayed_node_ancestor_count);
    splay_node(splayed_node);
  }

  return splayed_node;
}

void Patch::remove_noop_change() {
  if (root && root->old_text && root->new_text && *root->old_text == *root->new_text) {
    splice_old(root->old_distance_from_left_ancestor, Point(), Point());
  }
}

// Private - non-splaying reads

template <typename CoordinateSpace>
vector<Patch::Change> Patch::get_changes_in_range(Point start, Point end, bool inclusive) const {
  vector<Change> result;

  Node *node = root;
  vector<Node *> node_stack;
  vector<Patch::PositionStackEntry> left_ancestor_stack{{}};

  Node *found_node = nullptr;
  size_t found_node_ancestor_count = 0;
  size_t found_node_left_ancestor_count = 0;

  while (node) {
    auto &left_ancestor_info = left_ancestor_stack.back();
    Point node_old_start = left_ancestor_info.old_end.traverse(node->old_distance_from_left_ancestor);
    Point node_new_start = left_ancestor_info.new_end.traverse(node->new_distance_from_left_ancestor);
    Point node_old_end = node_old_start.traverse(node->old_extent);
    Point node_new_end = node_new_start.traverse(node->new_extent);

    Point node_end = CoordinateSpace::choose(node_old_end, node_new_end);
    if (node_end > start || (inclusive && node_end == start)) {
      found_node_ancestor_count = node_stack.size();
      found_node_left_ancestor_count = left_ancestor_stack.size();
      found_node = node;
      if (node->left) {
        node_stack.push_back(node);
        node = node->left;
      } else {
        break;
      }
    } else {
      if (node->right) {
        left_ancestor_stack.push_back({
          node_old_end,
          node_new_end,
          left_ancestor_info.total_old_text_size + node->left_subtree_old_text_size() + node->old_text_size(),
          left_ancestor_info.total_new_text_size + node->left_subtree_new_text_size() + node->new_text_size()
        });
        node_stack.push_back(node);
        node = node->right;
      } else {
        break;
      }
    }
  }

  node = found_node;
  node_stack.resize(found_node_ancestor_count);
  left_ancestor_stack.resize(found_node_left_ancestor_count);

  while (node) {
    PositionStackEntry &left_ancestor_info = left_ancestor_stack.back();
    Point old_start = left_ancestor_info.old_end.traverse(
        node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_info.new_end.traverse(
        node->new_distance_from_left_ancestor);
    Point node_start = CoordinateSpace::choose(old_start, new_start);
    if (node_start > end || (!inclusive && node_start == end)) break;

    Point old_end = old_start.traverse(node->old_extent);
    Point new_end = new_start.traverse(node->new_extent);
    Text *old_text = node->old_text.get();
    Text *new_text = node->new_text.get();
    uint32_t old_text_size = node->old_text_size();
    uint32_t preceding_old_text_size =
      left_ancestor_info.total_old_text_size + node->left_subtree_old_text_size();
    uint32_t preceding_new_text_size =
      left_ancestor_info.total_new_text_size + node->left_subtree_new_text_size();
    Change change{
      old_start,
      old_end,
      new_start,
      new_end,
      old_text,
      new_text,
      preceding_old_text_size,
      preceding_new_text_size,
      old_text_size,
    };
    result.push_back(change);

    if (node->right) {
      left_ancestor_stack.push_back(PositionStackEntry{
        old_end,
        new_end,
        preceding_old_text_size + node->old_text_size(),
        preceding_new_text_size + node->new_text_size()
      });
      node_stack.push_back(node);
      node = node->right;

      while (node->left) {
        node_stack.push_back(node);
        node = node->left;
      }
    } else {
      while (!node_stack.empty() && node_stack.back()->right == node) {
        node = node_stack.back();
        node_stack.pop_back();
        left_ancestor_stack.pop_back();
      }

      if (node_stack.empty()) {
        node = nullptr;
      } else {
        node = node_stack.back();
        node_stack.pop_back();
      }
    }
  }

  return result;
}

template <typename CoordinateSpace>
optional<Patch::Change> Patch::get_change_starting_before_position(Point target) const {
  const Node *found_node = nullptr;
  const Node *node = root;
  Patch::PositionStackEntry left_ancestor_info;
  Patch::PositionStackEntry found_node_left_ancestor_info;

  while (node) {
    Point node_old_start = left_ancestor_info.old_end.traverse(node->old_distance_from_left_ancestor);
    Point node_new_start = left_ancestor_info.new_end.traverse(node->new_distance_from_left_ancestor);
    if (CoordinateSpace::choose(node_old_start, node_new_start) <= target) {
      found_node = node;
      found_node_left_ancestor_info = left_ancestor_info;
      if (node->right) {
        left_ancestor_info.old_end = node_old_start.traverse(node->old_extent);
        left_ancestor_info.new_end = node_new_start.traverse(node->new_extent);
        left_ancestor_info.total_old_text_size += node->left_subtree_old_text_size() + node->old_text_size();
        left_ancestor_info.total_new_text_size += node->left_subtree_new_text_size() + node->new_text_size();
        node = node->right;
      } else {
        break;
      }
    } else {
      if (node->left) {
        node = node->left;
      } else {
        break;
      }
    }
  }

  if (found_node) {
    Point old_start = found_node_left_ancestor_info.old_end.traverse(found_node->old_distance_from_left_ancestor);
    Point new_start = found_node_left_ancestor_info.new_end.traverse(found_node->new_distance_from_left_ancestor);

    return Change{
      old_start, old_start.traverse(found_node->old_extent),
      new_start, new_start.traverse(found_node->new_extent),
      found_node->old_text.get(),
      found_node->new_text.get(),
      found_node_left_ancestor_info.total_old_text_size + found_node->left_subtree_old_text_size(),
      found_node_left_ancestor_info.total_new_text_size + found_node->left_subtree_new_text_size(),
      found_node->old_text_size()
    };
  } else {
    return optional<Change>{};
  }
}

template <typename CoordinateSpace>
optional<Patch::Change> Patch::get_change_ending_after_position(Point target) const {
  Node *found_node = nullptr;
  Node *node = root;
  Patch::PositionStackEntry left_ancestor_info, found_node_left_ancestor_info;

  while (node) {
    Point node_old_start = left_ancestor_info.old_end.traverse(node->old_distance_from_left_ancestor);
    Point node_new_start = left_ancestor_info.new_end.traverse(node->new_distance_from_left_ancestor);
    Point node_old_end = node_old_start.traverse(node->old_extent);
    Point node_new_end = node_new_start.traverse(node->new_extent);
    if (CoordinateSpace::choose(node_old_end, node_new_end) > target) {
      found_node = node;
      found_node_left_ancestor_info = left_ancestor_info;
      if (node->left) {
        node = node->left;
      } else {
        break;
      }
    } else {
      if (node->right) {
        left_ancestor_info.old_end = node_old_end;
        left_ancestor_info.new_end = node_new_end;
        left_ancestor_info.total_old_text_size += node->left_subtree_old_text_size() + node->old_text_size();
        left_ancestor_info.total_new_text_size += node->left_subtree_new_text_size() + node->new_text_size();
        node = node->right;
      } else {
        break;
      }
    }
  }

  if (found_node) {
    Point old_start = found_node_left_ancestor_info.old_end.traverse(found_node->old_distance_from_left_ancestor);
    Point new_start = found_node_left_ancestor_info.new_end.traverse(found_node->new_distance_from_left_ancestor);

    return Change{
      old_start, old_start.traverse(found_node->old_extent),
      new_start, new_start.traverse(found_node->new_extent),
      found_node->old_text.get(),
      found_node->new_text.get(),
      found_node_left_ancestor_info.total_old_text_size + found_node->left_subtree_old_text_size(),
      found_node_left_ancestor_info.total_new_text_size + found_node->left_subtree_new_text_size(),
      found_node->old_text_size()
    };
  } else {
    return optional<Change>{};
  }
}

// Private - splaying reads

template <typename CoordinateSpace>
vector<Change> Patch::grab_changes_in_range(Point start, Point end, bool inclusive) {
  vector<Change> result;
  if (!root)
    return result;

  Node *lower_bound = splay_node_starting_before<CoordinateSpace>(start);

  node_stack.clear();
  left_ancestor_stack.clear();
  left_ancestor_stack.push_back({Point(), Point(), 0, 0});

  Node *node = root;
  if (!lower_bound) {
    while (node->left) {
      node_stack.push_back(node);
      node = node->left;
    }
  }

  while (node) {
    Patch::PositionStackEntry &left_ancestor_info =
        left_ancestor_stack.back();
    Point old_start = left_ancestor_info.old_end.traverse(
        node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_info.new_end.traverse(
        node->new_distance_from_left_ancestor);

    Point old_end = old_start.traverse(node->old_extent);
    Point new_end = new_start.traverse(node->new_extent);
    Text *old_text = node->old_text.get();
    Text *new_text = node->new_text.get();
    uint32_t old_text_size = node->old_text_size();
    uint32_t preceding_old_text_size =
      left_ancestor_info.total_old_text_size + node->left_subtree_old_text_size();
    uint32_t preceding_new_text_size =
      left_ancestor_info.total_new_text_size + node->left_subtree_new_text_size();
    Change change{
      old_start,
      old_end,
      new_start,
      new_end,
      old_text,
      new_text,
      preceding_old_text_size,
      preceding_new_text_size,
      old_text_size,
    };

    if (inclusive) {
      if (CoordinateSpace::start(change) > end) {
        break;
      }

      if (CoordinateSpace::end(change) >= start) {
        result.push_back(change);
      }
    } else {
      if (CoordinateSpace::start(change) >= end) {
        break;
      }

      if (CoordinateSpace::end(change) > start) {
        result.push_back(change);
      }
    }

    if (node->right) {
      left_ancestor_stack.push_back(Patch::PositionStackEntry{
        old_end,
        new_end,
        preceding_old_text_size + node->old_text_size(),
        preceding_new_text_size + node->new_text_size()
      });
      node_stack.push_back(node);
      node = node->right;

      while (node->left) {
        node_stack.push_back(node);
        node = node->left;
      }
    } else {
      while (!node_stack.empty() && node_stack.back()->right == node) {
        node = node_stack.back();
        node_stack.pop_back();
        left_ancestor_stack.pop_back();
      }

      if (node_stack.empty()) {
        node = nullptr;
      } else {
        node = node_stack.back();
        node_stack.pop_back();
      }
    }
  }

  return result;
}

template <typename CoordinateSpace>
optional<Change> Patch::grab_change_starting_before_position(Point target) {
  if (splay_node_starting_before<CoordinateSpace>(target)) {
    return change_for_root_node();
  } else {
    return optional<Change>{};
  }
}

template <typename CoordinateSpace>
Patch::Node *Patch::splay_node_ending_after(Point target, optional<Point> exclusive_lower_bound) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.traverse(
        CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.traverse(CoordinateSpace::extent(node));
    if (node_end >= target && (!exclusive_lower_bound || node_end > *exclusive_lower_bound)) {
      splayed_node = node;
      splayed_node_ancestor_count = node_stack.size();
      if (node->left) {
        node_stack.push_back(node);
        node = node->left;
      } else {
        break;
      }
    } else {
      if (node->right) {
        left_ancestor_end = node_end;
        node_stack.push_back(node);
        node = node->right;
      } else {
        break;
      }
    }
  }

  if (splayed_node) {
    node_stack.resize(splayed_node_ancestor_count);
    splay_node(splayed_node);
  }

  return splayed_node;
}

template <typename CoordinateSpace>
Patch::Node *Patch::splay_node_starting_after(Point target, optional<Point> exclusive_lower_bound) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.traverse(
        CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.traverse(CoordinateSpace::extent(node));
    if (node_start >= target && (!exclusive_lower_bound || node_start > *exclusive_lower_bound)) {
      splayed_node = node;
      splayed_node_ancestor_count = node_stack.size();
      if (node->left) {
        node_stack.push_back(node);
        node = node->left;
      } else {
        break;
      }
    } else {
      if (node->right) {
        left_ancestor_end = node_end;
        node_stack.push_back(node);
        node = node->right;
      } else {
        break;
      }
    }
  }

  if (splayed_node) {
    node_stack.resize(splayed_node_ancestor_count);
    splay_node(splayed_node);
  }

  return splayed_node;
}

Change Patch::change_for_root_node() {
  Point old_start = root->old_distance_from_left_ancestor;
  Point new_start = root->new_distance_from_left_ancestor;
  Point old_end = old_start.traverse(root->old_extent);
  Point new_end = new_start.traverse(root->new_extent);
  Text *old_text = root->old_text.get();
  Text *new_text = root->new_text.get();
  uint32_t old_text_size = root->old_text_size();
  uint32_t preceding_old_text_size = root->left_subtree_old_text_size();
  uint32_t preceding_new_text_size = root->left_subtree_new_text_size();
  return Change{
    old_start,
    old_end,
    new_start,
    new_end,
    old_text,
    new_text,
    preceding_old_text_size,
    preceding_new_text_size,
    old_text_size,
  };
}

ostream &operator<<(ostream &stream, const Patch::Change &change) {
  stream
    << "{Change "
    << "old_range: (" << change.old_start << " - " << change.old_end << ")"
    << ", new_range: (" << change.new_start << " - " << change.new_end << ")"
    << ", old_text: ";

  if (change.old_text) {
    stream << *change.old_text;
  } else {
    stream << "null";
  }

  stream << ", new_text: ";

  if (change.new_text) {
    stream << *change.new_text;
  } else {
    stream << "null";
  }

  stream << ", preceding_old_text_size: " << change.preceding_old_text_size;
  stream << ", preceding_new_text_size: " << change.preceding_new_text_size;

  stream << "}";

  return stream;
}
