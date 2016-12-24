#include "marker-index.h"
#include <assert.h>
#include <climits>
#include <iterator>
#include <random>
#include <sstream>
#include <stdlib.h>
#include "range.h"

#include <iostream>

using std::default_random_engine;
using std::unordered_map;
using std::unordered_set;
using std::endl;

MarkerIndex::Node::Node(Node *parent, Point distance_from_left_ancestor) :
  parent {parent},
  left {nullptr},
  right {nullptr},
  distance_from_left_ancestor {distance_from_left_ancestor} {}

bool MarkerIndex::Node::IsMarkerEndpoint() {
  return (start_marker_ids.size() + end_marker_ids.size()) > 0;
}

void MarkerIndex::Node::WriteDotGraph(std::stringstream &result, Point left_ancestor_position) {
  Point position = left_ancestor_position.Traverse(distance_from_left_ancestor);

  result << "node_" << this << " [shape=box label=\"position: " << position;

  result << "\\n left: ";
  for (auto id : left_marker_ids) result << id << " ";
  result << "\\n end: ";
  for (auto id : end_marker_ids) result << id << " ";
  result << "\\n start: ";
  for (auto id : start_marker_ids) result << id << " ";
  result << "\\n right: ";
  for (auto id : right_marker_ids) result << id << " ";
  result << "\"]" << endl;
  result << "node_" << this << " -> ";
  if (left) {
    result << "node_" << left << endl;
    left->WriteDotGraph(result, left_ancestor_position);
  } else {
    result << "node_" << this << "_left_null" << endl;
    result << "node_" << this << "_left_null [shape=point]" << endl;
  }

  result << "node_" << this << " -> ";
  if (right) {
    result << "node_" << right << endl;
    right->WriteDotGraph(result, position);
  } else {
    result << "node_" << this << "_right_null" << endl;
    result << "node_" << this << "_right_null [shape=point]" << endl;
  }
}

MarkerIndex::MarkerIndex() : root{nullptr} {}

void MarkerIndex::Insert(MarkerId id, Point start, Point end) {
  Node *start_node = InsertNode(start);
  Node *end_node = InsertNode(end);

  start_node->start_marker_ids.insert(id);
  end_node->end_marker_ids.insert(id);

  if (start_node != end_node) {
    if (start_node != end_node->left) {
      RotateNodeRight(start_node);
    }
    start_node->right_marker_ids.insert(id);
  }

  node_position_cache.insert({start_node, start});
  node_position_cache.insert({end_node, end});
  start_nodes_by_id.insert({id, start_node});
  end_nodes_by_id.insert({id, end_node});
}

void MarkerIndex::Delete(MarkerId id) {
  Node *start_node = start_nodes_by_id.find(id)->second;
  Node *end_node = end_nodes_by_id.find(id)->second;

  SplayNode(start_node);
  SplayNode(end_node);
  if (start_node != end_node && start_node != end_node->left) {
    RotateNodeRight(start_node);
  }

  start_node->start_marker_ids.erase(id);
  start_node->right_marker_ids.erase(id);
  end_node->end_marker_ids.erase(id);

  if (!end_node->IsMarkerEndpoint()) {
    node_position_cache.erase(end_node);
    DeleteSingleNode(end_node);
  }
  if (!start_node->IsMarkerEndpoint() && start_node != end_node) {
    node_position_cache.erase(start_node);
    DeleteSingleNode(start_node);
  }

  start_nodes_by_id.erase(id);
  end_nodes_by_id.erase(id);
}

void MarkerIndex::SetExclusive(MarkerId id, bool exclusive) {
  if (exclusive) {
    exclusive_marker_ids.insert(id);
  } else {
    exclusive_marker_ids.erase(id);
  }
}

MarkerIndex::SpliceResult MarkerIndex::Splice(Point start, Point deleted_extent, Point inserted_extent) {
  SpliceResult invalidated;

  if (!root || (deleted_extent.IsZero() && inserted_extent.IsZero())) return invalidated;

  node_position_cache.clear();

  Node *start_node = InsertNode(start);
  Node *end_node = InsertNode(start.Traverse(deleted_extent), !deleted_extent.IsZero());
  if (start_node != end_node->left) {
    RotateNodeRight(start_node);
  }

  unordered_set<MarkerId> starting_inside_splice;
  unordered_set<MarkerId> ending_inside_splice;

  if (deleted_extent.IsZero()) {
    for (auto iter = start_node->start_marker_ids.begin(); iter != start_node->start_marker_ids.end();) {
      MarkerId id = *iter;
      if (exclusive_marker_ids.count(id) > 0) {
        start_node->start_marker_ids.erase(iter++);
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
        start_node->end_marker_ids.erase(iter++);
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
        start_node->start_marker_ids.erase(iter++);
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

  DeleteSubtree(&start_node->right);
  end_node->distance_from_left_ancestor = start.Traverse(inserted_extent);

  if (inserted_extent.IsZero()) {
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
    DeleteSingleNode(root);
  } else if (!end_node->IsMarkerEndpoint()) {
    DeleteSingleNode(root);
  }

  if (!start_node->IsMarkerEndpoint()) {
    DeleteSingleNode(start_node);
  }

  return invalidated;
}

Point MarkerIndex::GetStart(MarkerId id) {
  auto result = start_nodes_by_id.find(id);
  if (result == start_nodes_by_id.end())
    return Point();
  else
    return GetNodePosition(result->second);
}

Point MarkerIndex::GetEnd(MarkerId id) {
  auto result = end_nodes_by_id.find(id);
  if (result == end_nodes_by_id.end())
    return Point();
  else
    return GetNodePosition(result->second);
}

Range MarkerIndex::GetRange(MarkerId id) {
  return Range{GetStart(id), GetEnd(id)};
}

int MarkerIndex::Compare(MarkerId id1, MarkerId id2) {
  switch (GetStart(id1).Compare(GetStart(id2))) {
    case -1:
      return -1;
    case 1:
      return 1;
    default:
      return GetEnd(id2).Compare(GetEnd(id1));
  }
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindIntersecting(Point start, Point end) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *start_node = SplayGreatestLowerBound(start);
  Node *end_node = SplayLeastUpperBound(end);

  Node *contained_subtree {};
  if (start_node) {
    if (end_node) {
      if (start_node != end_node->left) RotateNodeRight(start_node);
      result.insert(start_node->right_marker_ids.begin(), start_node->right_marker_ids.end());
    }
    contained_subtree = start_node->right;
  } else if (end_node) {
    contained_subtree = end_node->left;
  } else {
    contained_subtree = root;
  }

  if (contained_subtree) {
    node_stack.clear();
    node_stack.push_back(contained_subtree);
    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left) node_stack.push_back(node->left);
      if (node->right) node_stack.push_back(node->right);
      result.insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
      result.insert(node->end_marker_ids.begin(), node->start_marker_ids.end());
    }
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindContaining(Point start, Point end) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *start_node = SplayGreatestLowerBound(start, true);
  Node *end_node = SplayLeastUpperBound(end, true);

  if (start_node && end_node) {
    if (start_node == end_node) {
      result.insert(start_node->start_marker_ids.begin(), start_node->start_marker_ids.end());
      result.insert(start_node->end_marker_ids.begin(), start_node->end_marker_ids.end());
      Node *start_node = SplayGreatestLowerBound(start, false);
      Node *end_node = SplayLeastUpperBound(end, false);
      if (start_node && end_node) {
        result.insert(start_node->right_marker_ids.begin(), start_node->right_marker_ids.end());
      }
    } else {
      if (start_node != end_node->left) RotateNodeRight(start_node);
      result.insert(start_node->right_marker_ids.begin(), start_node->right_marker_ids.end());
    }
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindContainedIn(Point start, Point end) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *start_node = SplayGreatestLowerBound(start);
  Node *end_node = SplayLeastUpperBound(end);
  Node *contained_subtree {};

  if (start_node) {
    if (end_node && start_node != end_node->left) RotateNodeRight(start_node);
    contained_subtree = start_node->right;
  } else if (end_node) {
    contained_subtree = end_node->left;
  } else {
    contained_subtree = root;
  }

  if (contained_subtree) {
    unordered_set<MarkerId> starting;
    unordered_set<MarkerId> ending;
    node_stack.clear();
    node_stack.push_back(contained_subtree);
    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left) node_stack.push_back(node->left);
      if (node->right) node_stack.push_back(node->right);
      starting.insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
      for (auto id : node->start_marker_ids) {
        if (ending.count(id) > 0) {
          result.insert(id);
        } else {
          starting.insert(id);
        }
      }

      for (auto id : node->end_marker_ids) {
        if (starting.count(id) > 0) {
          result.insert(id);
        } else {
          ending.insert(id);
        }
      }
    }
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindStartingIn(Point start, Point end) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *start_node = SplayGreatestLowerBound(start);
  Node *end_node = SplayLeastUpperBound(end);
  Node *contained_subtree {};

  if (start_node) {
    if (end_node && start_node != end_node->left) RotateNodeRight(start_node);
    contained_subtree = start_node->right;
  } else if (end_node) {
    contained_subtree = end_node->left;
  } else {
    contained_subtree = root;
  }

  if (contained_subtree) {
    node_stack.clear();
    node_stack.push_back(contained_subtree);
    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left) node_stack.push_back(node->left);
      if (node->right) node_stack.push_back(node->right);
      result.insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    }
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindStartingAt(Point position) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *node = SplayGreatestLowerBound(position, true);
  if (node && node->distance_from_left_ancestor == position) {
    result.insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindEndingIn(Point start, Point end) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *start_node = SplayGreatestLowerBound(start);
  Node *end_node = SplayLeastUpperBound(end);
  Node *contained_subtree {};

  if (start_node) {
    if (end_node && start_node != end_node->left) RotateNodeRight(start_node);
    contained_subtree = start_node->right;
  } else if (end_node) {
    contained_subtree = end_node->left;
  } else {
    contained_subtree = root;
  }

  if (contained_subtree) {
    node_stack.clear();
    node_stack.push_back(contained_subtree);
    while (!node_stack.empty()) {
      Node *node = node_stack.back();
      node_stack.pop_back();
      if (node->left) node_stack.push_back(node->left);
      if (node->right) node_stack.push_back(node->right);
      result.insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
    }
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindEndingAt(Point position) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *node = SplayGreatestLowerBound(position, true);
  if (node && node->distance_from_left_ancestor == position) {
    result.insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
  }

  return result;
}

unordered_map<MarkerIndex::MarkerId, Range> MarkerIndex::Dump() {
  unordered_map<MarkerIndex::MarkerId, Range> result;
  return result;
}

std::string MarkerIndex::GetDotGraph() {
  std::stringstream result;
  result << "digraph marker_index {" << endl;
  if (root) root->WriteDotGraph(result, Point::Zero());
  result << "}" << endl;
  return result.str();
}

Point MarkerIndex::GetNodePosition(Node *node) {
  auto cache_entry = node_position_cache.find(node);
  if (cache_entry == node_position_cache.end()) {
    SplayNode(node);
    return node->distance_from_left_ancestor;
  } else {
    return cache_entry->second;
  }
}

MarkerIndex::Node *MarkerIndex::InsertNode(Point target_position, bool return_existing) {
  if (!root) {
    root = BuildNode(nullptr, target_position);
    return root;
  }

  Node *node = root;
  node_stack.clear();
  Point left_ancestor_position = Point::Zero();

  while (true) {
    Point position = left_ancestor_position.Traverse(node->distance_from_left_ancestor);
    if (position == target_position && return_existing) {
      break;
    } else {
      node_stack.push_back(node);
      if (position <= target_position) {
        if (node->right) {
          left_ancestor_position = position;
          node = node->right;
        } else {
          node->right = BuildNode(node, target_position.Traversal(position));
          node = node->right;
          break;
        }
      } else {
        if (node->left) {
          node = node->left;
        } else {
          node->left = BuildNode(node, target_position.Traversal(left_ancestor_position));
          node = node->left;
          break;
        }
      }
    }
  }

  SplayNode(node);
  return node;
}

MarkerIndex::Node *MarkerIndex::SplayGreatestLowerBound(Point target_position, bool inclusive) {
  if (!root) return nullptr;

  Node *greatest_lower_bound {};
  Node *node = root;
  Point left_ancestor_position;
  while (true) {
    Point node_position = left_ancestor_position.Traverse(node->distance_from_left_ancestor);
    node_position_cache.insert({node, node_position});
    if (inclusive && node_position == target_position) {
      greatest_lower_bound = node;
      break;
    } else if (node_position < target_position) {
      greatest_lower_bound = node;
      if (node->right) {
        left_ancestor_position = node_position;
        node = node->right;
      } else {
        break;
      }
    } else if (node_position >= target_position) {
      if (node->left) {
        node = node->left;
      } else {
        break;
      }
    }
  }

  if (greatest_lower_bound) SplayNode(greatest_lower_bound);
  return greatest_lower_bound;
}

MarkerIndex::Node *MarkerIndex::SplayLeastUpperBound(Point target_position, bool inclusive) {
  if (!root) return nullptr;

  Node *least_upper_bound {};
  Node *node = root;
  Point left_ancestor_position;
  while (true) {
    Point node_position = left_ancestor_position.Traverse(node->distance_from_left_ancestor);
    node_position_cache.insert({node, node_position});
    if (inclusive && node_position == target_position) {
      least_upper_bound = node;
      break;
    } else if (node_position <= target_position) {
      if (node->right) {
        left_ancestor_position = node_position;
        node = node->right;
      } else {
        break;
      }
    } else if (node_position > target_position) {
      least_upper_bound = node;
      if (node->left) {
        node = node->left;
      } else {
        break;
      }
    }
  }

  if (least_upper_bound) SplayNode(least_upper_bound);
  return least_upper_bound;
}

void MarkerIndex::SplayNode(Node *node) {
  Node *parent = node->parent;
  while (parent) {
    Node *grandparent = parent->parent;
    if (grandparent) {
      if (grandparent->left == parent && parent->right == node) {
        RotateNodeLeft(node);
        RotateNodeRight(node);
      } else if (grandparent->right == parent && parent->left == node) {
        RotateNodeRight(node);
        RotateNodeLeft(node);
      } else if (grandparent->left == parent && parent->left == node) {
        RotateNodeRight(parent);
        RotateNodeRight(node);
      } else if (grandparent->right == parent && parent->right == node) {
        RotateNodeLeft(parent);
        RotateNodeLeft(node);
      }
    } else {
      if (parent->left == node) {
        RotateNodeRight(node);
      } else if (parent->right == node) {
        RotateNodeLeft(node);
      } else {
        assert(!"Unexpected state");
      }
    }
    parent = node->parent;
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

  rotation_pivot->distance_from_left_ancestor = rotation_root->distance_from_left_ancestor.Traverse(rotation_pivot->distance_from_left_ancestor);

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

  rotation_root->distance_from_left_ancestor = rotation_root->distance_from_left_ancestor.Traversal(rotation_pivot->distance_from_left_ancestor);

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

void MarkerIndex::DeleteSingleNode(Node *node) {
  while (true) {
    if (node->left) {
      RotateNodeRight(node->left);
    } else if (node->right) {
      RotateNodeLeft(node->right);
    } else if (node->parent) {
      if (node->parent->left == node) {
        DeleteSubtree(&node->parent->left);
        break;
      } else if (node->parent->right == node) {
        DeleteSubtree(&node->parent->right);
        break;
      }
    } else {
      DeleteSubtree(&root);
      break;
    }
  }
}

MarkerIndex::Node *MarkerIndex::BuildNode(Node *parent, Point distance_from_left_ancestor) {
  node_count++;
  return new Node{parent, distance_from_left_ancestor};
}

void MarkerIndex::DeleteSubtree(Node **node_to_delete) {
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
      node_count--;
    }

    *node_to_delete = nullptr;
  }
}

void MarkerIndex::GetStartingAndEndingMarkersWithinSubtree(Node *node, unordered_set<MarkerId> *starting, unordered_set<MarkerId> *ending) {
  if (node == nullptr) {
    return;
  }

  node_stack.clear();
  node_stack.push_back(node);

  while (!node_stack.empty()) {
    Node *node = node_stack.back();
    node_stack.pop_back();
    if (node->left) node_stack.push_back(node->left);
    if (node->right) node_stack.push_back(node->right);
    starting->insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
    ending->insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
  }
}

void MarkerIndex::PopulateSpliceInvalidationSets(SpliceResult *invalidated,
                                                 const Node *start_node, const Node *end_node,
                                                 const unordered_set<MarkerId> &starting_inside_splice,
                                                 const unordered_set<MarkerId> &ending_inside_splice) {
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
