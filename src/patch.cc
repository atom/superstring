#include <assert.h>
#include <stdio.h>
#include <vector>
#include "patch.h"

using std::vector;

struct Node {
  Node *parent;
  Node *left;
  Node *right;

  Point old_distance_from_left_ancestor;
  Point new_distance_from_left_ancestor;
  Point old_extent;
  Point new_extent;

  ~Node() {
    if (left) {
      delete left;
    }

    if (right) {
      delete right;
    }
  }

  bool IsLeftChild() const {
    return parent && parent->left == this;
  }

  bool IsRightChild() const {
    return parent && parent->right == this;
  }

  void DeleteLeft() {
    delete left;
    left = nullptr;
  }

  void DeleteRight() {
    delete right;
    right = nullptr;
  }

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

Patch::Patch() : root{nullptr} {}

Patch::~Patch() {
  if (root) {
    delete root;
  }
}

template<typename CoordinateSpace>
Node *SplayLowerBound(Patch &patch, Point target) {
  Node *lower_bound = nullptr;
  Point left_ancestor_end = Point::Zero();
  Node *node = patch.root;

  while (node) {
    Point node_start = left_ancestor_end.Traverse(CoordinateSpace::distance_from_left_ancestor(node));
    if (node_start <= target) {
      lower_bound = node;
      if (node->right) {
        left_ancestor_end = node_start.Traverse(CoordinateSpace::extent(node));
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

  if (lower_bound) {
    patch.SplayNode(lower_bound);
  }

  return lower_bound;
}

template<typename CoordinateSpace>
Node *SplayUpperBound(Patch &patch, Point target) {
  Node *upper_bound = nullptr;
  Point left_ancestor_end = Point::Zero();
  Node *node = patch.root;

  while (node) {
    Point node_end = left_ancestor_end
      .Traverse(CoordinateSpace::distance_from_left_ancestor(node))
      .Traverse(CoordinateSpace::extent(node));
    if (node_end >= target) {
      upper_bound = node;
      if (node->left) {
        node = node->left;
      } else {
        break;
      }
    } else {
      if (node->right) {
        left_ancestor_end = node_end;
        node = node->right;
      } else {
        break;
      }
    }
  }

  if (upper_bound) {
    patch.SplayNode(upper_bound);
  }

  return upper_bound;
}

template<typename CoordinateSpace>
vector<Hunk> GetHunksInRange(Patch &patch, Point start, Point end) {
  vector<Hunk> result;
  if (!patch.root) {
    return result;
  }

  patch.left_ancestor_stack.clear();
  patch.left_ancestor_stack.push_back({Point::Zero(), Point::Zero()});

  Node *node;
  if (SplayLowerBound<CoordinateSpace>(patch, start)) {
    node = patch.root;
  } else {
    node = patch.root;
    while (node->left) {
      node = node->left;
    }
  }

  while (node) {
    Patch::PositionStackEntry &left_ancestor_position = patch.left_ancestor_stack.back();
    Point old_start = left_ancestor_position.old_end.Traverse(node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_position.new_end.Traverse(node->new_distance_from_left_ancestor);
    Point old_end = old_start.Traverse(node->old_extent);
    Point new_end = new_start.Traverse(node->new_extent);
    Hunk hunk = {old_start, old_end, new_start, new_end};

    if (CoordinateSpace::start(hunk) >= end) {
      break;
    }

    if (CoordinateSpace::end(hunk) > start) {
      result.push_back(hunk);
    }

    if (node->right) {
      patch.left_ancestor_stack.push_back(Patch::PositionStackEntry{old_end, new_end});
      node = node->right;

      while (node->left) {
        node = node->left;
      }
    } else {
      while (node->IsRightChild()) {
        node = node->parent;
        patch.left_ancestor_stack.pop_back();
      }

      node = node->parent;
    }
  }

  return result;
}

template<typename InputSpace, typename OutputSpace>
Point TranslatePosition(Patch &patch, Point target) {
  Node *lower_bound = SplayLowerBound<InputSpace>(patch, target);
  if (lower_bound) {
    Point input_start = InputSpace::distance_from_left_ancestor(lower_bound);
    Point output_start = OutputSpace::distance_from_left_ancestor(lower_bound);
    Point input_end = input_start.Traverse(InputSpace::extent(lower_bound));
    Point output_end = output_start.Traverse(OutputSpace::extent(lower_bound));
    if (target >= input_end) {
      return output_end.Traverse(target.Traversal(input_end));
    } else {
      return Point::Min(
        output_end,
        output_start.Traverse(target.Traversal(input_start))
      );
    }
  } else {
    return target;
  }
}

void Patch::Splice(Point new_splice_start, Point new_deletion_extent, Point new_insertion_extent) {
  if (new_deletion_extent.IsZero() && new_insertion_extent.IsZero()) {
    return;
  }

  if (!root) {
    root = new Node{
      nullptr,
      nullptr,
      nullptr,
      new_splice_start,
      new_splice_start,
      new_deletion_extent,
      new_insertion_extent
    };
    return;
  }

  Point new_deletion_end = new_splice_start.Traverse(new_deletion_extent);
  Point new_insertion_end = new_splice_start.Traverse(new_insertion_extent);

  Node *lower_bound = SplayLowerBound<NewCoordinates>(*this, new_splice_start);
  Node *upper_bound = SplayUpperBound<NewCoordinates>(*this, new_deletion_end);
  if (upper_bound && lower_bound && lower_bound != upper_bound) {
    if (lower_bound != upper_bound->left) {
      RotateNodeRight(lower_bound);
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

    // Splice overlaps both the upper and lower bounds
    if (new_splice_start <= lower_bound_new_end && new_deletion_end >= upper_bound_new_start) {
      Point new_extent_prefix = new_splice_start.Traversal(lower_bound_new_start);
      Point new_extent_suffix = upper_bound_new_end.Traversal(new_deletion_end);

      upper_bound->old_extent = upper_bound_old_end.Traversal(lower_bound_old_start);
      upper_bound->new_extent = new_extent_prefix.Traverse(new_insertion_extent).Traverse(new_extent_suffix);
      upper_bound->old_distance_from_left_ancestor = lower_bound_old_start;
      upper_bound->new_distance_from_left_ancestor = lower_bound_new_start;

      if (lower_bound == upper_bound) {
        if (root->old_extent.IsZero() && root->new_extent.IsZero()) {
          DeleteRoot();
        }
      } else {
        upper_bound->left = lower_bound->left;
        if (upper_bound->left) {
          upper_bound->left->parent = upper_bound;
        }
        lower_bound->left = nullptr;
        delete lower_bound;
      }

    // Splice overlaps the upper bound
    } else if (new_deletion_end >= upper_bound_new_start) {
      Point old_splice_start = lower_bound_old_end.Traverse(new_splice_start.Traversal(lower_bound_new_end));
      Point new_extent_suffix = upper_bound_new_end.Traversal(new_deletion_end);

      upper_bound->old_distance_from_left_ancestor = old_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent = upper_bound_old_end.Traversal(old_splice_start);
      upper_bound->new_extent = new_insertion_extent.Traverse(new_extent_suffix);
      lower_bound->DeleteRight();
      if (upper_bound->left != lower_bound) {
        upper_bound->DeleteLeft();
      }

    // Splice overlaps the lower bound
    } else if (new_splice_start <= lower_bound_new_end) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      lower_bound->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
      Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));
      Point new_extent_prefix = new_splice_start.Traversal(lower_bound_new_start);

      upper_bound->new_distance_from_left_ancestor = new_insertion_end.Traverse(upper_bound_new_start.Traversal(new_deletion_end));
      lower_bound->old_extent = old_deletion_end.Traversal(lower_bound_old_start);
      lower_bound->new_extent = new_extent_prefix.Traverse(new_insertion_extent);
      lower_bound->DeleteRight();
      RotateNodeRight(lower_bound);

    // Splice doesn't overlap either bound
    } else {
      Point rightmost_child_old_end, rightmost_child_new_end;
      lower_bound->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
      Point old_splice_start = lower_bound_old_end.Traverse(new_splice_start.Traversal(lower_bound_new_end));
      Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));

      root = new Node{
        nullptr,
        lower_bound,
        upper_bound,
        old_splice_start,
        new_splice_start,
        old_deletion_end.Traversal(old_splice_start),
        new_insertion_extent
      };

      lower_bound->DeleteRight();
      lower_bound->parent = root;
      upper_bound->left = nullptr;
      upper_bound->parent = root;
      upper_bound->old_distance_from_left_ancestor = upper_bound_old_start.Traversal(old_deletion_end);
      upper_bound->new_distance_from_left_ancestor = upper_bound_new_start.Traversal(new_deletion_end);
    }

  } else if (lower_bound) {
    Point lower_bound_old_start = lower_bound->old_distance_from_left_ancestor;
    Point lower_bound_new_start = lower_bound->new_distance_from_left_ancestor;
    Point lower_bound_new_end = lower_bound_new_start.Traverse(lower_bound->new_extent);
    Point lower_bound_old_end = lower_bound_old_start.Traverse(lower_bound->old_extent);
    Point rightmost_child_old_end, rightmost_child_new_end;
    lower_bound->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
    Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));

    lower_bound->DeleteRight();
    if (new_splice_start <= lower_bound_new_end) {
      lower_bound->old_extent = old_deletion_end.Traversal(lower_bound_old_start);
      lower_bound->new_extent = new_insertion_end.Traversal(lower_bound_new_start);
    } else {
      Point old_splice_start = lower_bound_old_end.Traverse(new_splice_start.Traversal(lower_bound_new_end));
      root = new Node{
        nullptr,
        lower_bound,
        nullptr,
        old_splice_start,
        new_splice_start,
        old_deletion_end.Traversal(old_splice_start),
        new_insertion_extent
      };
      lower_bound->parent = root;
    }

  } else if (upper_bound) {
    Point upper_bound_new_start = upper_bound->new_distance_from_left_ancestor;
    Point upper_bound_old_start = upper_bound->old_distance_from_left_ancestor;
    Point upper_bound_new_end = upper_bound_new_start.Traverse(upper_bound->new_extent);

    Point old_deletion_end;
    if (upper_bound->left) {
      Point rightmost_child_old_end, rightmost_child_new_end;
      upper_bound->left->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
      old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));
    } else {
      old_deletion_end = new_deletion_end;
    }

    upper_bound->DeleteLeft();
    if (new_deletion_end >= upper_bound_new_start) {
      upper_bound->old_distance_from_left_ancestor = new_splice_start;
      upper_bound->new_distance_from_left_ancestor = new_splice_start;
      upper_bound->old_extent = upper_bound_old_start.Traversal(new_splice_start).Traverse(upper_bound->old_extent);
      upper_bound->new_extent = new_insertion_extent.Traverse(upper_bound_new_end.Traversal(new_deletion_end));
    } else {
      root = new Node{
        nullptr,
        nullptr,
        upper_bound,
        new_splice_start,
        new_splice_start,
        old_deletion_end.Traversal(new_splice_start),
        new_insertion_extent
      };
      upper_bound->parent = root;
      Point distance_from_end_of_root_to_start_of_upper_bound = upper_bound_new_start.Traversal(new_deletion_end);
      upper_bound->old_distance_from_left_ancestor = distance_from_end_of_root_to_start_of_upper_bound;
      upper_bound->new_distance_from_left_ancestor = distance_from_end_of_root_to_start_of_upper_bound;
    }

  } else {
    Point rightmost_child_old_end, rightmost_child_new_end;
    root->GetSubtreeEnd(&rightmost_child_old_end, &rightmost_child_new_end);
    Point old_deletion_end = rightmost_child_old_end.Traverse(new_deletion_end.Traversal(rightmost_child_new_end));
    delete root;
    root = new Node{
      nullptr,
      nullptr,
      nullptr,
      new_splice_start,
      new_splice_start,
      old_deletion_end.Traversal(new_splice_start),
      new_insertion_extent
    };
  }
}

void Patch::SplayNode(Node *node) {
  while (node->parent) {
    Node *parent = node->parent;
    if (parent->IsLeftChild() && node->IsRightChild()) {
      RotateNodeLeft(node);
      RotateNodeRight(node);
    } else if (parent->IsRightChild() && node->IsLeftChild()) {
      RotateNodeRight(node);
      RotateNodeLeft(node);
    } else if (parent->IsLeftChild() && node->IsLeftChild()) {
      RotateNodeRight(parent);
      RotateNodeRight(node);
    } else if (parent->IsRightChild() && node->IsRightChild()) {
      RotateNodeLeft(parent);
      RotateNodeLeft(node);
    } else if (node->IsLeftChild()) {
      RotateNodeRight(node);
    } else if (node->IsRightChild()) {
      RotateNodeLeft(node);
    } else {
      assert(!"Unexpected state");
    }
  }
}

void Patch::RotateNodeLeft(Node *pivot) {
  Node *root = pivot->parent;
  Node *root_parent = root->parent;

  if (root_parent) {
    if (root->IsLeftChild()) {
      root_parent->left = pivot;
    } else {
      root_parent->right = pivot;
    }
  } else {
    this->root = pivot;
  }
  pivot->parent = root_parent;

  root->right = pivot->left;
  if (root->right) {
    root->right->parent = root;
  }

  pivot->left = root;
  root->parent = pivot;

  pivot->old_distance_from_left_ancestor = root->old_distance_from_left_ancestor
    .Traverse(root->old_extent)
    .Traverse(pivot->old_distance_from_left_ancestor);
  pivot->new_distance_from_left_ancestor = root->new_distance_from_left_ancestor
    .Traverse(root->new_extent)
    .Traverse(pivot->new_distance_from_left_ancestor);
}

void Patch::RotateNodeRight(Node *pivot) {
  Node *root = pivot->parent;
  Node *root_parent = root->parent;

  if (root_parent) {
    if (root->IsLeftChild()) {
      root_parent->left = pivot;
    } else {
      root_parent->right = pivot;
    }
  } else {
    this->root = pivot;
  }
  pivot->parent = root_parent;

  root->left = pivot->right;
  if (root->left) {
    root->left->parent = root;
  }

  pivot->right = root;
  root->parent = pivot;

  root->old_distance_from_left_ancestor = root->old_distance_from_left_ancestor
    .Traversal(pivot->old_distance_from_left_ancestor.Traverse(pivot->old_extent));
  root->new_distance_from_left_ancestor = root->new_distance_from_left_ancestor
    .Traversal(pivot->new_distance_from_left_ancestor.Traverse(pivot->new_extent));
}

void Patch::DeleteRoot() {
  Node *node = root;
  while (true) {
    if (node->left) {
      RotateNodeRight(node->left);
    } else if (node->right) {
      RotateNodeLeft(node->right);
    } else {
      if (node->IsLeftChild()) {
        node->parent->DeleteLeft();
      } else if (node->IsRightChild()) {
        node->parent->DeleteRight();
      } else {
        delete node;
        root = nullptr;
      }
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
    "node_%p [label=\"new: (%u, %u) - (%u, %u)\nold: (%u, %u) - (%u, %u)\"]\n",
    node,
    node_new_start.row,
    node_new_start.column,
    node_new_end.row,
    node_new_end.column,
    node_old_start.row,
    node_old_start.column,
    node_old_end.row,
    node_old_end.column
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

vector<Hunk> Patch::GetHunks() const {
  vector<Hunk> result;
  if (!root) {
    return result;
  }

  Node *node = root;
  left_ancestor_stack.clear();
  left_ancestor_stack.push_back({Point::Zero(), Point::Zero()});

  while (node->left) {
    node = node->left;
  }

  while (node) {
    PositionStackEntry &left_ancestor_position = left_ancestor_stack.back();
    Point old_start = left_ancestor_position.old_end.Traverse(node->old_distance_from_left_ancestor);
    Point new_start = left_ancestor_position.new_end.Traverse(node->new_distance_from_left_ancestor);
    Point old_end = old_start.Traverse(node->old_extent);
    Point new_end = new_start.Traverse(node->new_extent);
    result.push_back(Hunk{old_start, old_end, new_start, new_end});

    if (node->right) {
      left_ancestor_stack.push_back(PositionStackEntry{old_end, new_end});
      node = node->right;

      while (node->left) {
        node = node->left;
      }
    } else {
      while (node->IsRightChild()) {
        node = node->parent;
        left_ancestor_stack.pop_back();
      }

      node = node->parent;
    }
  }

  return result;
}

vector<Hunk> Patch::GetHunksInOldRange(Point start, Point end) {
  return GetHunksInRange<OldCoordinates>(*this, start, end);
}

vector<Hunk> Patch::GetHunksInNewRange(Point start, Point end) {
  return GetHunksInRange<NewCoordinates>(*this, start, end);
}

Point Patch::TranslateOldPosition(Point target) {
  return TranslatePosition<OldCoordinates, NewCoordinates>(*this, target);
}

Point Patch::TranslateNewPosition(Point target) {
  return TranslatePosition<NewCoordinates, OldCoordinates>(*this, target);
}
