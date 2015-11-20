#include <unordered_set>
#include <vector>
#include "iterator.h"
#include "marker-index.h"
#include "range.h"

using std::unordered_map;
using std::unordered_set;

Iterator::Iterator(MarkerIndex *marker_index) :
  marker_index {marker_index},
  node {nullptr} {}

void Iterator::Reset() {
  node = marker_index->root;
  if (node) {
    node_position = node->left_extent;
  }
  left_ancestor_position = Point(0, 0);
  right_ancestor_position = Point::Max();
  left_ancestor_position_stack.clear();
  right_ancestor_position_stack.clear();
}

Node* Iterator::InsertMarkerStart(const MarkerId &id, const Point &start_position, const Point &end_position) {
  Reset();

  if (!node) {
    return marker_index->root = new Node(nullptr, start_position);
  }

  while (true) {
    switch (start_position.Compare(node_position)) {
      case 0:
        MarkRight(id, start_position, end_position);
        return node;
      case -1:
        MarkRight(id, start_position, end_position);
        if (node->left) {
          DescendLeft();
          break;
        } else {
          InsertLeftChild(start_position);
          DescendLeft();
          MarkRight(id, start_position, end_position);
          return node;
        }
      case 1:
        if (node->right) {
          DescendRight();
          break;
        } else {
          InsertRightChild(start_position);
          DescendRight();
          MarkRight(id, start_position, end_position);
          return node;
        }
    }
  }
}

Node* Iterator::InsertMarkerEnd(const MarkerId &id, const Point &start_position, const Point &end_position) {
  Reset();

  if (!node) {
    return marker_index->root = new Node(nullptr, end_position);
  }

  while (true) {
    switch (end_position.Compare(node_position)) {
      case 0:
        MarkLeft(id, start_position, end_position);
        return node;
      case -1:
        if (node->left) {
          DescendLeft();
          break;
        } else {
          InsertLeftChild(end_position);
          DescendLeft();
          MarkLeft(id, start_position, end_position);
          return node;
        }
      case 1:
        MarkLeft(id, start_position, end_position);
        if (node->right) {
          DescendRight();
          break;
        } else {
          InsertRightChild(end_position);
          DescendRight();
          MarkLeft(id, start_position, end_position);
          return node;
        }
    }
  }
}

Node* Iterator::InsertSpliceBoundary(const Point &position, bool is_insertion_end) {
  Reset();

  while (true) {
    int comparison = position.Compare(node_position);
    if (comparison == 0 && !is_insertion_end) {
      return node;
    } else if (comparison < 0) {
      if (node->left) {
        DescendLeft();
      } else {
        InsertLeftChild(position);
        return node->left;
      }
    } else { // comparison > 0
      if (node->right) {
        DescendRight();
      } else {
        InsertRightChild(position);
        return node->right;
      }
    }
  }
}

void Iterator::FindIntersecting(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!node) return;

  while (true) {
    if (start < node_position) {
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
  } while (node && node_position <= end);
}

void Iterator::FindContainedIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  unordered_set<MarkerId> started;
  while (node && node_position <= end) {
    started.insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    for (const MarkerId &id : node->end_marker_ids) {
      if (started.count(id) > 0) result->insert(id);
    }
    MoveToSuccessor();
  }
}

void Iterator::FindStartingIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (node && node_position <= end) {
    result->insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    MoveToSuccessor();
  }
}

void Iterator::FindEndingIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (node && node_position <= end) {
    result->insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
    MoveToSuccessor();
  }
}

unordered_map<MarkerId, Range> Iterator::Dump() {
  Reset();

  unordered_map<MarkerId, Range> snapshot;

  if (!node) return snapshot;

  while (node && node->left) DescendLeft();

  while (node) {
    for (const MarkerId &id : node->start_marker_ids) {
      Range range;
      range.start = node_position;
      snapshot.insert({id, range});
    }
    for (const MarkerId &id : node->end_marker_ids) {
      snapshot[id].end = node_position;
    }
    MoveToSuccessor();
  }

  return snapshot;
}

void Iterator::Ascend() {
  if (node->parent) {
    if (node->parent->left == node) {
      node_position = right_ancestor_position;
    } else {
      node_position = left_ancestor_position;
    }
    left_ancestor_position = left_ancestor_position_stack.back();
    left_ancestor_position_stack.pop_back();
    right_ancestor_position = right_ancestor_position_stack.back();
    right_ancestor_position_stack.pop_back();
    node = node->parent;
  } else {
    node = nullptr;
    node_position = Point();
    left_ancestor_position = Point();
    right_ancestor_position = Point::Max();
  }
}

void Iterator::DescendLeft() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  right_ancestor_position = node_position;
  node = node->left;
  node_position = left_ancestor_position.Traverse(node->left_extent);
}

void Iterator::DescendRight() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  left_ancestor_position = node_position;
  node = node->right;
  node_position = left_ancestor_position.Traverse(node->left_extent);
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

void Iterator::SeekToFirstNodeGreaterThanOrEqualTo(const Point &position) {
  while (true) {
    if (position == node_position) {
      break;
    } else if (position < node_position) {
      if (node->left) {
        DescendLeft();
      } else {
        break;
      }
    } else { // position > node_position
      if (node->right) {
        DescendRight();
      } else {
        break;
      }
    }
  }

  if (node_position < position) MoveToSuccessor();
}

void Iterator::MarkRight(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (left_ancestor_position < start_position
    && start_position <= node_position
    && right_ancestor_position <= end_position) {
    node->right_marker_ids.insert(id);
  }
}

void Iterator::MarkLeft(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (!node_position.IsZero() && start_position <= left_ancestor_position && node_position <= end_position) {
    node->left_marker_ids.insert(id);
  }
}

Node* Iterator::InsertLeftChild(const Point &position) {
  return node->left = new Node(node, position.Traversal(left_ancestor_position));
}

Node* Iterator::InsertRightChild(const Point &position) {
  return node->right = new Node(node, position.Traversal(node_position));
}

void Iterator::CheckIntersection(const Point &start, const Point &end, unordered_set<MarkerId> *result) {
  if (left_ancestor_position <= end && start <= node_position) {
    result->insert(node->left_marker_ids.begin(), node->left_marker_ids.end());
  }

  if (start <= node_position && node_position <= end) {
    result->insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    result->insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
  }

  if (node_position <= end && start <= right_ancestor_position) {
    result->insert(node->right_marker_ids.begin(), node->right_marker_ids.end());
  }
}
