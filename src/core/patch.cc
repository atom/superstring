#include "patch.h"
#include "optional.h"
#include "text.h"
#include <assert.h>
#include <cmath>
#include <memory>
#include <stdio.h>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

using std::move;
using std::vector;
using std::unique_ptr;
using std::ostream;
using std::endl;
typedef Patch::Hunk Hunk;

struct Patch::Node {
  Node *left;
  Node *right;

  Point old_distance_from_left_ancestor;
  Point new_distance_from_left_ancestor;
  Point old_extent;
  Point new_extent;

  unique_ptr<Text> old_text;
  unique_ptr<Text> new_text;

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
    return new Node{
        left,
        right,
        old_distance_from_left_ancestor,
        new_distance_from_left_ancestor,
        old_extent,
        new_extent,
        old_text ? unique_ptr<Text>(new Text(*old_text)) : nullptr,
        new_text ? unique_ptr<Text>(new Text(*new_text)) : nullptr,
    };
  }

  Node *invert() {
    return new Node{
        left,
        right,
        new_distance_from_left_ancestor,
        old_distance_from_left_ancestor,
        new_extent,
        old_extent,
        new_text ? unique_ptr<Text>(new Text(*new_text)) : nullptr,
        old_text ? unique_ptr<Text>(new Text(*old_text)) : nullptr,
    };
  }

  void write_dot_graph(std::stringstream &result, Point left_ancestor_old_end, Point left_ancestor_new_end) {
    Point node_old_start = left_ancestor_old_end.traverse(old_distance_from_left_ancestor);
    Point node_new_start = left_ancestor_new_end.traverse(new_distance_from_left_ancestor);
    Point node_old_end = node_old_start.traverse(old_extent);
    Point node_new_end = node_new_start.traverse(new_extent);

    result << "node_" << this << " ["
      << "label=\""
      << "new: " << node_new_start << " - " << node_new_end << ", " << endl
      << "old: " << node_old_start << " - " << node_old_end << ", " << endl
      << "\""
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
};

struct Patch::OldCoordinates {
  static Point distance_from_left_ancestor(const Node *node) {
    return node->old_distance_from_left_ancestor;
  }
  static Point extent(const Node *node) { return node->old_extent; }
  static Point start(const Hunk &hunk) { return hunk.old_start; }
  static Point end(const Hunk &hunk) { return hunk.old_end; }
};

struct Patch::NewCoordinates {
  static Point distance_from_left_ancestor(const Node *node) {
    return node->new_distance_from_left_ancestor;
  }
  static Point extent(const Node *node) { return node->new_extent; }
  static Point start(const Hunk &hunk) { return hunk.new_start; }
  static Point end(const Hunk &hunk) { return hunk.new_end; }
};

Patch::Patch()
    : root{nullptr}, frozen_node_array{nullptr}, merges_adjacent_hunks{true},
      hunk_count{0} {}

Patch::Patch(bool merges_adjacent_hunks)
    : root{nullptr}, frozen_node_array{nullptr},
      merges_adjacent_hunks{merges_adjacent_hunks}, hunk_count{0} {}

Patch::Patch(Patch &&other)
    : root{nullptr}, frozen_node_array{other.frozen_node_array},
      merges_adjacent_hunks{other.merges_adjacent_hunks},
      hunk_count{other.hunk_count} {
  std::swap(root, other.root);
  std::swap(left_ancestor_stack, other.left_ancestor_stack);
  std::swap(node_stack, other.node_stack);
}

Patch::Patch(Node *root, uint32_t hunk_count, bool merges_adjacent_hunks)
    : root{root}, frozen_node_array{nullptr},
      merges_adjacent_hunks{merges_adjacent_hunks}, hunk_count{hunk_count} {}

Patch::Patch(const vector<const Patch *> &patches_to_compose) : Patch() {
  bool left_to_right = true;
  for (const Patch *patch : patches_to_compose) {
    auto hunks = patch->get_hunks();

    if (left_to_right) {
      for (auto iter = hunks.begin(), end = hunks.end(); iter != end; ++iter) {
        splice(iter->new_start, iter->old_end.traversal(iter->old_start),
               iter->new_end.traversal(iter->new_start),
               iter->old_text ? unique_ptr<Text>{new Text(*iter->old_text)} : nullptr,
               iter->new_text ? unique_ptr<Text>{new Text(*iter->new_text)} : nullptr);
      }
    } else {
      for (auto iter = hunks.rbegin(), end = hunks.rend(); iter != end;
           ++iter) {
        splice(iter->old_start, iter->old_end.traversal(iter->old_start),
               iter->new_end.traversal(iter->new_start),
               iter->old_text ? unique_ptr<Text>{new Text(*iter->old_text)} : nullptr,
               iter->new_text ? unique_ptr<Text>{new Text(*iter->new_text)} : nullptr);
      }
    }

    left_to_right = !left_to_right;
  }
}

Patch::~Patch() {
  if (root) {
    if (frozen_node_array) {
      free(frozen_node_array);
    } else {
      delete_node(&root);
    }
  }
}

Patch::Node *Patch::build_node(Node *left, Node *right,
                       Point old_distance_from_left_ancestor,
                       Point new_distance_from_left_ancestor, Point old_extent,
                       Point new_extent, unique_ptr<Text> old_text,
                       unique_ptr<Text> new_text) {
  hunk_count++;
  return new Node{left,
                  right,
                  old_distance_from_left_ancestor,
                  new_distance_from_left_ancestor,
                  old_extent,
                  new_extent,
                  move(old_text),
                  move(new_text)};
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
      hunk_count--;
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

template <typename CoordinateSpace>
Patch::Node *Patch::splay_node_ending_after(Point splice_start, Point splice_end) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.traverse(
        CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.traverse(CoordinateSpace::extent(node));
    if (node_end >= splice_end && node_end > splice_start) {
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
Patch::Node *Patch::splay_node_starting_after(Point splice_start, Point splice_end) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.traverse(
        CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.traverse(CoordinateSpace::extent(node));
    if (node_start >= splice_end && node_start > splice_start) {
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
vector<Hunk> Patch::get_hunks_in_range(Point start, Point end, bool inclusive) {
  vector<Hunk> result;
  if (!root)
    return result;

  Node *lower_bound = splay_node_starting_before<CoordinateSpace>(start);

  node_stack.clear();
  left_ancestor_stack.clear();
  left_ancestor_stack.push_back({Point(), Point()});

  Node *node = root;
  if (!lower_bound) {
    while (node->left) {
      node_stack.push_back(node);
      node = node->left;
    }
  }

  while (node) {
    Patch::PositionStackEntry &left_ancestor_position =
        left_ancestor_stack.back();
    Point old_start = left_ancestor_position.old_end.traverse(
        node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_position.new_end.traverse(
        node->new_distance_from_left_ancestor);
    Point old_end = old_start.traverse(node->old_extent);
    Point new_end = new_start.traverse(node->new_extent);
    Text *old_text = node->old_text.get();
    Text *new_text = node->new_text.get();
    Hunk hunk = {old_start, old_end, new_start, new_end, old_text, new_text};

    if (inclusive) {
      if (CoordinateSpace::start(hunk) > end) {
        break;
      }

      if (CoordinateSpace::end(hunk) >= start) {
        result.push_back(hunk);
      }
    } else {
      if (CoordinateSpace::start(hunk) >= end) {
        break;
      }

      if (CoordinateSpace::end(hunk) > start) {
        result.push_back(hunk);
      }
    }

    if (node->right) {
      left_ancestor_stack.push_back(
          Patch::PositionStackEntry{old_end, new_end});
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
optional<Hunk> Patch::hunk_for_position(Point target) {
  Node *lower_bound = splay_node_starting_before<CoordinateSpace>(target);
  if (lower_bound) {
    Point old_start = lower_bound->old_distance_from_left_ancestor;
    Point new_start = lower_bound->new_distance_from_left_ancestor;
    Point old_end = old_start.traverse(lower_bound->old_extent);
    Point new_end = new_start.traverse(lower_bound->new_extent);
    Text *old_text = lower_bound->old_text.get();
    Text *new_text = lower_bound->new_text.get();
    return Hunk{old_start, old_end, new_start, new_end, old_text, new_text};
  } else {
    return optional<Hunk>{};
  }
}

bool Patch::splice(Point new_splice_start, Point new_deletion_extent,
                   Point new_insertion_extent, unique_ptr<Text> deleted_text,
                   unique_ptr<Text> inserted_text) {
  if (is_frozen()) {
    return false;
  }

  if (new_deletion_extent.is_zero() && new_insertion_extent.is_zero()) {
    return true;
  }

  if (!root) {
    root = build_node(nullptr, nullptr, new_splice_start, new_splice_start,
                     new_deletion_extent, new_insertion_extent,
                     move(deleted_text), move(inserted_text));
    return true;
  }

  Point new_deletion_end = new_splice_start.traverse(new_deletion_extent);
  Point new_insertion_end = new_splice_start.traverse(new_insertion_extent);

  Node *lower_bound = splay_node_starting_before<NewCoordinates>(new_splice_start);
  unique_ptr<Text> old_text =
      compute_old_text(move(deleted_text), new_splice_start, new_deletion_end);
  Node *upper_bound =
      splay_node_ending_after<NewCoordinates>(new_splice_start, new_deletion_end);
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
    Point lower_bound_old_end =
        lower_bound_old_start.traverse(lower_bound->old_extent);
    Point lower_bound_new_end =
        lower_bound_new_start.traverse(lower_bound->new_extent);
    Point upper_bound_old_end =
        upper_bound_old_start.traverse(upper_bound->old_extent);
    Point upper_bound_new_end =
        upper_bound_new_start.traverse(upper_bound->new_extent);

    bool overlaps_lower_bound, overlaps_upper_bound;
    if (merges_adjacent_hunks) {
      overlaps_lower_bound = new_splice_start <= lower_bound_new_end;
      overlaps_upper_bound = new_deletion_end >= upper_bound_new_start;
    } else {
      overlaps_lower_bound = new_splice_start < lower_bound_new_end &&
                             new_deletion_end > lower_bound_new_start;
      overlaps_upper_bound = new_splice_start < upper_bound_new_end &&
                             new_deletion_end > upper_bound_new_start;
    }

    if (overlaps_lower_bound && overlaps_upper_bound) {
      Point new_extent_prefix =
          new_splice_start.traversal(lower_bound_new_start);
      Point new_extent_suffix = upper_bound_new_end.traversal(new_deletion_end);

      upper_bound->old_extent =
          upper_bound_old_end.traversal(lower_bound_old_start);
      upper_bound->new_extent = new_extent_prefix.traverse(new_insertion_extent)
                                    .traverse(new_extent_suffix);
      upper_bound->old_distance_from_left_ancestor = lower_bound_old_start;
      upper_bound->new_distance_from_left_ancestor = lower_bound_new_start;

      if (inserted_text && lower_bound->new_text && upper_bound->new_text) {
        TextSlice new_text_prefix =
            TextSlice(*lower_bound->new_text).prefix(new_extent_prefix);
        TextSlice new_text_suffix = TextSlice(*upper_bound->new_text).suffix(
            new_deletion_end.traversal(upper_bound_new_start));
        upper_bound->new_text = unique_ptr<Text>{new Text(TextSlice::concat(
            new_text_prefix, TextSlice(*inserted_text), new_text_suffix))};
      } else {
        upper_bound->new_text = nullptr;
      }

      upper_bound->old_text = move(old_text);

      if (lower_bound == upper_bound) {
        if (root->old_extent.is_zero() && root->new_extent.is_zero()) {
          delete_root();
        }
      } else {
        upper_bound->left = lower_bound->left;
        lower_bound->left = nullptr;
        delete_node(&lower_bound);
      }

    } else if (overlaps_upper_bound) {
      Point old_splice_start = lower_bound_old_end.traverse(
          new_splice_start.traversal(lower_bound_new_end));
      Point new_extent_suffix = upper_bound_new_end.traversal(new_deletion_end);

      upper_bound->old_distance_from_left_ancestor = old_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent = upper_bound_old_end.traversal(old_splice_start);
      upper_bound->new_extent =
          new_insertion_extent.traverse(new_extent_suffix);

      if (inserted_text && upper_bound->new_text) {
        TextSlice new_text_suffix = TextSlice(*upper_bound->new_text).suffix(
            new_deletion_end.traversal(upper_bound_new_start));
        upper_bound->new_text =
            unique_ptr<Text>{new Text(TextSlice::concat(TextSlice(*inserted_text), new_text_suffix))};
      } else {
        upper_bound->new_text = nullptr;
      }

      upper_bound->old_text = move(old_text);

      delete_node(&lower_bound->right);
      if (upper_bound->left != lower_bound) {
        delete_node(&upper_bound->left);
      }

    } else if (overlaps_lower_bound) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      lower_bound->get_subtree_end(&rightmost_child_old_end,
                                 &rightmost_child_new_end);
      Point old_deletion_end = rightmost_child_old_end.traverse(
          new_deletion_end.traversal(rightmost_child_new_end));
      Point new_extent_prefix =
          new_splice_start.traversal(lower_bound_new_start);

      upper_bound->new_distance_from_left_ancestor = new_insertion_end.traverse(
          upper_bound_new_start.traversal(new_deletion_end));
      lower_bound->old_extent =
          old_deletion_end.traversal(lower_bound_old_start);
      lower_bound->new_extent =
          new_extent_prefix.traverse(new_insertion_extent);
      if (inserted_text && lower_bound->new_text) {
        TextSlice new_text_prefix =
            TextSlice(*lower_bound->new_text).prefix(new_extent_prefix);
        lower_bound->new_text =
            unique_ptr<Text>{new Text(TextSlice::concat(new_text_prefix, TextSlice(*inserted_text)))};
      } else {
        lower_bound->new_text = nullptr;
      }

      lower_bound->old_text = move(old_text);

      delete_node(&lower_bound->right);
      rotate_node_right(lower_bound, upper_bound, nullptr);

      // Splice doesn't overlap either bound
    } else {
      // If bounds are the same node, this is an insertion at the beginning of
      // that node with merges_adjacent_hunks set to false.
      if (lower_bound == upper_bound) {
        assert(!merges_adjacent_hunks);
        assert(new_deletion_extent.is_zero());
        assert(new_splice_start == upper_bound_new_start);

        root = build_node(upper_bound->left, upper_bound, upper_bound_old_start,
                         upper_bound_new_start, Point(),
                         new_insertion_extent, move(old_text),
                         move(inserted_text));

        upper_bound->left = nullptr;
        upper_bound->old_distance_from_left_ancestor = Point();
        upper_bound->new_distance_from_left_ancestor = Point();
      } else {
        Point rightmost_child_old_end, rightmost_child_new_end;
        lower_bound->get_subtree_end(&rightmost_child_old_end,
                                   &rightmost_child_new_end);
        Point old_splice_start = lower_bound_old_end.traverse(
            new_splice_start.traversal(lower_bound_new_end));
        Point old_deletion_end = rightmost_child_old_end.traverse(
            new_deletion_end.traversal(rightmost_child_new_end));

        root = build_node(
            lower_bound, upper_bound, old_splice_start, new_splice_start,
            old_deletion_end.traversal(old_splice_start), new_insertion_extent,
            move(old_text), move(inserted_text));

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
    lower_bound->get_subtree_end(&rightmost_child_old_end,
                               &rightmost_child_new_end);
    Point old_deletion_end = rightmost_child_old_end.traverse(
        new_deletion_end.traversal(rightmost_child_new_end));
    bool overlaps_lower_bound =
        new_splice_start < lower_bound_new_end ||
        (merges_adjacent_hunks && new_splice_start == lower_bound_new_end);

    delete_node(&lower_bound->right);

    if (overlaps_lower_bound) {
      lower_bound->old_extent =
          old_deletion_end.traversal(lower_bound_old_start);
      lower_bound->new_extent =
          new_insertion_end.traversal(lower_bound_new_start);
      if (inserted_text && lower_bound->new_text) {
        TextSlice new_text_prefix = TextSlice(*lower_bound->new_text).prefix(
            new_splice_start.traversal(lower_bound_new_start));
        lower_bound->new_text =
            unique_ptr<Text>{new Text(TextSlice::concat(new_text_prefix, TextSlice(*inserted_text)))};
      } else {
        lower_bound->new_text = nullptr;
      }

      lower_bound->old_text = move(old_text);
    } else {
      Point old_splice_start = lower_bound_old_end.traverse(
          new_splice_start.traversal(lower_bound_new_end));
      root =
          build_node(lower_bound, nullptr, old_splice_start, new_splice_start,
                    old_deletion_end.traversal(old_splice_start),
                    new_insertion_extent, move(old_text), move(inserted_text));
    }

  } else if (upper_bound) {
    Point upper_bound_new_start = upper_bound->new_distance_from_left_ancestor;
    Point upper_bound_old_start = upper_bound->old_distance_from_left_ancestor;
    Point upper_bound_new_end =
        upper_bound_new_start.traverse(upper_bound->new_extent);
    bool overlaps_upper_bound =
        new_deletion_end > upper_bound_new_start ||
        (merges_adjacent_hunks && new_deletion_end == upper_bound_new_start);

    Point old_deletion_end;
    if (upper_bound->left) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      upper_bound->left->get_subtree_end(&rightmost_child_old_end,
                                       &rightmost_child_new_end);
      old_deletion_end = rightmost_child_old_end.traverse(
          new_deletion_end.traversal(rightmost_child_new_end));
    } else {
      old_deletion_end = new_deletion_end;
    }

    delete_node(&upper_bound->left);
    if (overlaps_upper_bound) {
      upper_bound->old_distance_from_left_ancestor = new_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent =
          upper_bound_old_start.traversal(new_splice_start)
              .traverse(upper_bound->old_extent);
      upper_bound->new_extent = new_insertion_extent.traverse(
          upper_bound_new_end.traversal(new_deletion_end));

      if (inserted_text && upper_bound->new_text) {
        TextSlice new_text_suffix = TextSlice(*upper_bound->new_text).suffix(
            new_deletion_end.traversal(upper_bound_new_start));
        upper_bound->new_text =
            unique_ptr<Text>{new Text(TextSlice::concat(TextSlice(*inserted_text), new_text_suffix))};
      } else {
        upper_bound->new_text = nullptr;
      }

      upper_bound->old_text = move(old_text);
    } else {
      root =
          build_node(nullptr, upper_bound, new_splice_start, new_splice_start,
                    old_deletion_end.traversal(new_splice_start),
                    new_insertion_extent, move(old_text), move(inserted_text));
      Point distance_from_end_of_root_to_start_of_upper_bound =
          upper_bound_new_start.traversal(new_deletion_end);
      upper_bound->old_distance_from_left_ancestor =
          distance_from_end_of_root_to_start_of_upper_bound;
      upper_bound->new_distance_from_left_ancestor =
          distance_from_end_of_root_to_start_of_upper_bound;
    }

  } else {
    Point rightmost_child_old_end, rightmost_child_new_end;
    root->get_subtree_end(&rightmost_child_old_end, &rightmost_child_new_end);
    Point old_deletion_end = rightmost_child_old_end.traverse(
        new_deletion_end.traversal(rightmost_child_new_end));
    delete_node(&root);
    root = build_node(nullptr, nullptr, new_splice_start, new_splice_start,
                     old_deletion_end.traversal(new_splice_start),
                     new_insertion_extent, move(old_text), move(inserted_text));
  }

  return true;
}

bool Patch::splice_old(Point old_splice_start, Point old_deletion_extent,
                      Point old_insertion_extent) {
  if (is_frozen()) {
    return false;
  }

  if (!root) {
    return true;
  }

  Point old_deletion_end = old_splice_start.traverse(old_deletion_extent);
  Point old_insertion_end = old_splice_start.traverse(old_insertion_extent);

  Node *lower_bound = splay_node_ending_before<OldCoordinates>(old_splice_start);
  Node *upper_bound = splay_node_starting_after<OldCoordinates>(old_splice_start,
                                                             old_deletion_end);

  if (!lower_bound && !upper_bound) {
    delete_node(&root);
    return true;
  }

  if (upper_bound == lower_bound) {
    assert(upper_bound->old_extent.is_zero());
    assert(old_deletion_extent.is_zero());
    root->old_distance_from_left_ancestor =
        root->old_distance_from_left_ancestor.traverse(old_insertion_extent);
    root->new_distance_from_left_ancestor =
        root->new_distance_from_left_ancestor.traverse(old_insertion_extent);
    return true;
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
        upper_bound->new_extent =
            lower_bound->new_extent.traverse(upper_bound->new_extent);
        upper_bound->left = lower_bound->left;
        delete_node(&lower_bound);
      }
    } else {
      delete_node(&upper_bound->left);
    }
  }

  return true;
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

  return Patch{new_root, hunk_count, merges_adjacent_hunks};
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

  return Patch{inverted_root, hunk_count, merges_adjacent_hunks};
}

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
}

void Patch::delete_root() {
  Node *node = root, *parent = nullptr;
  while (true) {
    if (node->left) {
      rotate_node_right(node->left, node, parent);
    } else if (node->right) {
      rotate_node_left(node->right, node, parent);
    } else if (parent) {
      if (parent->left == node) {
        delete_node(&parent->left);
      } else if (parent->right == node) {
        delete_node(&parent->right);
      }
    } else {
      delete_node(&root);
      break;
    }
  }
}

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

size_t Patch::get_hunk_count() const { return hunk_count; }

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
  uint32_t n = hunk_count;
  uint32_t m = std::pow(2, std::floor(std::log2(n + 1))) - 1;
  perform_rebalancing_rotations(n - m);
  while (m > 1) {
    m = m / 2;
    perform_rebalancing_rotations(m);
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

vector<Hunk> Patch::get_hunks() const {
  vector<Hunk> result;
  if (!root) {
    return result;
  }

  Node *node = root;
  node_stack.clear();
  left_ancestor_stack.clear();
  left_ancestor_stack.push_back({Point(), Point()});

  while (node->left) {
    node_stack.push_back(node);
    node = node->left;
  }

  while (node) {
    PositionStackEntry &left_ancestor_position = left_ancestor_stack.back();
    Point old_start = left_ancestor_position.old_end.traverse(
        node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_position.new_end.traverse(
        node->new_distance_from_left_ancestor);
    Point old_end = old_start.traverse(node->old_extent);
    Point new_end = new_start.traverse(node->new_extent);
    Text *old_text = node->old_text.get();
    Text *new_text = node->new_text.get();
    result.push_back(
        Hunk{old_start, old_end, new_start, new_end, old_text, new_text});

    if (node->right) {
      left_ancestor_stack.push_back(PositionStackEntry{old_end, new_end});
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

vector<Hunk> Patch::get_hunks_in_old_range(Point start, Point end) {
  return get_hunks_in_range<OldCoordinates>(start, end);
}

vector<Hunk> Patch::get_hunks_in_new_range(Point start, Point end, bool inclusive) {
  return get_hunks_in_range<NewCoordinates>(start, end, inclusive);
}

optional<Hunk> Patch::hunk_for_old_position(Point target) {
  return hunk_for_position<OldCoordinates>(target);
}

optional<Hunk> Patch::hunk_for_new_position(Point target) {
  return hunk_for_position<NewCoordinates>(target);
}

unique_ptr<Text> Patch::compute_old_text(unique_ptr<Text> deleted_text,
                                       Point new_splice_start,
                                       Point new_deletion_end) {
  if (!deleted_text.get())
    return nullptr;

  unique_ptr<Text> result {new Text()};
  Point range_start = new_splice_start, range_end = new_deletion_end;

  auto overlapping_hunks =
      get_hunks_in_new_range(range_start, range_end, merges_adjacent_hunks);
  TextSlice deleted_text_slice = TextSlice(*deleted_text);
  Point deleted_text_slice_start = new_splice_start;

  for (const Hunk &hunk : overlapping_hunks) {
    if (!hunk.old_text)
      return nullptr;

    if (hunk.new_start > deleted_text_slice_start) {
      auto split_result = deleted_text_slice.split(
          hunk.new_start.traversal(deleted_text_slice_start));
      deleted_text_slice_start = hunk.new_start;
      deleted_text_slice = split_result.second;
      result->insert(result->end(), split_result.first.begin(), split_result.first.end());
    }

    result->insert(result->end(), hunk.old_text->begin(), hunk.old_text->end());
    deleted_text_slice = deleted_text_slice.suffix(
        hunk.new_end.traversal(deleted_text_slice_start));
    deleted_text_slice_start = hunk.new_end;
  }

  result->insert(result->end(), deleted_text_slice.begin(), deleted_text_slice.end());
  return result;
}

static const uint32_t SERIALIZATION_VERSION = 1;

enum Transition : uint32_t { None, Left, Right, Up };

template <typename T> T network_to_host(T input);

template <typename T> T host_to_network(T input);

template <> uint16_t network_to_host(uint16_t input) { return ntohs(input); }

template <> uint16_t host_to_network(uint16_t input) { return htons(input); }

template <> uint32_t network_to_host(uint32_t input) { return ntohl(input); }

template <> uint32_t host_to_network(uint32_t input) { return htonl(input); }

template <typename T> void append_to_buffer(vector<uint8_t> *output, T value) {
  value = host_to_network(value);
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
  output->insert(output->end(), bytes, bytes + sizeof(T));
}

template <typename T>
T get_from_buffer(const uint8_t **data, const uint8_t *end) {
  const T *pointer = reinterpret_cast<const T *>(*data);
  *data = *data + sizeof(T);
  if (*data <= end) {
    return network_to_host<T>(*pointer);
  } else {
    return 0;
  }
}

void get_point_from_buffer(const uint8_t **data, const uint8_t *end,
                        Point *point) {
  point->row = get_from_buffer<uint32_t>(data, end);
  point->column = get_from_buffer<uint32_t>(data, end);
}

void append_point_to_buffer(vector<uint8_t> *output, const Point &point) {
  append_to_buffer(output, point.row);
  append_to_buffer(output, point.column);
}

void append_text_to_buffer(vector<uint8_t> *output, const Text *text) {
  if (text) {
    append_to_buffer<uint32_t>(output, 1);
    append_to_buffer(output, static_cast<uint32_t>(text->size()));
    for (uint16_t character : *text) {
      append_to_buffer(output, character);
    }
  } else {
    append_to_buffer<uint32_t>(output, 0);
  }
}

unique_ptr<Text> get_text_from_buffer(const uint8_t **data, const uint8_t *end) {
  if (get_from_buffer<uint32_t>(data, end)) {
    uint32_t length = get_from_buffer<uint32_t>(data, end);
    unique_ptr<Text> result {new Text()};
    result->reserve(length);
    for (uint32_t i = 0; i < length; i++) {
      result->push_back(get_from_buffer<uint16_t>(data, end));
    }
    return result;
  } else {
    return nullptr;
  }
}

void get_node_from_buffer(const uint8_t **data, const uint8_t *end, Patch::Node *node) {
  get_point_from_buffer(data, end, &node->old_extent);
  get_point_from_buffer(data, end, &node->new_extent);
  get_point_from_buffer(data, end, &node->old_distance_from_left_ancestor);
  get_point_from_buffer(data, end, &node->new_distance_from_left_ancestor);
  node->old_text = get_text_from_buffer(data, end);
  node->new_text = get_text_from_buffer(data, end);
  node->left = nullptr;
  node->right = nullptr;
}

void append_node_to_buffer(vector<uint8_t> *output, const Patch::Node &node) {
  append_point_to_buffer(output, node.old_extent);
  append_point_to_buffer(output, node.new_extent);
  append_point_to_buffer(output, node.old_distance_from_left_ancestor);
  append_point_to_buffer(output, node.new_distance_from_left_ancestor);
  append_text_to_buffer(output, node.old_text.get());
  append_text_to_buffer(output, node.new_text.get());
}

void Patch::serialize(vector<uint8_t> *output) const {
  if (!root)
    return;

  append_to_buffer(output, SERIALIZATION_VERSION);

  append_to_buffer(output, hunk_count);

  append_node_to_buffer(output, *root);

  Node *node = root;
  node_stack.clear();
  int previous_node_child_index = -1;

  while (node) {
    if (node->left && previous_node_child_index < 0) {
      append_to_buffer<uint32_t>(output, Left);
      append_node_to_buffer(output, *node->left);
      node_stack.push_back(node);
      node = node->left;
      previous_node_child_index = -1;
    } else if (node->right && previous_node_child_index < 1) {
      append_to_buffer<uint32_t>(output, Right);
      append_node_to_buffer(output, *node->right);
      node_stack.push_back(node);
      node = node->right;
      previous_node_child_index = -1;
    } else if (!node_stack.empty()) {
      append_to_buffer<uint32_t>(output, Up);
      Node *parent = node_stack.back();
      node_stack.pop_back();
      previous_node_child_index = (node == parent->left) ? 0 : 1;
      node = parent;
    } else {
      break;
    }
  }
}

bool Patch::is_frozen() const { return frozen_node_array != nullptr; }

Patch::Patch(const vector<uint8_t> &input)
    : root{nullptr}, frozen_node_array{nullptr}, merges_adjacent_hunks{true} {
  const uint8_t *begin = input.data();
  const uint8_t *data = begin;
  const uint8_t *end = data + input.size();

  uint32_t serialization_version = get_from_buffer<uint32_t>(&data, end);
  if (serialization_version != SERIALIZATION_VERSION) {
    return;
  }

  hunk_count = get_from_buffer<uint32_t>(&data, end);
  if (hunk_count == 0) {
    return;
  }

  node_stack.reserve(hunk_count);
  frozen_node_array = static_cast<Node *>(calloc(hunk_count, sizeof(Node)));
  root = frozen_node_array;
  Node *node = root, *next_node = root + 1;
  get_node_from_buffer(&data, end, node);

  while (next_node < root + hunk_count) {
    switch (get_from_buffer<uint32_t>(&data, end)) {
    case Left:
      get_node_from_buffer(&data, end, next_node);
      node->left = next_node;
      node_stack.push_back(node);
      node = next_node;
      next_node++;
      break;
    case Right:
      get_node_from_buffer(&data, end, next_node);
      node->right = next_node;
      node_stack.push_back(node);
      node = next_node;
      next_node++;
      break;
    case Up:
      node = node_stack.back();
      node_stack.pop_back();
      break;
    default:
      delete[] root;
      return;
    }
  }
}

ostream &operator<<(ostream &stream, const Patch::Hunk &hunk) {
  return stream <<
    "{Hunk old-range: (" << hunk.old_start << " - " << hunk.old_end << ")" <<
    ", new-range: (" << hunk.new_start << " - " << hunk.new_end << ")}";
}
