#include <unordered_set>
#include <vector>
#include "iterator.h"
#include "marker-index.h"

using std::set;
using std::unordered_set;

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

Node* Iterator::InsertSpliceBoundary(const Point &offset, bool is_insertion_end) {
  Reset();

  while (true) {
    int comparison = offset.Compare(node_offset);
    if (comparison == 0 && !is_insertion_end) {
      return node;
    } else if (comparison < 0) {
      if (node->left) {
        DescendLeft();
      } else {
        InsertLeftChild(offset);
        return node->left;
      }
    } else { // comparison > 0
      if (node->right) {
        DescendRight();
      } else {
        InsertRightChild(offset);
        return node->right;
      }
    }
  }
}

void Iterator::FindIntersecting(const Point &start, const Point &end, std::set<MarkerId> *result) {
  Reset();

  if (!node) return;

  while (true) {
    if (start < node_offset) {
      if (node->left) {
        CheckIntersection(start, end, result);
        DescendLeft();
      } else {
        break;
      }
    } else {
      if (node->right) {
        CheckIntersection(start, end, result);
        DescendRight();
      } else {
        break;
      }
    }
  }

  do {
    CheckIntersection(start, end, result);
    MoveToSuccessor();
  } while (node && node_offset <= end);
}

void Iterator::FindContainedIn(const Point &start, const Point &end, std::set<MarkerId> *result) {
  Reset();

  if (!node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  unordered_set<MarkerId> started;
  while (node && node_offset <= end) {
    started.insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    for (const MarkerId &id : node->end_marker_ids) {
      if (started.count(id) > 0) result->insert(id);
    }
    MoveToSuccessor();
  }
}

void Iterator::FindStartingIn(const Point &start, const Point &end, std::set<MarkerId> *result) {
  Reset();

  if (!node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (node && node_offset <= end) {
    result->insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    MoveToSuccessor();
  }
}

void Iterator::FindEndingIn(const Point &start, const Point &end, std::set<MarkerId> *result) {
  Reset();

  if (!node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (node && node_offset <= end) {
    result->insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
    MoveToSuccessor();
  }
}

void Iterator::Ascend() {
  if (node->parent) {
    if (node->parent->left == node) {
      node_offset = right_ancestor_offset;
    } else {
      node_offset = left_ancestor_offset;
    }
    left_ancestor_offset = left_ancestor_offset_stack.back();
    left_ancestor_offset_stack.pop_back();
    right_ancestor_offset = right_ancestor_offset_stack.back();
    right_ancestor_offset_stack.pop_back();
    node = node->parent;
  } else {
    node = nullptr;
    node_offset = Point();
    left_ancestor_offset = Point();
    right_ancestor_offset = Point::Max();
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

void Iterator::MoveToSuccessor() {
  if (!node) return;

  if (node->right) {
    DescendRight();
    while (node->left) {
      DescendLeft();
    }
  } else {
    while (node->parent && node->parent->right == node) {
      Ascend();
    }
    Ascend();
  }
}

void Iterator::SeekToFirstNodeGreaterThanOrEqualTo(const Point &offset) {
  while (true) {
    if (offset == node_offset) {
      break;
    } else if (offset < node_offset) {
      if (node->left) {
        DescendLeft();
      } else {
        break;
      }
    } else { // offset > node_offset
      if (node->right) {
        DescendRight();
      } else {
        break;
      }
    }
  }

  if (node_offset < offset) MoveToSuccessor();
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

void Iterator::CheckIntersection(const Point &start, const Point &end, set<MarkerId> *result) {
  if (left_ancestor_offset <= end && start <= node_offset) {
    result->insert(node->left_marker_ids.begin(), node->left_marker_ids.end());
  }

  if (start <= node_offset && node_offset <= end) {
    result->insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    result->insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
  }

  if (node_offset <= end && start <= right_ancestor_offset) {
    result->insert(node->right_marker_ids.begin(), node->right_marker_ids.end());
  }
}
