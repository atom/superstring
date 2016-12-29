#include "marker-index.h"
#include <climits>
#include <iterator>
#include <random>
#include <stdlib.h>
#include "range.h"

using std::default_random_engine;
using std::unordered_map;

MarkerIndex::Node::Node(Node *parent, Point left_extent) :
  parent {parent},
  left {nullptr},
  right {nullptr},
  left_extent {left_extent},
  priority {0} {}

bool MarkerIndex::Node::IsMarkerEndpoint() {
  return (start_marker_ids.size() + end_marker_ids.size()) > 0;
}

MarkerIndex::Iterator::Iterator(MarkerIndex *marker_index) :
  marker_index {marker_index},
  current_node {nullptr} {}

void MarkerIndex::Iterator::Reset() {
  current_node = marker_index->root;
  if (current_node) {
    current_node_position = current_node->left_extent;
  }
  left_ancestor_position = Point(0, 0);
  right_ancestor_position = Point(UINT32_MAX, UINT32_MAX);
  left_ancestor_position_stack.clear();
  right_ancestor_position_stack.clear();
}

MarkerIndex::Node *MarkerIndex::Iterator::InsertMarkerStart(const MarkerId &id, const Point &start_position, const Point &end_position) {
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

MarkerIndex::Node *MarkerIndex::Iterator::InsertMarkerEnd(const MarkerId &id, const Point &start_position, const Point &end_position) {
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

MarkerIndex::Node *MarkerIndex::Iterator::InsertSpliceBoundary(const Point &position, bool is_insertion_end) {
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

void MarkerIndex::Iterator::FindIntersecting(const Point &start, const Point &end, MarkerIdSet *result) {
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

void MarkerIndex::Iterator::FindContainedIn(const Point &start, const Point &end, MarkerIdSet *result) {
  Reset();

  if (!current_node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  MarkerIdSet started;
  while (current_node && current_node_position <= end) {
    started.insert(current_node->start_marker_ids.begin(), current_node->start_marker_ids.end());
    for (MarkerId id : current_node->end_marker_ids) {
      if (started.count(id) > 0) result->insert(id);
    }
    CacheNodePosition();
    MoveToSuccessor();
  }
}

void MarkerIndex::Iterator::FindStartingIn(const Point &start, const Point &end, MarkerIdSet *result) {
  Reset();

  if (!current_node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (current_node && current_node_position <= end) {
    result->insert(current_node->start_marker_ids.begin(), current_node->start_marker_ids.end());
    CacheNodePosition();
    MoveToSuccessor();
  }
}

void MarkerIndex::Iterator::FindEndingIn(const Point &start, const Point &end, MarkerIdSet *result) {
  Reset();

  if (!current_node) return;

  SeekToFirstNodeGreaterThanOrEqualTo(start);

  while (current_node && current_node_position <= end) {
    result->insert(current_node->end_marker_ids.begin(), current_node->end_marker_ids.end());
    CacheNodePosition();
    MoveToSuccessor();
  }
}

unordered_map<MarkerIndex::MarkerId, Range> MarkerIndex::Iterator::Dump() {
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

void MarkerIndex::Iterator::Ascend() {
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
    right_ancestor_position = Point(UINT32_MAX, UINT32_MAX);
  }
}

void MarkerIndex::Iterator::DescendLeft() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  right_ancestor_position = current_node_position;
  current_node = current_node->left;
  current_node_position = left_ancestor_position.Traverse(current_node->left_extent);
}

void MarkerIndex::Iterator::DescendRight() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  left_ancestor_position = current_node_position;
  current_node = current_node->right;
  current_node_position = left_ancestor_position.Traverse(current_node->left_extent);
}

void MarkerIndex::Iterator::MoveToSuccessor() {
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

void MarkerIndex::Iterator::SeekToFirstNodeGreaterThanOrEqualTo(const Point &position) {
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

void MarkerIndex::Iterator::MarkRight(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (left_ancestor_position < start_position
    && start_position <= current_node_position
    && right_ancestor_position <= end_position) {
    current_node->right_marker_ids.insert(id);
  }
}

void MarkerIndex::Iterator::MarkLeft(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (!current_node_position.IsZero() && start_position <= left_ancestor_position && current_node_position <= end_position) {
    current_node->left_marker_ids.insert(id);
  }
}

MarkerIndex::Node *MarkerIndex::Iterator::InsertLeftChild(const Point &position) {
  return current_node->left = new Node(current_node, position.Traversal(left_ancestor_position));
}

MarkerIndex::Node *MarkerIndex::Iterator::InsertRightChild(const Point &position) {
  return current_node->right = new Node(current_node, position.Traversal(current_node_position));
}

void MarkerIndex::Iterator::CheckIntersection(const Point &start, const Point &end, MarkerIdSet *result) {
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

void MarkerIndex::Iterator::CacheNodePosition() const {
  marker_index->node_position_cache.insert({current_node, current_node_position});
}

MarkerIndex::MarkerIndex(unsigned seed)
  : random_engine {static_cast<default_random_engine::result_type>(seed)},
    random_distribution{1, INT_MAX - 1},
    root {nullptr},
    iterator {this} {}

int MarkerIndex::GenerateRandomNumber() {
  return random_distribution(random_engine);
}

void MarkerIndex::Insert(MarkerId id, Point start, Point end) {
  Node *start_node = iterator.InsertMarkerStart(id, start, end);
  Node *end_node = iterator.InsertMarkerEnd(id, start, end);

  node_position_cache.insert({start_node, start});
  node_position_cache.insert({end_node, end});

  start_node->start_marker_ids.insert(id);
  end_node->end_marker_ids.insert(id);

  if (start_node->priority == 0) {
    start_node->priority = GenerateRandomNumber();
    BubbleNodeUp(start_node);
  }

  if (end_node->priority == 0) {
    end_node->priority = GenerateRandomNumber();
    BubbleNodeUp(end_node);
  }

  start_nodes_by_id.insert({id, start_node});
  end_nodes_by_id.insert({id, end_node});
}

void MarkerIndex::SetExclusive(MarkerId id, bool exclusive) {
  if (exclusive) {
    exclusive_marker_ids.insert(id);
  } else {
    exclusive_marker_ids.erase(id);
  }
}

void MarkerIndex::Delete(MarkerId id) {
  Node *start_node = start_nodes_by_id.find(id)->second;
  Node *end_node = end_nodes_by_id.find(id)->second;

  Node *node = start_node;
  while (node) {
    node->right_marker_ids.erase(id);
    node = node->parent;
  }

  node = end_node;
  while (node) {
    node->left_marker_ids.erase(id);
    node = node->parent;
  }

  start_node->start_marker_ids.erase(id);
  end_node->end_marker_ids.erase(id);

  if (!start_node->IsMarkerEndpoint()) {
    DeleteNode(start_node);
  }

  if (end_node != start_node && !end_node->IsMarkerEndpoint()) {
    DeleteNode(end_node);
  }

  start_nodes_by_id.erase(id);
  end_nodes_by_id.erase(id);
}

MarkerIndex::SpliceResult MarkerIndex::Splice(Point start, Point old_extent, Point new_extent) {
  node_position_cache.clear();

  SpliceResult invalidated;

  if (!root || (old_extent.IsZero() && new_extent.IsZero())) return invalidated;

  bool is_insertion = old_extent.IsZero();
  Node *start_node = iterator.InsertSpliceBoundary(start, false);
  Node *end_node = iterator.InsertSpliceBoundary(start.Traverse(old_extent), is_insertion);

  start_node->priority = -1;
  BubbleNodeUp(start_node);
  end_node->priority = -2;
  BubbleNodeUp(end_node);

  MarkerIdSet starting_inside_splice;
  MarkerIdSet ending_inside_splice;

  if (is_insertion) {
    for (auto iter = start_node->start_marker_ids.begin(); iter != start_node->start_marker_ids.end();) {
      MarkerId id = *iter;
      if (exclusive_marker_ids.count(id) > 0) {
        iter = start_node->start_marker_ids.erase(iter);
        start_node->right_marker_ids.erase(id);
        end_node->start_marker_ids.insert(id);
        start_nodes_by_id[id] = end_node;
      } else {
        ++iter;
      }
    }
    for (auto iter = start_node->end_marker_ids.begin(); iter != start_node->end_marker_ids.end();) {
      MarkerId id = *iter;
      if (exclusive_marker_ids.count(id) == 0 || end_node->start_marker_ids.count(id) > 0) {
        iter = start_node->end_marker_ids.erase(iter);
        if (end_node->start_marker_ids.count(id) == 0) {
          start_node->right_marker_ids.insert(id);
        }
        end_node->end_marker_ids.insert(id);
        end_nodes_by_id[id] = end_node;
      } else {
        ++iter;
      }
    }
  } else {
    GetStartingAndEndingMarkersWithinSubtree(start_node->right, &starting_inside_splice, &ending_inside_splice);

    for (MarkerId id : ending_inside_splice) {
      end_node->end_marker_ids.insert(id);
      if (!starting_inside_splice.count(id)) {
        start_node->right_marker_ids.insert(id);
      }
      end_nodes_by_id[id] = end_node;
    }

    for (MarkerId id : end_node->end_marker_ids) {
      if (exclusive_marker_ids.count(id) && !end_node->start_marker_ids.count(id)) {
        ending_inside_splice.insert(id);
      }
    }

    for (MarkerId id : starting_inside_splice) {
      end_node->start_marker_ids.insert(id);
      start_nodes_by_id[id] = end_node;
    }

    for (auto iter = start_node->start_marker_ids.begin(); iter != start_node->start_marker_ids.end();) {
      MarkerId id = *iter;
      if (exclusive_marker_ids.count(id) && !start_node->end_marker_ids.count(id)) {
        iter = start_node->start_marker_ids.erase(iter);
        start_node->right_marker_ids.erase(id);
        end_node->start_marker_ids.insert(id);
        start_nodes_by_id[id] = end_node;
        starting_inside_splice.insert(id);
      } else {
        ++iter;
      }
    }
  }

  PopulateSpliceInvalidationSets(&invalidated, start_node, end_node, starting_inside_splice, ending_inside_splice);

  if (start_node->right) {
    DeleteSubtree(start_node->right);
    start_node->right = nullptr;
  }

  end_node->left_extent = start.Traverse(new_extent);

  if (start_node->left_extent == end_node->left_extent) {
    for (MarkerId id : end_node->start_marker_ids) {
      start_node->start_marker_ids.insert(id);
      start_node->right_marker_ids.insert(id);
      start_nodes_by_id[id] = start_node;
    }

    for (MarkerId id : end_node->end_marker_ids) {
      start_node->end_marker_ids.insert(id);
      if (end_node->left_marker_ids.count(id) > 0) {
        start_node->left_marker_ids.insert(id);
        end_node->left_marker_ids.erase(id);
      }
      end_nodes_by_id[id] = start_node;
    }
    DeleteNode(end_node);
  } else if (end_node->IsMarkerEndpoint()) {
    end_node->priority = GenerateRandomNumber();
    BubbleNodeDown(end_node);
  } else {
    DeleteNode(end_node);
  }

  if (start_node->IsMarkerEndpoint()) {
    start_node->priority = GenerateRandomNumber();
    BubbleNodeDown(start_node);
  } else {
    DeleteNode(start_node);
  }

  return invalidated;
}

Point MarkerIndex::GetStart(MarkerId id) const {
  auto result = start_nodes_by_id.find(id);
  if (result == start_nodes_by_id.end())
    return Point();
  else
    return GetNodePosition(result->second);
}

Point MarkerIndex::GetEnd(MarkerId id) const {
  auto result = end_nodes_by_id.find(id);
  if (result == end_nodes_by_id.end())
    return Point();
  else
    return GetNodePosition(result->second);
}

Range MarkerIndex::GetRange(MarkerId id) const {
  return Range{GetStart(id), GetEnd(id)};
}

int MarkerIndex::Compare(MarkerId id1, MarkerId id2) const {
  switch (GetStart(id1).Compare(GetStart(id2))) {
    case -1:
      return -1;
    case 1:
      return 1;
    default:
      return GetEnd(id2).Compare(GetEnd(id1));
  }
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::FindIntersecting(Point start, Point end) {
  MarkerIdSet result;
  iterator.FindIntersecting(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::FindContaining(Point start, Point end) {
  MarkerIdSet containing_start;
  iterator.FindIntersecting(start, start, &containing_start);
  if (end == start) {
    return containing_start;
  } else {
    MarkerIdSet containing_end;
    iterator.FindIntersecting(end, end, &containing_end);
    MarkerIdSet containing_start_and_end;
    for (MarkerId id : containing_start) {
      if (containing_end.count(id) > 0) {
        containing_start_and_end.insert(id);
      }
    }
    return containing_start_and_end;
  }
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::FindContainedIn(Point start, Point end) {
  MarkerIdSet result;
  iterator.FindContainedIn(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::FindStartingIn(Point start, Point end) {
  MarkerIdSet result;
  iterator.FindStartingIn(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::FindStartingAt(Point position) {
  return FindStartingIn(position, position);
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::FindEndingIn(Point start, Point end) {
  MarkerIdSet result;
  iterator.FindEndingIn(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::FindEndingAt(Point position) {
  return FindEndingIn(position, position);
}

unordered_map<MarkerIndex::MarkerId, Range> MarkerIndex::Dump() {
  return iterator.Dump();
}

Point MarkerIndex::GetNodePosition(const Node *node) const {
  auto cache_entry = node_position_cache.find(node);
  if (cache_entry == node_position_cache.end()) {
    Point position = node->left_extent;
    const Node *current_node = node;
    while (current_node->parent) {
      if (current_node->parent->right == current_node) {
        position = current_node->parent->left_extent.Traverse(position);
      }

      current_node = current_node->parent;
    }
    node_position_cache.insert({node, position});
    return position;
  } else {
    return cache_entry->second;
  }
}

void MarkerIndex::DeleteNode(Node *node) {
  node_position_cache.erase(node);
  node->priority = INT_MAX;

  BubbleNodeDown(node);

  if (node->parent) {
    if (node->parent->left == node) {
      node->parent->left = nullptr;
    } else {
      node->parent->right = nullptr;
    }
  } else {
    root = nullptr;
  }

  delete node;
}

void MarkerIndex::DeleteSubtree(Node *node) {
  if (node->left) DeleteSubtree(node->left);
  if (node->right) DeleteSubtree(node->right);
  delete node;
}

void MarkerIndex::BubbleNodeUp(Node *node) {
  while (node->parent && node->priority < node->parent->priority) {
    if (node == node->parent->left) {
      RotateNodeRight(node);
    } else {
      RotateNodeLeft(node);
    }
  }
}

void MarkerIndex::BubbleNodeDown(Node *node) {
  while (true) {
    int left_child_priority = (node->left) ? node->left->priority : INT_MAX;
    int right_child_priority = (node->right) ? node->right->priority : INT_MAX;

    if (left_child_priority < right_child_priority && left_child_priority < node->priority) {
      RotateNodeRight(node->left);
    } else if (right_child_priority < node->priority) {
      RotateNodeLeft(node->right);
    } else {
      break;
    }
  }
}

void MarkerIndex::RotateNodeLeft(Node *rotation_pivot) {
  Node *rotation_root = rotation_pivot->parent;

  if (rotation_root->parent) {
    if (rotation_root->parent->left == rotation_root) {
      rotation_root->parent->left = rotation_pivot;
    } else {
      rotation_root->parent->right = rotation_pivot;
    }
  } else {
    root = rotation_pivot;
  }
  rotation_pivot->parent = rotation_root->parent;

  rotation_root->right = rotation_pivot->left;
  if (rotation_root->right) {
    rotation_root->right->parent = rotation_root;
  }

  rotation_pivot->left = rotation_root;
  rotation_root->parent = rotation_pivot;

  rotation_pivot->left_extent = rotation_root->left_extent.Traverse(rotation_pivot->left_extent);

  rotation_pivot->right_marker_ids.insert(rotation_root->right_marker_ids.begin(), rotation_root->right_marker_ids.end());

  for (auto it = rotation_pivot->left_marker_ids.begin(); it != rotation_pivot->left_marker_ids.end();) {
    if (rotation_root->left_marker_ids.count(*it)) {
      rotation_root->left_marker_ids.erase(*it);
      ++it;
    } else {
      rotation_root->right_marker_ids.insert(*it);
      it = rotation_pivot->left_marker_ids.erase(it);
    }
  }
}

void MarkerIndex::RotateNodeRight(Node *rotation_pivot) {
  Node *rotation_root = rotation_pivot->parent;

  if (rotation_root->parent) {
    if (rotation_root->parent->left == rotation_root) {
      rotation_root->parent->left = rotation_pivot;
    } else {
      rotation_root->parent->right = rotation_pivot;
    }
  } else {
    root = rotation_pivot;
  }
  rotation_pivot->parent = rotation_root->parent;

  rotation_root->left = rotation_pivot->right;
  if (rotation_root->left) {
    rotation_root->left->parent = rotation_root;
  }

  rotation_pivot->right = rotation_root;
  rotation_root->parent = rotation_pivot;

  rotation_root->left_extent = rotation_root->left_extent.Traversal(rotation_pivot->left_extent);

  for (auto it = rotation_root->left_marker_ids.begin(); it != rotation_root->left_marker_ids.end(); ++it) {
    if (!rotation_pivot->start_marker_ids.count(*it)) {
      rotation_pivot->left_marker_ids.insert(*it);
    }
  }

  for (auto it = rotation_pivot->right_marker_ids.begin(); it != rotation_pivot->right_marker_ids.end();) {
    if (rotation_root->right_marker_ids.count(*it)) {
      rotation_root->right_marker_ids.erase(*it);
      ++it;
    } else {
      rotation_root->left_marker_ids.insert(*it);
      it = rotation_pivot->right_marker_ids.erase(it);
    }
  }
}

void MarkerIndex::GetStartingAndEndingMarkersWithinSubtree(const Node *node, MarkerIdSet *starting, MarkerIdSet *ending) {
  if (node == nullptr) {
    return;
  }

  GetStartingAndEndingMarkersWithinSubtree(node->left, starting, ending);
  starting->insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
  ending->insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
  GetStartingAndEndingMarkersWithinSubtree(node->right, starting, ending);
}

void MarkerIndex::PopulateSpliceInvalidationSets(SpliceResult *invalidated, const Node *start_node, const Node *end_node, const MarkerIdSet &starting_inside_splice, const MarkerIdSet &ending_inside_splice) {
  invalidated->touch.insert(start_node->end_marker_ids.begin(), start_node->end_marker_ids.end());
  invalidated->touch.insert(end_node->start_marker_ids.begin(), end_node->start_marker_ids.end());

  for (MarkerId id : start_node->right_marker_ids) {
    invalidated->touch.insert(id);
    invalidated->inside.insert(id);
  }

  for (MarkerId id : end_node->left_marker_ids) {
    invalidated->touch.insert(id);
    invalidated->inside.insert(id);
  }

  for (MarkerId id : starting_inside_splice) {
    invalidated->touch.insert(id);
    invalidated->inside.insert(id);
    invalidated->overlap.insert(id);
    if (ending_inside_splice.count(id)) invalidated->surround.insert(id);
  }

  for (MarkerId id : ending_inside_splice) {
    invalidated->touch.insert(id);
    invalidated->inside.insert(id);
    invalidated->overlap.insert(id);
  }
}
