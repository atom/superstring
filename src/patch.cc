#include <assert.h>
#include <stdio.h>
#include <cmath>
#include <vector>
#include <memory>
#include "patch.h"
#include "text.h"

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

using std::move;
using std::vector;
using std::unique_ptr;
using Nan::Maybe;

struct Node {
  Node *left;
  Node *right;

  Point old_distance_from_left_ancestor;
  Point new_distance_from_left_ancestor;
  Point old_extent;
  Point new_extent;

  unique_ptr<Text> old_text;
  unique_ptr<Text> new_text;

  void GetSubtreeEnd(Point *old_end, Point *new_end) {
    Node *node = this;
    *old_end = Point::Zero();
    *new_end = Point::Zero();
    while (node) {
      *old_end = old_end->Traverse(node->old_distance_from_left_ancestor).Traverse(node->old_extent);
      *new_end = new_end->Traverse(node->new_distance_from_left_ancestor).Traverse(node->new_extent);
      node = node->right;
    }
  }
};

struct Patch::PositionStackEntry {
  Point old_end;
  Point new_end;
};

struct OldCoordinates {
  static Point distance_from_left_ancestor(const Node *node) { return node->old_distance_from_left_ancestor; }
  static Point extent(const Node *node) { return node->old_extent; }
  static Point start(const Hunk &hunk) { return hunk.old_start; }
  static Point end(const Hunk &hunk) { return hunk.old_end; }
};

struct NewCoordinates {
  static Point distance_from_left_ancestor(const Node *node) { return node->new_distance_from_left_ancestor; }
  static Point extent(const Node *node) { return node->new_extent; }
  static Point start(const Hunk &hunk) { return hunk.new_start; }
  static Point end(const Hunk &hunk) { return hunk.new_end; }
};

Patch::Patch() : root{nullptr}, is_frozen{false}, merges_adjacent_hunks{true}, hunk_count{0} {}
Patch::Patch(bool merges_adjacent_hunks) :
  root{nullptr}, is_frozen{false},
  merges_adjacent_hunks{merges_adjacent_hunks}, hunk_count{0} {}

Patch::Patch(Patch &&other) : root{nullptr}, is_frozen{other.is_frozen},
                              merges_adjacent_hunks{other.merges_adjacent_hunks},
                              hunk_count{other.hunk_count} {
  std::swap(root, other.root);
  std::swap(left_ancestor_stack, other.left_ancestor_stack);
  std::swap(node_stack, other.node_stack);
}

Patch::Patch(const vector<const Patch *> &patches_to_compose) : Patch() {
  bool left_to_right = true;
  for (const Patch *patch : patches_to_compose) {
    auto hunks = patch->GetHunks();

    if (left_to_right) {
      for (auto iter = hunks.begin(), end = hunks.end(); iter != end; ++iter) {
        Splice(
          iter->new_start,
          iter->old_end.Traversal(iter->old_start),
          iter->new_end.Traversal(iter->new_start),
          iter->old_text ? Text::Build(iter->old_text->content) : nullptr,
          iter->new_text ? Text::Build(iter->new_text->content) : nullptr
        );
      }
    } else {
      for (auto iter = hunks.rbegin(), end = hunks.rend(); iter != end; ++iter) {
        Splice(
          iter->old_start,
          iter->old_end.Traversal(iter->old_start),
          iter->new_end.Traversal(iter->new_start),
          iter->old_text ? Text::Build(iter->old_text->content) : nullptr,
          iter->new_text ? Text::Build(iter->new_text->content) : nullptr
        );
      }
    }

    left_to_right = !left_to_right;
  }
}

Patch::~Patch() {
  if (root) {
    if (is_frozen) {
      free(root);
    } else {
      DeleteNode(&root);
    }
  }
}

Node *Patch::BuildNode(Node *left, Node *right, Point old_distance_from_left_ancestor,
                       Point new_distance_from_left_ancestor, Point old_extent,
                       Point new_extent, unique_ptr<Text> old_text,
                       unique_ptr<Text> new_text) {
  hunk_count++;
  return new Node{
    left,
    right,
    old_distance_from_left_ancestor,
    new_distance_from_left_ancestor,
    old_extent,
    new_extent,
    move(old_text),
    move(new_text)
  };
}

void Patch::DeleteNode(Node **node_to_delete) {
  if (*node_to_delete) {
    node_stack.clear();
    node_stack.push_back(*node_to_delete);

    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left) node_stack.push_back(node->left);
      if (node->right) node_stack.push_back(node->right);
      delete node;
      hunk_count--;
    }

    *node_to_delete = nullptr;
  }
}

template<typename CoordinateSpace>
Node *Patch::SplayNodeEndingBefore(Point target) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point::Zero();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.Traverse(CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.Traverse(CoordinateSpace::extent(node));
    if (node_end <= target) {
      splayed_node = node;
      splayed_node_ancestor_count = node_stack.size();
      if (node->right) {
        left_ancestor_end = node_start.Traverse(CoordinateSpace::extent(node));
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
    SplayNode(splayed_node);
  }

  return splayed_node;
}

template<typename CoordinateSpace>
Node *Patch::SplayNodeStartingBefore(Point target) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point::Zero();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.Traverse(CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.Traverse(CoordinateSpace::extent(node));
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
    SplayNode(splayed_node);
  }

  return splayed_node;
}

template<typename CoordinateSpace>
Node *Patch::SplayNodeEndingAfter(Point splice_start, Point splice_end) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point::Zero();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.Traverse(CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.Traverse(CoordinateSpace::extent(node));
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
    SplayNode(splayed_node);
  }

  return splayed_node;
}

template<typename CoordinateSpace>
Node *Patch::SplayNodeStartingAfter(Point splice_start, Point splice_end) {
  Node *splayed_node = nullptr;
  Point left_ancestor_end = Point::Zero();
  Node *node = root;

  node_stack.clear();
  size_t splayed_node_ancestor_count = 0;
  while (node) {
    Point node_start = left_ancestor_end.Traverse(CoordinateSpace::distance_from_left_ancestor(node));
    Point node_end = node_start.Traverse(CoordinateSpace::extent(node));
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
    SplayNode(splayed_node);
  }

  return splayed_node;
}

template<typename CoordinateSpace>
vector<Hunk> Patch::GetHunksInRange(Point start, Point end, bool inclusive) {
  vector<Hunk> result;
  if (!root) return result;

  Node *lower_bound = SplayNodeStartingBefore<CoordinateSpace>(start);

  node_stack.clear();
  left_ancestor_stack.clear();
  left_ancestor_stack.push_back({Point::Zero(), Point::Zero()});

  Node *node = root;
  if (!lower_bound) {
    while (node->left) {
      node_stack.push_back(node);
      node = node->left;
    }
  }

  while (node) {
    Patch::PositionStackEntry &left_ancestor_position = left_ancestor_stack.back();
    Point old_start = left_ancestor_position.old_end.Traverse(node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_position.new_end.Traverse(node->new_distance_from_left_ancestor);
    Point old_end = old_start.Traverse(node->old_extent);
    Point new_end = new_start.Traverse(node->new_extent);
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
      left_ancestor_stack.push_back(Patch::PositionStackEntry{old_end, new_end});
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

template<typename CoordinateSpace>
Nan::Maybe<Hunk> Patch::HunkForPosition(Point target) {
  Node *lower_bound = SplayNodeStartingBefore<CoordinateSpace>(target);
  if (lower_bound) {
    Point old_start = lower_bound->old_distance_from_left_ancestor;
    Point new_start = lower_bound->new_distance_from_left_ancestor;
    Point old_end = old_start.Traverse(lower_bound->old_extent);
    Point new_end = new_start.Traverse(lower_bound->new_extent);
    Text *old_text = lower_bound->old_text.get();
    Text *new_text = lower_bound->new_text.get();
    return Nan::Just(Hunk{old_start, old_end, new_start, new_end, old_text, new_text});
  } else {
    return Nan::Nothing<Hunk>();
  }
}

bool Patch::Splice(Point new_splice_start, Point new_deletion_extent,
                   Point new_insertion_extent, unique_ptr<Text> deleted_text,
                   unique_ptr<Text> inserted_text) {
  if (is_frozen) {
    return false;
  }

  if (new_deletion_extent.IsZero() && new_insertion_extent.IsZero()) {
    return true;
  }

  if (!root) {
    root = BuildNode(
      nullptr,
      nullptr,
      new_splice_start,
      new_splice_start,
      new_deletion_extent,
      new_insertion_extent,
      move(deleted_text),
      move(inserted_text)
    );
    return true;
  }

  Point new_deletion_end = new_splice_start.Traverse(new_deletion_extent);
  Point new_insertion_end = new_splice_start.Traverse(new_insertion_extent);

  Node *lower_bound = SplayNodeStartingBefore<NewCoordinates>(new_splice_start);
  unique_ptr<Text> old_text = ComputeOldText(move(deleted_text), new_splice_start, new_deletion_end);
  Node *upper_bound = SplayNodeEndingAfter<NewCoordinates>(new_splice_start, new_deletion_end);
  if (upper_bound && lower_bound && lower_bound != upper_bound) {
    if (lower_bound != upper_bound->left) {
      RotateNodeRight(lower_bound, upper_bound->left, upper_bound);
    }
  }

  if (lower_bound && upper_bound) {
    Point lower_bound_old_start = lower_bound->old_distance_from_left_ancestor;
    Point lower_bound_new_start = lower_bound->new_distance_from_left_ancestor;
    Point upper_bound_old_start = upper_bound->old_distance_from_left_ancestor;
    Point upper_bound_new_start = upper_bound->new_distance_from_left_ancestor;
    Point lower_bound_old_end = lower_bound_old_start.Traverse(lower_bound->old_extent);
    Point lower_bound_new_end = lower_bound_new_start.Traverse(lower_bound->new_extent);
    Point upper_bound_old_end = upper_bound_old_start.Traverse(upper_bound->old_extent);
    Point upper_bound_new_end = upper_bound_new_start.Traverse(upper_bound->new_extent);

    bool overlaps_lower_bound, overlaps_upper_bound;
    if (merges_adjacent_hunks) {
      overlaps_lower_bound = new_splice_start <= lower_bound_new_end;
      overlaps_upper_bound = new_deletion_end >= upper_bound_new_start ;
    } else {
      overlaps_lower_bound = new_splice_start < lower_bound_new_end && new_deletion_end > lower_bound_new_start;
      overlaps_upper_bound = new_splice_start < upper_bound_new_end && new_deletion_end > upper_bound_new_start;
    }

    if (overlaps_lower_bound && overlaps_upper_bound) {
      Point new_extent_prefix = new_splice_start.Traversal(lower_bound_new_start);
      Point new_extent_suffix = upper_bound_new_end.Traversal(new_deletion_end);

      upper_bound->old_extent = upper_bound_old_end.Traversal(lower_bound_old_start);
      upper_bound->new_extent = new_extent_prefix.Traverse(new_insertion_extent).Traverse(new_extent_suffix);
      upper_bound->old_distance_from_left_ancestor = lower_bound_old_start;
      upper_bound->new_distance_from_left_ancestor = lower_bound_new_start;

      if (inserted_text && lower_bound->new_text && upper_bound->new_text) {
        TextSlice new_text_prefix = lower_bound->new_text->Prefix(new_extent_prefix);
        TextSlice new_text_suffix = upper_bound->new_text->Suffix(new_deletion_end.Traversal(upper_bound_new_start));
        upper_bound->new_text = Text::Concat(new_text_prefix, inserted_text->AsSlice(), new_text_suffix);
      } else {
        upper_bound->new_text = nullptr;
      }

      upper_bound->old_text = move(old_text);

      if (lower_bound == upper_bound) {
        if (root->old_extent.IsZero() && root->new_extent.IsZero()) {
          DeleteRoot();
        }
      } else {
        upper_bound->left = lower_bound->left;
        lower_bound->left = nullptr;
        DeleteNode(&lower_bound);
      }

    } else if (overlaps_upper_bound) {
      Point old_splice_start = lower_bound_old_end.Traverse(new_splice_start.Traversal(lower_bound_new_end));
      Point new_extent_suffix = upper_bound_new_end.Traversal(new_deletion_end);

      upper_bound->old_distance_from_left_ancestor = old_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent = upper_bound_old_end.Traversal(old_splice_start);
      upper_bound->new_extent = new_insertion_extent.Traverse(new_extent_suffix);

      if (inserted_text && upper_bound->new_text) {
        TextSlice new_text_suffix = upper_bound->new_text->Suffix(new_deletion_end.Traversal(upper_bound_new_start));
        upper_bound->new_text = Text::Concat(inserted_text->AsSlice(), new_text_suffix);
      } else {
        upper_bound->new_text = nullptr;
      }

      upper_bound->old_text = move(old_text);

      DeleteNode(&lower_bound->right);
      if (upper_bound->left != lower_bound) {
        DeleteNode(&upper_bound->left);
      }

    } else if (overlaps_lower_bound) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      lower_bound->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
      Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));
      Point new_extent_prefix = new_splice_start.Traversal(lower_bound_new_start);

      upper_bound->new_distance_from_left_ancestor = new_insertion_end.Traverse(upper_bound_new_start.Traversal(new_deletion_end));
      lower_bound->old_extent = old_deletion_end.Traversal(lower_bound_old_start);
      lower_bound->new_extent = new_extent_prefix.Traverse(new_insertion_extent);
      if (inserted_text && lower_bound->new_text) {
        TextSlice new_text_prefix = lower_bound->new_text->Prefix(new_extent_prefix);
        lower_bound->new_text = Text::Concat(new_text_prefix, inserted_text->AsSlice());
      } else {
        lower_bound->new_text = nullptr;
      }

      lower_bound->old_text = move(old_text);

      DeleteNode(&lower_bound->right);
      RotateNodeRight(lower_bound, upper_bound, nullptr);

    // Splice doesn't overlap either bound
    } else {
      // If bounds are the same node, this is an insertion at the beginning of
      // that node with merges_adjacent_hunks set to false.
      if (lower_bound == upper_bound) {
        assert(!merges_adjacent_hunks);
        assert(new_deletion_extent.IsZero());
        assert(new_splice_start == upper_bound_new_start);

        root = BuildNode(
          upper_bound->left,
          upper_bound,
          upper_bound_old_start,
          upper_bound_new_start,
          Point::Zero(),
          new_insertion_extent,
          move(old_text),
          move(inserted_text)
        );

        upper_bound->left = nullptr;
        upper_bound->old_distance_from_left_ancestor = Point::Zero();
        upper_bound->new_distance_from_left_ancestor = Point::Zero();
      } else {
        Point rightmost_child_old_end, rightmost_child_new_end;
        lower_bound->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
        Point old_splice_start = lower_bound_old_end.Traverse(new_splice_start.Traversal(lower_bound_new_end));
        Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));

        root = BuildNode(
          lower_bound,
          upper_bound,
          old_splice_start,
          new_splice_start,
          old_deletion_end.Traversal(old_splice_start),
          new_insertion_extent,
          move(old_text),
          move(inserted_text)
        );

        DeleteNode(&lower_bound->right);
        upper_bound->left = nullptr;
        upper_bound->old_distance_from_left_ancestor = upper_bound_old_start.Traversal(old_deletion_end);
        upper_bound->new_distance_from_left_ancestor = upper_bound_new_start.Traversal(new_deletion_end);
      }
    }

  } else if (lower_bound) {
    Point lower_bound_old_start = lower_bound->old_distance_from_left_ancestor;
    Point lower_bound_new_start = lower_bound->new_distance_from_left_ancestor;
    Point lower_bound_new_end = lower_bound_new_start.Traverse(lower_bound->new_extent);
    Point lower_bound_old_end = lower_bound_old_start.Traverse(lower_bound->old_extent);
    Point rightmost_child_old_end, rightmost_child_new_end;
    lower_bound->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
    Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));
    bool overlaps_lower_bound =
      new_splice_start < lower_bound_new_end ||
        (merges_adjacent_hunks && new_splice_start == lower_bound_new_end);

    DeleteNode(&lower_bound->right);

    if (overlaps_lower_bound) {
      lower_bound->old_extent = old_deletion_end.Traversal(lower_bound_old_start);
      lower_bound->new_extent = new_insertion_end.Traversal(lower_bound_new_start);
      if (inserted_text && lower_bound->new_text) {
        TextSlice new_text_prefix = lower_bound->new_text->Prefix(new_splice_start.Traversal(lower_bound_new_start));
        lower_bound->new_text = Text::Concat(new_text_prefix, inserted_text->AsSlice());
      } else {
        lower_bound->new_text = nullptr;
      }

      lower_bound->old_text = move(old_text);
    } else {
      Point old_splice_start = lower_bound_old_end.Traverse(new_splice_start.Traversal(lower_bound_new_end));
      root = BuildNode(
        lower_bound,
        nullptr,
        old_splice_start,
        new_splice_start,
        old_deletion_end.Traversal(old_splice_start),
        new_insertion_extent,
        move(old_text),
        move(inserted_text)
      );
    }

  } else if (upper_bound) {
    Point upper_bound_new_start = upper_bound->new_distance_from_left_ancestor;
    Point upper_bound_old_start = upper_bound->old_distance_from_left_ancestor;
    Point upper_bound_new_end = upper_bound_new_start.Traverse(upper_bound->new_extent);
    bool overlaps_upper_bound =
      new_deletion_end > upper_bound_new_start ||
        (merges_adjacent_hunks && new_deletion_end == upper_bound_new_start);

    Point old_deletion_end;
    if (upper_bound->left) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      upper_bound->left->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
      old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));
    } else {
      old_deletion_end = new_deletion_end;
    }

    DeleteNode(&upper_bound->left);
    if (overlaps_upper_bound) {
      upper_bound->old_distance_from_left_ancestor = new_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent = upper_bound_old_start.Traversal(new_splice_start).Traverse(upper_bound->old_extent);
      upper_bound->new_extent = new_insertion_extent.Traverse(upper_bound_new_end.Traversal(new_deletion_end));

      if (inserted_text && upper_bound->new_text) {
        TextSlice new_text_suffix = upper_bound->new_text->Suffix(new_deletion_end.Traversal(upper_bound_new_start));
        upper_bound->new_text = Text::Concat(inserted_text->AsSlice(), new_text_suffix);
      } else {
        upper_bound->new_text = nullptr;
      }

      upper_bound->old_text = move(old_text);
    } else {
      root = BuildNode(
        nullptr,
        upper_bound,
        new_splice_start,
        new_splice_start,
        old_deletion_end.Traversal(new_splice_start),
        new_insertion_extent,
        move(old_text),
        move(inserted_text)
      );
      Point distance_from_end_of_root_to_start_of_upper_bound = upper_bound_new_start.Traversal(new_deletion_end);
      upper_bound->old_distance_from_left_ancestor = distance_from_end_of_root_to_start_of_upper_bound;
      upper_bound->new_distance_from_left_ancestor = distance_from_end_of_root_to_start_of_upper_bound;
    }

  } else {
    Point rightmost_child_old_end, rightmost_child_new_end;
    root->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
    Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));
    DeleteNode(&root);
    root = BuildNode(
      nullptr,
      nullptr,
      new_splice_start,
      new_splice_start,
      old_deletion_end.Traversal(new_splice_start),
      new_insertion_extent,
      move(old_text),
      move(inserted_text)
    );
  }

  return true;
}

bool Patch::SpliceOld(Point old_splice_start, Point old_deletion_extent, Point old_insertion_extent) {
  if (is_frozen) {
    return false;
  }

  if (!root) {
    return true;
  }

  Point old_deletion_end = old_splice_start.Traverse(old_deletion_extent);
  Point old_insertion_end = old_splice_start.Traverse(old_insertion_extent);

  Node *lower_bound = SplayNodeEndingBefore<OldCoordinates>(old_splice_start);
  Node *upper_bound = SplayNodeStartingAfter<OldCoordinates>(old_splice_start, old_deletion_end);

  // fprintf(stderr, "graph message { label=\"splayed upper and lower bounds\" }\n");
  // PrintDotGraph();

  if (!lower_bound && !upper_bound) {
    DeleteNode(&root);
    return true;
  }

  if (upper_bound == lower_bound) {
    assert(upper_bound->old_extent.IsZero());
    assert(old_deletion_extent.IsZero());
    root->old_distance_from_left_ancestor = root->old_distance_from_left_ancestor.Traverse(old_insertion_extent);
    root->new_distance_from_left_ancestor = root->new_distance_from_left_ancestor.Traverse(old_insertion_extent);
    return true;
  }

  if (upper_bound && lower_bound) {
    if (lower_bound != upper_bound->left) {
      RotateNodeRight(lower_bound, upper_bound->left, upper_bound);
    }
  }

  Point new_deletion_end, new_insertion_end;

  if (lower_bound) {
    Point lower_bound_old_start = lower_bound->old_distance_from_left_ancestor;
    Point lower_bound_new_start = lower_bound->new_distance_from_left_ancestor;
    Point lower_bound_old_end = lower_bound_old_start.Traverse(lower_bound->old_extent);
    Point lower_bound_new_end = lower_bound_new_start.Traverse(lower_bound->new_extent);
    new_deletion_end = lower_bound_new_end.Traverse(old_deletion_end.Traversal(lower_bound_old_end));
    new_insertion_end = lower_bound_new_end.Traverse(old_insertion_end.Traversal(lower_bound_old_end));

    DeleteNode(&lower_bound->right);
  } else {
    new_deletion_end = old_deletion_end;
    new_insertion_end = old_insertion_end;
  }

  if (upper_bound) {
    Point distance_between_splice_and_upper_bound = upper_bound->old_distance_from_left_ancestor.Traversal(old_deletion_end);
    upper_bound->old_distance_from_left_ancestor = old_insertion_end.Traverse(distance_between_splice_and_upper_bound);
    upper_bound->new_distance_from_left_ancestor = new_insertion_end.Traverse(distance_between_splice_and_upper_bound);

    if (lower_bound) {
      if (lower_bound->old_distance_from_left_ancestor.Traverse(lower_bound->old_extent) == upper_bound->old_distance_from_left_ancestor) {
        upper_bound->old_distance_from_left_ancestor = lower_bound->old_distance_from_left_ancestor;
        upper_bound->new_distance_from_left_ancestor = lower_bound->new_distance_from_left_ancestor;
        upper_bound->old_extent = lower_bound->old_extent.Traverse(upper_bound->old_extent);
        upper_bound->new_extent = lower_bound->new_extent.Traverse(upper_bound->new_extent);
        upper_bound->left = lower_bound->left;
        DeleteNode(&lower_bound);
      }
    } else {
      DeleteNode(&upper_bound->left);
    }
  }

  return true;
}

void Patch::SplayNode(Node *node) {
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
        RotateNodeLeft(node, parent, grandparent);
        RotateNodeRight(node, grandparent, great_grandparent);
      } else if (grandparent->right == parent && parent->left == node) {
        RotateNodeRight(node, parent, grandparent);
        RotateNodeLeft(node, grandparent, great_grandparent);
      } else if (grandparent->left == parent && parent->left == node) {
        RotateNodeRight(parent, grandparent, great_grandparent);
        RotateNodeRight(node, parent, great_grandparent);
      } else if (grandparent->right == parent && parent->right == node) {
        RotateNodeLeft(parent, grandparent, great_grandparent);
        RotateNodeLeft(node, parent, great_grandparent);
      }
    } else {
      if (parent->left == node) {
        RotateNodeRight(node, parent, nullptr);
      } else if (parent->right == node) {
        RotateNodeLeft(node, parent, nullptr);
      } else {
        assert(!"Unexpected state");
      }
    }
  }
}

void Patch::RotateNodeLeft(Node *pivot, Node *root, Node *root_parent) {
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

  pivot->old_distance_from_left_ancestor = root->old_distance_from_left_ancestor
    .Traverse(root->old_extent)
    .Traverse(pivot->old_distance_from_left_ancestor);
  pivot->new_distance_from_left_ancestor = root->new_distance_from_left_ancestor
    .Traverse(root->new_extent)
    .Traverse(pivot->new_distance_from_left_ancestor);
}

void Patch::RotateNodeRight(Node *pivot, Node *root, Node *root_parent) {
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

  root->old_distance_from_left_ancestor = root->old_distance_from_left_ancestor
    .Traversal(pivot->old_distance_from_left_ancestor.Traverse(pivot->old_extent));
  root->new_distance_from_left_ancestor = root->new_distance_from_left_ancestor
    .Traversal(pivot->new_distance_from_left_ancestor.Traverse(pivot->new_extent));
}

void Patch::DeleteRoot() {
  Node *node = root, *parent = nullptr;
  while (true) {
    if (node->left) {
      RotateNodeRight(node->left, node, parent);
    } else if (node->right) {
      RotateNodeLeft(node->right, node, parent);
    } else if (parent) {
      if (parent->left == node) {
        DeleteNode(&parent->left);
      } else if (parent->right == node) {
        DeleteNode(&parent->right);
      }
    } else {
      DeleteNode(&root);
      break;
    }
  }
}

static void PrintDotGraphForNode(Node *node, Point left_ancestor_old_end, Point left_ancestor_new_end) {
  Point node_old_start = left_ancestor_old_end.Traverse(node->old_distance_from_left_ancestor);
  Point node_new_start = left_ancestor_new_end.Traverse(node->new_distance_from_left_ancestor);
  Point node_old_end = node_old_start.Traverse(node->old_extent);
  Point node_new_end = node_new_start.Traverse(node->new_extent);

  fprintf(
    stderr,
    "node_%p [label=\"new: (%u, %u) - (%u, %u)\nold: (%u, %u) - (%u, %u)\n\", tooltip=\"%p\"]\n",
    node,
    node_new_start.row,
    node_new_start.column,
    node_new_end.row,
    node_new_end.column,
    node_old_start.row,
    node_old_start.column,
    node_old_end.row,
    node_old_end.column,
    node
  );

  fprintf(stderr, "node_%p -> ", node);
  if (node->left) {
    fprintf(stderr, "node_%p\n", node->left);
    PrintDotGraphForNode(node->left, left_ancestor_old_end, left_ancestor_new_end);
  } else {
    fprintf(stderr, "node_%p_left_null\n", node);
    fprintf(stderr, "node_%p_left_null [label=\"\" shape=point]\n", node);
  }

  fprintf(stderr, "node_%p -> ", node);
  if (node->right) {
    fprintf(stderr, "node_%p\n", node->right);
    PrintDotGraphForNode(node->right, node_old_end, node_new_end);
  } else {
    fprintf(stderr, "node_%p_right_null\n", node);
    fprintf(stderr, "node_%p_right_null [label=\"\" shape=point]\n", node);
  }
}

void Patch::PrintDotGraph() const {
  fprintf(stderr, "digraph patch {\n");
  if (root) {
    PrintDotGraphForNode(root, Point::Zero(), Point::Zero());
  }
  fprintf(stderr, "}\n");
}

size_t Patch::GetHunkCount() const {
  return hunk_count;
}

void Patch::Rebalance() {
  if (!root) return;

  // Transform tree to vine
  Node *pseudo_root = root, *pseudo_root_parent = nullptr;
  while (pseudo_root) {
    Node *left = pseudo_root->left;
    Node *right = pseudo_root->right;
    if (left) {
      RotateNodeRight(left, pseudo_root, pseudo_root_parent);
      pseudo_root = left;
    } else {
      pseudo_root_parent = pseudo_root;
      pseudo_root = right;
    }
  }

  // Transform vine to balanced tree
  uint32_t n = hunk_count;
  uint32_t m = std::pow(2, std::floor(std::log2(n + 1))) - 1;
  PerformRebalancingRotations(n - m);
  while (m > 1) {
    m = m / 2;
    PerformRebalancingRotations(m);
  }
}

void Patch::PerformRebalancingRotations(uint32_t count) {
  Node *pseudo_root = root, *pseudo_root_parent = nullptr;
  for (uint32_t i = 0; i < count; i++) {
    if (!pseudo_root) return;
    Node *right_child = pseudo_root->right;
    if (!right_child) return;
    RotateNodeLeft(right_child, pseudo_root, pseudo_root_parent);
    pseudo_root = right_child->right;
    pseudo_root_parent = right_child;
  }
}

vector<Hunk> Patch::GetHunks() const {
  vector<Hunk> result;
  if (!root) {
    return result;
  }

  Node *node = root;
  node_stack.clear();
  left_ancestor_stack.clear();
  left_ancestor_stack.push_back({Point::Zero(), Point::Zero()});

  while (node->left) {
    node_stack.push_back(node);
    node = node->left;
  }

  while (node) {
    PositionStackEntry &left_ancestor_position = left_ancestor_stack.back();
    Point old_start = left_ancestor_position.old_end.Traverse(node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_position.new_end.Traverse(node->new_distance_from_left_ancestor);
    Point old_end = old_start.Traverse(node->old_extent);
    Point new_end = new_start.Traverse(node->new_extent);
    Text *old_text = node->old_text.get();
    Text *new_text = node->new_text.get();
    result.push_back(Hunk{old_start, old_end, new_start, new_end, old_text, new_text});

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

vector<Hunk> Patch::GetHunksInOldRange(Point start, Point end) {
  return GetHunksInRange<OldCoordinates>(start, end);
}

vector<Hunk> Patch::GetHunksInNewRange(Point start, Point end, bool inclusive) {
  return GetHunksInRange<NewCoordinates>(start, end, inclusive);
}

Maybe<Hunk> Patch::HunkForOldPosition(Point target) {
  return HunkForPosition<OldCoordinates>(target);
}

Maybe<Hunk> Patch::HunkForNewPosition(Point target) {
  return HunkForPosition<NewCoordinates>(target);
}

unique_ptr<Text> Patch::ComputeOldText(unique_ptr<Text> deleted_text, Point new_splice_start, Point new_deletion_end) {
  if (!deleted_text.get()) return nullptr;

  vector<uint16_t> content;
  auto result = Text::Build(content);
  Point range_start = new_splice_start, range_end = new_deletion_end;

  auto overlapping_hunks = GetHunksInNewRange(range_start, range_end, merges_adjacent_hunks);
  TextSlice deleted_text_slice = deleted_text->AsSlice();
  Point deleted_text_slice_start = new_splice_start;

  for (const Hunk &hunk : overlapping_hunks) {
    if (!hunk.old_text) return nullptr;

    if (hunk.new_start > deleted_text_slice_start) {
      auto split_result = deleted_text_slice.Split(hunk.new_start.Traversal(deleted_text_slice_start));
      deleted_text_slice_start = hunk.new_start;
      deleted_text_slice = split_result.second;
      result->Append(split_result.first);
    }

    result->Append(hunk.old_text->AsSlice());
    deleted_text_slice = deleted_text_slice.Suffix(hunk.new_end.Traversal(deleted_text_slice_start));
    deleted_text_slice_start = hunk.new_end;
  }

  result->Append(deleted_text_slice);
  return result;
}

static const uint32_t SERIALIZATION_VERSION = 1;

enum Transition: uint32_t {
  None,
  Left,
  Right,
  Up
};

template<typename T>
T network_to_host(T input);

template<typename T>
T host_to_network(T input);

template<>
uint16_t network_to_host(uint16_t input) {
  return ntohs(input);
}

template<>
uint16_t host_to_network(uint16_t input) {
  return htons(input);
}

template<>
uint32_t network_to_host(uint32_t input) {
  return ntohl(input);
}

template<>
uint32_t host_to_network(uint32_t input) {
  return htonl(input);
}

template<typename T>
void AppendToBuffer(vector<uint8_t> *output, T value) {
  value = host_to_network(value);
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
  output->insert(output->end(), bytes, bytes + sizeof(T));
}

template<typename T>
T GetFromBuffer(const uint8_t **data, const uint8_t *end) {
  const T *pointer = reinterpret_cast<const T *>(*data);
  *data = *data + sizeof(T);
  if (*data <= end) {
    return network_to_host<T>(*pointer);
  } else {
    return 0;
  }
}

void GetPointFromBuffer(const uint8_t **data, const uint8_t *end, Point *point) {
  point->row = GetFromBuffer<uint32_t>(data, end);
  point->column = GetFromBuffer<uint32_t>(data, end);
}

void AppendPointToBuffer(vector<uint8_t> *output, const Point &point) {
  AppendToBuffer(output, point.row);
  AppendToBuffer(output, point.column);
}

void AppendTextToBuffer(vector<uint8_t> *output, const Text *text) {
  if (text) {
    AppendToBuffer<uint32_t>(output, 1);
    AppendToBuffer(output, static_cast<uint32_t>(text->content.size()));
    for (uint16_t character : text->content) {
      AppendToBuffer(output, character);
    }
  } else {
    AppendToBuffer<uint32_t>(output, 0);
  }
}

unique_ptr<Text> GetTextFromBuffer(const uint8_t **data, const uint8_t *end) {
  if (GetFromBuffer<uint32_t>(data, end)) {
    uint32_t length = GetFromBuffer<uint32_t>(data, end);
    vector<uint16_t> content;
    content.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
      content.push_back(GetFromBuffer<uint16_t>(data, end));
    }
    return Text::Build(content);
  } else {
    return nullptr;
  }
}

void GetNodeFromBuffer(const uint8_t **data, const uint8_t *end, Node *node) {
  GetPointFromBuffer(data, end, &node->old_extent);
  GetPointFromBuffer(data, end, &node->new_extent);
  GetPointFromBuffer(data, end, &node->old_distance_from_left_ancestor);
  GetPointFromBuffer(data, end, &node->new_distance_from_left_ancestor);
  node->old_text = GetTextFromBuffer(data, end);
  node->new_text = GetTextFromBuffer(data, end);
  node->left = nullptr;
  node->right = nullptr;
}

void AppendNodeToBuffer(vector<uint8_t> *output, const Node &node) {
  AppendPointToBuffer(output, node.old_extent);
  AppendPointToBuffer(output, node.new_extent);
  AppendPointToBuffer(output, node.old_distance_from_left_ancestor);
  AppendPointToBuffer(output, node.new_distance_from_left_ancestor);
  AppendTextToBuffer(output, node.old_text.get());
  AppendTextToBuffer(output, node.new_text.get());
}

void Patch::Serialize(vector<uint8_t> *output) const {
  if (!root) return;

  AppendToBuffer(output, SERIALIZATION_VERSION);

  AppendToBuffer(output, hunk_count);

  AppendNodeToBuffer(output, *root);

  Node *node = root;
  node_stack.clear();
  int previous_node_child_index = -1;

  while (node) {
    if (node->left && previous_node_child_index < 0) {
      AppendToBuffer<uint32_t>(output, Left);
      AppendNodeToBuffer(output, *node->left);
      node_stack.push_back(node);
      node = node->left;
      previous_node_child_index = -1;
    } else if (node->right && previous_node_child_index < 1) {
      AppendToBuffer<uint32_t>(output, Right);
      AppendNodeToBuffer(output, *node->right);
      node_stack.push_back(node);
      node = node->right;
      previous_node_child_index = -1;
    } else if (!node_stack.empty()) {
      AppendToBuffer<uint32_t>(output, Up);
      Node *parent = node_stack.back();
      node_stack.pop_back();
      previous_node_child_index = (node == parent->left) ? 0 : 1;
      node = parent;
    } else {
      break;
    }
  }
}

Patch::Patch(const vector<uint8_t> &input) : root{nullptr}, is_frozen{true}, merges_adjacent_hunks{true} {
  const uint8_t *begin = input.data();
  const uint8_t *data = begin;
  const uint8_t *end = data + input.size();

  uint32_t serialization_version = GetFromBuffer<uint32_t>(&data, end);
  if (serialization_version != SERIALIZATION_VERSION) {
    return;
  }

  hunk_count = GetFromBuffer<uint32_t>(&data, end);
  if (hunk_count == 0) {
    return;
  }

  node_stack.reserve(hunk_count);
  root = static_cast<Node *>(calloc(hunk_count, sizeof(Node)));
  Node *node = root, *next_node = root + 1;
  GetNodeFromBuffer(&data, end, node);

  while (next_node < root + hunk_count) {
    switch (GetFromBuffer<uint32_t>(&data, end)) {
      case Left:
        GetNodeFromBuffer(&data, end, next_node);
        node->left = next_node;
        node_stack.push_back(node);
        node = next_node;
        next_node++;
        break;
      case Right:
        GetNodeFromBuffer(&data, end, next_node);
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
