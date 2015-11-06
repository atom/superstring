#include "iterator.h"
#include "marker-index.h"

Iterator::Iterator(MarkerIndex *marker_index) :
  marker_index {marker_index},
  node {nullptr} {}

void Iterator::Reset() {
  node = marker_index->root;
  if (node) {
    node_offset = node->left_extent;
  }
  left_ancestor_offset = Point(0, 0);
  right_ancestor_offset = Point::Max();
  left_ancestor_offset_stack.clear();
  right_ancestor_offset_stack.clear();
}

Node* Iterator::InsertMarkerStart(const MarkerId &id, const Point &start_offset, const Point &end_offset) {
  Reset();

  if (!node) {
    return marker_index->root = new Node(nullptr, start_offset);
  }

  while (true) {
    switch (start_offset.Compare(node_offset)) {
      case 0:
        MarkRight(id, start_offset, end_offset);
        return node;
      case -1:
        MarkRight(id, start_offset, end_offset);
        if (node->left) {
          DescendLeft();
          break;
        } else {
          InsertLeftChild(start_offset);
          DescendLeft();
          MarkRight(id, start_offset, end_offset);
          return node;
        }
      case 1:
        if (node->right) {
          DescendRight();
          break;
        } else {
          InsertRightChild(start_offset);
          DescendRight();
          MarkRight(id, start_offset, end_offset);
          return node;
        }
    }
  }
}

Node* Iterator::InsertMarkerEnd(const MarkerId &id, const Point &start_offset, const Point &end_offset) {
  Reset();

  if (!node) {
    return marker_index->root = new Node(nullptr, end_offset);
  }

  while (true) {
    switch (end_offset.Compare(node_offset)) {
      case 0:
        MarkLeft(id, start_offset, end_offset);
        return node;
      case -1:
        if (node->left) {
          DescendLeft();
          break;
        } else {
          InsertLeftChild(end_offset);
          DescendLeft();
          MarkLeft(id, start_offset, end_offset);
          return node;
        }
      case 1:
        MarkLeft(id, start_offset, end_offset);
        if (node->right) {
          DescendRight();
          break;
        } else {
          InsertRightChild(end_offset);
          DescendRight();
          MarkLeft(id, start_offset, end_offset);
          return node;
        }
    }
  }
}

void Iterator::DescendLeft() {
  left_ancestor_offset_stack.push_back(left_ancestor_offset);
  right_ancestor_offset_stack.push_back(right_ancestor_offset);

  right_ancestor_offset = node_offset;
  node = node->left;
  node_offset = left_ancestor_offset.Traverse(node->left_extent);
}

void Iterator::DescendRight() {
  left_ancestor_offset_stack.push_back(left_ancestor_offset);
  right_ancestor_offset_stack.push_back(right_ancestor_offset);

  left_ancestor_offset = node_offset;
  node = node->right;
  node_offset = left_ancestor_offset.Traverse(node->left_extent);
}

void Iterator::MarkRight(const MarkerId &id, const Point &start_offset, const Point &end_offset) {
  if (left_ancestor_offset < start_offset
    && start_offset <= node_offset
    && right_ancestor_offset <= end_offset) {
    node->right_marker_ids.insert(id);
  }
}

void Iterator::MarkLeft(const MarkerId &id, const Point &start_offset, const Point &end_offset) {
  if (!node_offset.IsZero() && start_offset <= left_ancestor_offset && node_offset <= end_offset) {
    node->left_marker_ids.insert(id);
  }
}

Node* Iterator::InsertLeftChild(const Point &offset) {
  return node->left = new Node(node, offset.Traversal(left_ancestor_offset));
}

Node* Iterator::InsertRightChild(const Point &offset) {
  return node->right = new Node(node, offset.Traversal(node_offset));
}
