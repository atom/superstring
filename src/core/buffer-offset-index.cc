#include <algorithm>
#include <random>
#include <climits>
#include "buffer-offset-index.h"

struct Extent {
  unsigned characters;
  unsigned rows;
};

struct LineNode {
  LineNode *left;
  LineNode *right;
  unsigned priority;
  unsigned length;
  Extent left_subtree_extent;
  Extent right_subtree_extent;

  ~LineNode() {
    if (left) {
      delete left;
    }
    if (right) {
      delete right;
    }
  }

  void compute_subtree_extents() {
    if (left) {
      left_subtree_extent = Extent {
        left->left_subtree_extent.characters + left->length + left->right_subtree_extent.characters,
        left->left_subtree_extent.rows + 1 + left->right_subtree_extent.rows
      };
    } else {
      left_subtree_extent = Extent {0, 0};
    }

    if (right) {
      right_subtree_extent = Extent {
        right->left_subtree_extent.characters + right->length + right->right_subtree_extent.characters,
        right->left_subtree_extent.rows + 1 + right->right_subtree_extent.rows
      };
    } else {
      right_subtree_extent = Extent {0, 0};
    }
  }
};

BufferOffsetIndex::BufferOffsetIndex() : root{nullptr} {}

BufferOffsetIndex::~BufferOffsetIndex() {
  if (root) {
    delete root;
  }
}

void BufferOffsetIndex::splice(unsigned start_row, unsigned deleted_lines_count, std::vector<unsigned> const &new_line_lengths) {
  auto start_node = find_and_bubble_node_up_to_root(start_row - 1);
  auto end_node = find_and_bubble_node_up_to_root(start_row + deleted_lines_count);

  if (start_node) {
    if (start_node->right) {
      delete start_node->right;
      start_node->right = nullptr;
    }

    if (!new_line_lengths.empty()) {
      start_node->right = build_node_tree_from_line_lengths(new_line_lengths, 0, new_line_lengths.size(), 1);
    }

    start_node->compute_subtree_extents();
  } else if (end_node) {
    if (end_node->left) {
      delete end_node->left;
      end_node->left = nullptr;
    }

    if (!new_line_lengths.empty()) {
      end_node->left = build_node_tree_from_line_lengths(new_line_lengths, 0, new_line_lengths.size(), 1);
    }

    end_node->compute_subtree_extents();
  } else {
    if (root) {
      delete root;
    }

    root = build_node_tree_from_line_lengths(new_line_lengths, 0, new_line_lengths.size(), 1);
  }

  if (end_node) {
    end_node->compute_subtree_extents();
  }

  std::uniform_int_distribution<unsigned> rng(1u, UINT_MAX);

  if (start_node) {
    start_node->priority = rng(rng_engine);
    bubble_node_down(start_node, end_node);
  }

  if (end_node) {
    end_node->priority = rng(rng_engine);
    bubble_node_down(end_node, nullptr);
  }
}

unsigned BufferOffsetIndex::character_index_for_position(Point position) const {
  auto left_ancestor_row = 0u;
  auto left_ancestor_char_index = 0u;
  auto current_node_char_index = 0u;
  auto current_node = root;
  while (current_node) {
    auto current_node_row = left_ancestor_row + current_node->left_subtree_extent.rows;
    current_node_char_index = left_ancestor_char_index + current_node->left_subtree_extent.characters;
    if (position.row < current_node_row) {
      current_node = current_node->left;
    } else if (position.row == current_node_row) {
      break;
    } else {
      left_ancestor_row = current_node_row + 1;
      left_ancestor_char_index = current_node_char_index + current_node->length;
      current_node = current_node->right;
    }
  }

  if (current_node) {
    return current_node_char_index + std::min(position.column, current_node->length);
  } else {
    return left_ancestor_char_index;
  }
}

Point BufferOffsetIndex::position_for_character_index(unsigned char_index) const {
  auto left_ancestor_row = 0u;
  auto left_ancestor_char_index = 0u;
  auto left_ancestor_length = 0u;
  auto current_node_row = 0u;
  auto current_node_char_index_start = 0u;
  auto current_node = root;
  while (current_node) {
      current_node_row = left_ancestor_row + current_node->left_subtree_extent.rows;
      current_node_char_index_start = left_ancestor_char_index + current_node->left_subtree_extent.characters;
      auto current_node_char_index_end = current_node_char_index_start + current_node->length;
      if (char_index >= current_node_char_index_start && char_index < current_node_char_index_end) {
          break;
      } else if (char_index < current_node_char_index_start) {
          current_node = current_node->left;
      } else {
          left_ancestor_row = current_node_row + 1;
          left_ancestor_length = current_node->length;
          left_ancestor_char_index = current_node_char_index_end;
          current_node = current_node->right;
      }
  }

  if (current_node) {
    return Point {current_node_row, char_index - current_node_char_index_start};
  } else {
    return Point {left_ancestor_row - 1, left_ancestor_length};
  }
}

LineNode *BufferOffsetIndex::find_and_bubble_node_up_to_root(unsigned row) {
  auto left_ancestor_row = 0u;
  auto current_node = root;
  auto ancestors_stack = std::vector<LineNode*>();
  while (current_node) {
    auto current_node_row = left_ancestor_row + current_node->left_subtree_extent.rows;
    if (row < current_node_row) {
      ancestors_stack.push_back(current_node);
      current_node = current_node->left;
    } else if (row == current_node_row) {
      break;
    } else {
      ancestors_stack.push_back(current_node);
      left_ancestor_row = current_node_row + 1;
      current_node = current_node->right;
    }
  }

  if (current_node) {
    while (!ancestors_stack.empty()) {
      auto root = ancestors_stack.back();
      ancestors_stack.pop_back();
      auto root_parent = ancestors_stack.empty() ? nullptr : ancestors_stack.back();
      if (root->right == current_node) {
        rotate_node_left(current_node, root, root_parent);
      } else { // root->left == current_node
        rotate_node_right(current_node, root, root_parent);
      }
    }

    return current_node;
  } else {
    return nullptr;
  }
}

void BufferOffsetIndex::bubble_node_down(LineNode * root, LineNode * root_parent) {
  while (true) {
    auto left_child_priority = root->left ? root->left->priority : UINT_MAX;
    auto right_child_priority = root->right ? root->right->priority : UINT_MAX;
    if (left_child_priority < right_child_priority && left_child_priority < root->priority) {
      auto pivot = root->left;
      rotate_node_right(pivot, root, root_parent);
      root_parent = pivot;
    } else if (right_child_priority < root->priority) {
      auto pivot = root->right;
      rotate_node_left(pivot, root, root_parent);
      root_parent = pivot;
    } else {
      break;
    }
  }
}

void BufferOffsetIndex::rotate_node_left(LineNode * pivot, LineNode * root, LineNode * root_parent) {
  if (root_parent) {
    if (root == root_parent->left) {
      root_parent->left = pivot;
    } else {
      root_parent->right = pivot;
    }
  } else {
    this->root = pivot;
  }

  root->right = pivot->left;
  pivot->left = root;

  root->compute_subtree_extents();
  pivot->compute_subtree_extents();
}

void BufferOffsetIndex::rotate_node_right(LineNode * pivot, LineNode * root, LineNode * root_parent) {
  if (root_parent) {
    if (root == root_parent->left) {
      root_parent->left = pivot;
    } else {
      root_parent->right = pivot;
    }
  } else {
    this->root = pivot;
  }

  root->left = pivot->right;
  pivot->right = root;

  root->compute_subtree_extents();
  pivot->compute_subtree_extents();
}

LineNode *BufferOffsetIndex::build_node_tree_from_line_lengths(std::vector<unsigned> const &line_lengths, unsigned start, unsigned end, unsigned min_priority) {
  if (start == end) {
    return nullptr;
  } else {
    std::uniform_int_distribution<unsigned> rng(min_priority, UINT_MAX);
    unsigned priority = rng(rng_engine) - 1;
    auto middle = (start + end) / 2;
    auto line_node = new LineNode {
      build_node_tree_from_line_lengths(line_lengths, start, middle, priority + 1),
      build_node_tree_from_line_lengths(line_lengths, middle + 1, end, priority + 1),
      priority,
      line_lengths[middle],
      Extent {0, 0},
      Extent {0, 0}
    };
    line_node->compute_subtree_extents();
    return line_node;
  }
}
