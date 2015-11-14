#include "marker-index.h"
#include <algorithm>
#include <climits>
#include <iterator>
#include <random>
#include <set>
#include <stdlib.h>

using std::set;
using std::default_random_engine;

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

Point MarkerIndex::GetStart(MarkerId id) const {
  auto result = start_nodes_by_id.find(id);
  if (result == start_nodes_by_id.end())
    return Point();
  else
    return GetNodeOffset(result->second);
}

Point MarkerIndex::GetEnd(MarkerId id) const {
  auto result = end_nodes_by_id.find(id);
  if (result == end_nodes_by_id.end())
    return Point();
  else
    return GetNodeOffset(result->second);
}

set<MarkerId> MarkerIndex::FindIntersecting(Point start, Point end) {
  set<MarkerId> result;
  iterator.FindIntersecting(start, end, &result);
  return result;
}

set<MarkerId> MarkerIndex::FindContaining(Point start, Point end) {
  set<MarkerId> containing_start;
  iterator.FindIntersecting(start, start, &containing_start);
  if (end == start) {
    return containing_start;
  } else {
    set<MarkerId> containing_end;
    iterator.FindIntersecting(end, end, &containing_end);
    set<MarkerId> containing_start_and_end;
    std::set_intersection(containing_start.begin(), containing_start.end(),
                         containing_end.begin(), containing_end.end(),
                         std::inserter(containing_start_and_end, containing_start_and_end.begin()));
    return containing_start_and_end;
  }
}

set<MarkerId> MarkerIndex::FindContainedIn(Point start, Point end) {
  set<MarkerId> result;
  iterator.FindContainedIn(start, end, &result);
  return result;
}

set<MarkerId> MarkerIndex::FindStartingIn(Point start, Point end) {
  set<MarkerId> result;
  iterator.FindStartingIn(start, end, &result);
  return result;
}

set<MarkerId> MarkerIndex::FindEndingIn(Point start, Point end) {
  set<MarkerId> result;
  iterator.FindEndingIn(start, end, &result);
  return result;
}

Point MarkerIndex::GetNodeOffset(const Node *node) const {
  Point offset = node->left_extent;
  while (node->parent) {
    if (node->parent->right == node) {
      offset = node->parent->left_extent.Traverse(offset);
    }

    node = node->parent;
  }
  return offset;
}

void MarkerIndex::DeleteNode(Node *node) {
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
      assert(!node->left);
      assert(!node->right);
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
      rotation_pivot->left_marker_ids.erase(it++);
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
      rotation_pivot->right_marker_ids.erase(it++);
    }
  }
}
