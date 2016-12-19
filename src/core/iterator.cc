#include <unordered_set>
#include <vector>
#include "iterator.h"
#include "marker-index.h"
#include "range.h"

using std::unordered_map;
using std::unordered_set;

Iterator::Iterator(MarkerIndex *marker_index) :
  marker_index {marker_index},
  current_node {nullptr} {}

void Iterator::Reset() {
  current_node = marker_index->root;
  if (current_node) {
    current_node_position = current_node->left_extent;
  }
  left_ancestor_position = Point(0, 0);
  right_ancestor_position = Point::Max();
  left_ancestor_position_stack.clear();
  right_ancestor_position_stack.clear();
}

Node* Iterator::InsertMarkerStart(const MarkerId &id, const Point &start_position, const Point &end_position) {
  Reset();

  if (!current_node) {
    return marker_index->root = new Node(nullptr, start_position);
  }

  while (true) {
    switch (start_position.Compare(current_node_position)) {
      case 0:
        MarkRight(id, start_position, end_position);
        return current_node;
      case -1:
        MarkRight(id, start_position, end_position);
        if (current_node->left) {
          DescendLeft();
          break;
        } else {
          InsertLeftChild(start_position);
          DescendLeft();
          MarkRight(id, start_position, end_position);
          return current_node;
        }
      case 1:
        if (current_node->right) {
          DescendRight();
          break;
        } else {
          InsertRightChild(start_position);
          DescendRight();
          MarkRight(id, start_position, end_position);
          return current_node;
        }
    }
  }
}

Node* Iterator::InsertMarkerEnd(const MarkerId &id, const Point &start_position, const Point &end_position) {
  Reset();

  if (!current_node) {
    return marker_index->root = new Node(nullptr, end_position);
  }

  while (true) {
    switch (end_position.Compare(current_node_position)) {
      case 0:
        MarkLeft(id, start_position, end_position);
        return current_node;
      case -1:
        if (current_node->left) {
          DescendLeft();
          break;
        } else {
          InsertLeftChild(end_position);
          DescendLeft();
          MarkLeft(id, start_position, end_position);
          return current_node;
        }
      case 1:
        MarkLeft(id, start_position, end_position);
        if (current_node->right) {
          DescendRight();
          break;
        } else {
          InsertRightChild(end_position);
          DescendRight();
          MarkLeft(id, start_position, end_position);
          return current_node;
        }
    }
  }
}

Node* Iterator::InsertSpliceBoundary(const Point &position, bool is_insertion_end) {
  Reset();

  while (true) {
    int comparison = position.Compare(current_node_position);
    if (comparison == 0 && !is_insertion_end) {
      return current_node;
    } else if (comparison < 0) {
      if (current_node->left) {
        DescendLeft();
      } else {
        InsertLeftChild(position);
        return current_node->left;
      }
    } else { // comparison > 0
      if (current_node->right) {
        DescendRight();
      } else {
        InsertRightChild(position);
        return current_node->right;
      }
    }
  }
}

void Iterator::FindIntersecting(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!current_node) return;

  while (true) {
    CacheNodePosition();
    if (start < current_node_position) {
      if (current_node->left) {
        CheckIntersection(start, end, result);
        DescendLeft();
      } else {
        break;
      }
    } else {
      if (current_node->right) {
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
    CacheNodePosition();
  } while (current_node && current_node_position <= end);
}

void Iterator::FindContainedIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!current_node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  unordered_set<MarkerId> started;
  while (current_node && current_node_position <= end) {
    started.insert(current_node->start_marker_ids.begin(), current_node->start_marker_ids.end());
    for (MarkerId id : current_node->end_marker_ids) {
      if (started.count(id) > 0) result->insert(id);
    }
    CacheNodePosition();
    MoveToSuccessor();
  }
}

void Iterator::FindStartingIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!current_node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (current_node && current_node_position <= end) {
    result->insert(current_node->start_marker_ids.begin(), current_node->start_marker_ids.end());
    CacheNodePosition();
    MoveToSuccessor();
  }
}

void Iterator::FindEndingIn(const Point &start, const Point &end, std::unordered_set<MarkerId> *result) {
  Reset();

  if (!current_node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (current_node && current_node_position <= end) {
    result->insert(current_node->end_marker_ids.begin(), current_node->end_marker_ids.end());
    CacheNodePosition();
    MoveToSuccessor();
  }
}

unordered_map<MarkerId, Range> Iterator::Dump() {
  Reset();

  unordered_map<MarkerId, Range> snapshot;

  if (!current_node) return snapshot;

  while (current_node && current_node->left) {
    CacheNodePosition();
    DescendLeft();
  }

  while (current_node) {
    for (MarkerId id : current_node->start_marker_ids) {
      Range range;
      range.start = current_node_position;
      snapshot.insert({id, range});
    }

    for (MarkerId id : current_node->end_marker_ids) {
      snapshot[id].end = current_node_position;
    }

    CacheNodePosition();
    MoveToSuccessor();
  }

  return snapshot;
}

void Iterator::Ascend() {
  if (current_node->parent) {
    if (current_node->parent->left == current_node) {
      current_node_position = right_ancestor_position;
    } else {
      current_node_position = left_ancestor_position;
    }
    left_ancestor_position = left_ancestor_position_stack.back();
    left_ancestor_position_stack.pop_back();
    right_ancestor_position = right_ancestor_position_stack.back();
    right_ancestor_position_stack.pop_back();
    current_node = current_node->parent;
  } else {
    current_node = nullptr;
    current_node_position = Point();
    left_ancestor_position = Point();
    right_ancestor_position = Point::Max();
  }
}

void Iterator::DescendLeft() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  right_ancestor_position = current_node_position;
  current_node = current_node->left;
  current_node_position = left_ancestor_position.Traverse(current_node->left_extent);
}

void Iterator::DescendRight() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  left_ancestor_position = current_node_position;
  current_node = current_node->right;
  current_node_position = left_ancestor_position.Traverse(current_node->left_extent);
}

void Iterator::MoveToSuccessor() {
  if (!current_node) return;

  if (current_node->right) {
    DescendRight();
    while (current_node->left) {
      DescendLeft();
    }
  } else {
    while (current_node->parent && current_node->parent->right == current_node) {
      Ascend();
    }
    Ascend();
  }
}

void Iterator::SeekToFirstNodeGreaterThanOrEqualTo(const Point &position) {
  while (true) {
    CacheNodePosition();
    if (position == current_node_position) {
      break;
    } else if (position < current_node_position) {
      if (current_node->left) {
        DescendLeft();
      } else {
        break;
      }
    } else { // position > current_node_position
      if (current_node->right) {
        DescendRight();
      } else {
        break;
      }
    }
  }

  if (current_node_position < position) MoveToSuccessor();
}

void Iterator::MarkRight(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (left_ancestor_position < start_position
    && start_position <= current_node_position
    && right_ancestor_position <= end_position) {
    current_node->right_marker_ids.insert(id);
  }
}

void Iterator::MarkLeft(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (!current_node_position.IsZero() && start_position <= left_ancestor_position && current_node_position <= end_position) {
    current_node->left_marker_ids.insert(id);
  }
}

Node* Iterator::InsertLeftChild(const Point &position) {
  return current_node->left = new Node(current_node, position.Traversal(left_ancestor_position));
}

Node* Iterator::InsertRightChild(const Point &position) {
  return current_node->right = new Node(current_node, position.Traversal(current_node_position));
}

void Iterator::CheckIntersection(const Point &start, const Point &end, unordered_set<MarkerId> *result) {
  if (left_ancestor_position <= end && start <= current_node_position) {
    result->insert(current_node->left_marker_ids.begin(), current_node->left_marker_ids.end());
  }

  if (start <= current_node_position && current_node_position <= end) {
    result->insert(current_node->start_marker_ids.begin(), current_node->start_marker_ids.end());
    result->insert(current_node->end_marker_ids.begin(), current_node->end_marker_ids.end());
  }

  if (current_node_position <= end && start <= right_ancestor_position) {
    result->insert(current_node->right_marker_ids.begin(), current_node->right_marker_ids.end());
  }
}

void Iterator::CacheNodePosition() const {
  marker_index->node_position_cache.insert({current_node, current_node_position});
}
