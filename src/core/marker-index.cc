#include "marker-index.h"
#include <algorithm>
#include <assert.h>
#include <climits>
#include <iterator>
#include <random>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include "range.h"

#include <iostream>

using std::default_random_engine;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::endl;

MarkerIndex::Node::Node(Node *parent, Point distance_from_left_ancestor) :
  parent {parent},
  left {nullptr},
  right {nullptr},
  distance_from_left_ancestor {distance_from_left_ancestor} {}

bool MarkerIndex::Node::IsMarkerEndpoint() {
  return (starting_markers.size() + ending_markers.size()) > 0;
}

void MarkerIndex::Node::WriteDotGraph(std::stringstream &result, Point left_ancestor_position) const {
  Point position = left_ancestor_position.Traverse(distance_from_left_ancestor);

  result << "node_" << this << " [shape=box label=\"position: " << position;

  result << "\\n left: ";
  for (auto id : markers_to_left_ancestor) result << id << " ";
  result << "\\n end: ";
  for (auto &&id : ending_markers) result << id << " ";
  result << "\\n start: ";
  for (auto id : starting_markers) result << id << " ";
  result << "\\n right: ";
  for (auto id : markers_to_right_ancestor) result << id << " ";
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

MarkerIndex::MarkerIdSet::MarkerIdSet() : marker_ids{} {};

MarkerIndex::MarkerIdSet::MarkerIdSet(vector<MarkerId> &&marker_ids) : marker_ids{marker_ids} {}

MarkerIndex::MarkerIdSet MarkerIndex::MarkerIdSet::operator +(const MarkerIdSet &other) const {
  vector<MarkerId> result;
  std::set_union(begin(), end(), other.begin(), other.end(), std::back_inserter(result));
  return MarkerIdSet(std::move(result));
}

MarkerIndex::MarkerIdSet MarkerIndex::MarkerIdSet::operator +=(const MarkerIdSet &other) const {
  return *this + other;
}

MarkerIndex::MarkerIdSet MarkerIndex::MarkerIdSet::operator -(const MarkerIdSet &other) const {
  vector<MarkerId> result;
  std::set_difference(begin(), end(), other.begin(), other.end(), std::back_inserter(result));
  return MarkerIdSet(std::move(result));
}

MarkerIndex::MarkerIdSet MarkerIndex::MarkerIdSet::operator -=(const MarkerIdSet &other) const {
  return *this - other;
}

void MarkerIndex::MarkerIdSet::Insert(MarkerId id) {
  marker_ids.insert(std::lower_bound(begin(), end(), id), id);
}

void MarkerIndex::MarkerIdSet::Erase(MarkerId id) {
  auto range = std::equal_range(marker_ids.begin(), marker_ids.end(), id);
  marker_ids.erase(range.first, range.second);
}

bool MarkerIndex::MarkerIdSet::Has(MarkerId id) {
  return std::binary_search(marker_ids.begin(), marker_ids.end(), id);
}

std::vector<MarkerIndex::MarkerId>::size_type MarkerIndex::MarkerIdSet::Size() {
  return marker_ids.size();
}

MarkerIndex::MarkerIdSet::iterator MarkerIndex::MarkerIdSet::begin() {
  return marker_ids.begin();
}

MarkerIndex::MarkerIdSet::iterator MarkerIndex::MarkerIdSet::end() {
  return marker_ids.end();
}

MarkerIndex::MarkerIdSet::const_iterator MarkerIndex::MarkerIdSet::begin() const {
  return marker_ids.cbegin();
}

MarkerIndex::MarkerIdSet::const_iterator MarkerIndex::MarkerIdSet::end() const {
  return marker_ids.cend();
}

MarkerIndex::MarkerIndex() : root{nullptr} {}

void MarkerIndex::Insert(MarkerId id, Point start, Point end) {
  Node *start_node = InsertNode(start);
  Node *end_node = InsertNode(end);

  start_node->starting_markers.Insert(id);
  end_node->ending_markers.Insert(id);

  if (start_node != end_node) {
    if (start_node != end_node->left) {
      RotateNodeRight(start_node);
    }
    start_node->markers_to_right_ancestor.Insert(id);
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

  start_node->starting_markers.Erase(id);
  start_node->markers_to_right_ancestor.Erase(id);
  end_node->ending_markers.Erase(id);

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
    for (auto iter = start_node->starting_markers.begin(); iter != start_node->starting_markers.end();) {
      MarkerId id = *iter;
      if (exclusive_marker_ids.count(id) > 0) {
        start_node->starting_markers.erase(iter++);
        start_node->markers_to_right_ancestor.erase(id);
        end_node->starting_markers.insert(id);
        start_nodes_by_id[id] = end_node;
      } else {
        ++iter;
      }
    }
    for (auto iter = start_node->ending_markers.begin(); iter != start_node->ending_markers.end();) {
      MarkerId id = *iter;
      if (exclusive_marker_ids.count(id) == 0 || end_node->starting_markers.count(id) > 0) {
        start_node->ending_markers.erase(iter++);
        if (end_node->starting_markers.count(id) == 0) {
          start_node->markers_to_right_ancestor.insert(id);
        }
        end_node->ending_markers.insert(id);
        end_nodes_by_id[id] = end_node;
      } else {
        ++iter;
      }
    }
  } else {
    GetStartingAndEndingMarkersWithinSubtree(start_node->right, &starting_inside_splice, &ending_inside_splice);

    for (MarkerId id : ending_inside_splice) {
      end_node->ending_markers.Insert(id);
      if (!starting_inside_splice.count(id)) {
        start_node->markers_to_right_ancestor.Insert(id);
      }
      end_nodes_by_id[id] = end_node;
    }

    for (MarkerId id : end_node->ending_markers) {
      if (exclusive_marker_ids.count(id) && !end_node->starting_markers.count(id)) {
        ending_inside_splice.insert(id);
      }
    }

    for (MarkerId id : starting_inside_splice) {
      end_node->starting_markers.insert(id);
      start_nodes_by_id[id] = end_node;
    }

    for (auto iter = start_node->starting_markers.begin(); iter != start_node->starting_markers.end();) {
      MarkerId id = *iter;
      if (exclusive_marker_ids.count(id) && !start_node->ending_markers.count(id)) {
        start_node->starting_markers.erase(iter++);
        start_node->markers_to_right_ancestor.erase(id);
        end_node->starting_markers.insert(id);
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
    for (MarkerId id : end_node->starting_markers) {
      start_node->starting_markers.insert(id);
      start_node->markers_to_right_ancestor.insert(id);
      start_nodes_by_id[id] = start_node;
    }

    for (MarkerId id : end_node->ending_markers) {
      start_node->ending_markers.insert(id);
      if (end_node->markers_to_left_ancestor.count(id) > 0) {
        start_node->markers_to_left_ancestor.insert(id);
        end_node->markers_to_left_ancestor.erase(id);
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
      result.insert(start_node->markers_to_right_ancestor.begin(), start_node->markers_to_right_ancestor.end());
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
      result.insert(node->starting_markers.begin(), node->starting_markers.end());
      result.insert(node->ending_markers.begin(), node->starting_markers.end());
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
      result.insert(start_node->starting_markers.begin(), start_node->starting_markers.end());
      result.insert(start_node->ending_markers.begin(), start_node->ending_markers.end());
      Node *start_node = SplayGreatestLowerBound(start, false);
      Node *end_node = SplayLeastUpperBound(end, false);
      if (start_node && end_node) {
        result.insert(start_node->markers_to_right_ancestor.begin(), start_node->markers_to_right_ancestor.end());
      }
    } else {
      if (start_node != end_node->left) RotateNodeRight(start_node);
      result.insert(start_node->markers_to_right_ancestor.begin(), start_node->markers_to_right_ancestor.end());
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
      starting.insert(node->starting_markers.begin(), node->starting_markers.end());
      for (auto id : node->starting_markers) {
        if (ending.count(id) > 0) {
          result.insert(id);
        } else {
          starting.insert(id);
        }
      }

      for (auto id : node->ending_markers) {
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
      result.insert(node->starting_markers.begin(), node->starting_markers.end());
    }
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindStartingAt(Point position) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *node = SplayGreatestLowerBound(position, true);
  if (node && node->distance_from_left_ancestor == position) {
    result.insert(node->starting_markers.begin(), node->starting_markers.end());
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
      result.insert(node->ending_markers.begin(), node->ending_markers.end());
    }
  }

  return result;
}

unordered_set<MarkerIndex::MarkerId> MarkerIndex::FindEndingAt(Point position) {
  unordered_set<MarkerId> result;
  if (!root) return result;

  Node *node = SplayGreatestLowerBound(position, true);
  if (node && node->distance_from_left_ancestor == position) {
    result.insert(node->ending_markers.begin(), node->ending_markers.end());
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
  Point left_ancestor_position = Point::Zero();

  while (true) {
    Point position = left_ancestor_position.Traverse(node->distance_from_left_ancestor);
    if (position == target_position && return_existing) {
      break;
    } else {
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

  rotation_pivot->markers_to_right_ancestor += rotation_root->markers_to_right_ancestor;
  auto markers_not_spanning_root = rotation_pivot->markers_to_left_ancestor - rotation_root->markers_to_left_ancestor;
  rotation_pivot->markers_to_left_ancestor -= markers_not_spanning_root;
  rotation_root->markers_to_right_ancestor += markers_not_spanning_root;
  rotation_root->markers_to_left_ancestor -= rotation_pivot->markers_to_left_ancestor;
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

  rotation_pivot->markers_to_left_ancestor += rotation_root->markers_to_left_ancestor;
  auto markers_not_spanning_root = rotation_pivot->markers_to_right_ancestor - rotation_root->markers_to_right_ancestor;
  rotation_pivot->markers_to_right_ancestor -= markers_not_spanning_root;
  rotation_root->markers_to_left_ancestor += markers_not_spanning_root;
  rotation_root->markers_to_right_ancestor -= rotation_pivot->markers_to_right_ancestor;
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
    starting->insert(node->starting_markers.begin(), node->starting_markers.end());
    ending->insert(node->ending_markers.begin(), node->ending_markers.end());
  }
}

void MarkerIndex::PopulateSpliceInvalidationSets(SpliceResult *invalidated,
                                                 const Node *start_node, const Node *end_node,
                                                 const unordered_set<MarkerId> &starting_inside_splice,
                                                 const unordered_set<MarkerId> &ending_inside_splice) {
  invalidated->touch.insert(start_node->ending_markers.begin(), start_node->ending_markers.end());
  invalidated->touch.insert(end_node->starting_markers.begin(), end_node->starting_markers.end());

  for (MarkerId id : start_node->markers_to_right_ancestor) {
    invalidated->touch.insert(id);
    invalidated->inside.insert(id);
  }

  for (MarkerId id : end_node->markers_to_left_ancestor) {
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
