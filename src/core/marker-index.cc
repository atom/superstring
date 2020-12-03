#include "marker-index.h"
#include <climits>
#include <iterator>
#include <random>
#include <cstdlib>
#include "range.h"

using std::default_random_engine;
using std::unordered_map;

MarkerIndex::Node::Node(Node *parent, Point left_extent) :
  parent{parent},
  left{nullptr},
  right{nullptr},
  left_extent{left_extent},
  priority{0} {}

bool MarkerIndex::Node::is_marker_endpoint() {
  return (start_marker_ids.size() + end_marker_ids.size()) > 0;
}

MarkerIndex::Iterator::Iterator(MarkerIndex *marker_index) :
  marker_index{marker_index},
  current_node{nullptr} {}

void MarkerIndex::Iterator::reset() {
  current_node = marker_index->root;
  if (current_node) {
    current_node_position = current_node->left_extent;
  }
  left_ancestor_position = Point(0, 0);
  right_ancestor_position = Point(UINT32_MAX, UINT32_MAX);
  left_ancestor_position_stack.clear();
  right_ancestor_position_stack.clear();
}

MarkerIndex::Node *MarkerIndex::Iterator::insert_marker_start(const MarkerId &id, const Point &start_position, const Point &end_position) {
  reset();

  if (!current_node) {
    return marker_index->root = new Node(nullptr, start_position);
  }

  while (true) {
    switch (start_position.compare(current_node_position)) {
      case 0:
        mark_right(id, start_position, end_position);
        return current_node;
      case -1:
        mark_right(id, start_position, end_position);
        if (current_node->left) {
          descend_left();
          break;
        } else {
          insert_left_child(start_position);
          descend_left();
          mark_right(id, start_position, end_position);
          return current_node;
        }
      case 1:
        if (current_node->right) {
          descend_right();
          break;
        } else {
          insert_right_child(start_position);
          descend_right();
          mark_right(id, start_position, end_position);
          return current_node;
        }
    }
  }
}

MarkerIndex::Node *MarkerIndex::Iterator::insert_marker_end(const MarkerId &id, const Point &start_position, const Point &end_position) {
  reset();

  if (!current_node) {
    return marker_index->root = new Node(nullptr, end_position);
  }

  while (true) {
    switch (end_position.compare(current_node_position)) {
      case 0:
        mark_left(id, start_position, end_position);
        return current_node;
      case -1:
        if (current_node->left) {
          descend_left();
          break;
        } else {
          insert_left_child(end_position);
          descend_left();
          mark_left(id, start_position, end_position);
          return current_node;
        }
      case 1:
        mark_left(id, start_position, end_position);
        if (current_node->right) {
          descend_right();
          break;
        } else {
          insert_right_child(end_position);
          descend_right();
          mark_left(id, start_position, end_position);
          return current_node;
        }
    }
  }
}

MarkerIndex::Node *MarkerIndex::Iterator::insert_splice_boundary(const Point &position, bool is_insertion_end) {
  reset();

  while (true) {
    int comparison = position.compare(current_node_position);
    if (comparison == 0 && !is_insertion_end) {
      return current_node;
    } else if (comparison < 0) {
      if (current_node->left) {
        descend_left();
      } else {
        insert_left_child(position);
        return current_node->left;
      }
    } else { // comparison > 0
      if (current_node->right) {
        descend_right();
      } else {
        insert_right_child(position);
        return current_node->right;
      }
    }
  }
}

void MarkerIndex::Iterator::find_intersecting(const Point &start, const Point &end, MarkerIdSet *result) {
  reset();

  if (!current_node) return;

  while (true) {
    cache_node_position();
    if (start < current_node_position) {
      if (current_node->left) {
        check_intersection(start, end, result);
        descend_left();
      } else {
        break;
      }
    } else {
      if (current_node->right) {
        check_intersection(start, end, result);
        descend_right();
      } else {
        break;
      }
    }
  }

  do {
    check_intersection(start, end, result);
    move_to_successor();
    cache_node_position();
  } while (current_node && current_node_position <= end);
}

void MarkerIndex::Iterator::find_contained_in(const Point &start, const Point &end, MarkerIdSet *result) {
  reset();

  if (!current_node) return;

  seek_to_first_node_greater_than_or_equal_to(start);

  MarkerIdSet started;
  while (current_node && current_node_position <= end) {
    started.insert(current_node->start_marker_ids.begin(), current_node->start_marker_ids.end());
    for (MarkerId id : current_node->end_marker_ids) {
      if (started.count(id) > 0) result->insert(id);
    }
    cache_node_position();
    move_to_successor();
  }
}

void MarkerIndex::Iterator::find_starting_in(const Point &start, const Point &end, MarkerIdSet *result) {
  reset();

  if (!current_node) return;

  seek_to_first_node_greater_than_or_equal_to(start);

  while (current_node && current_node_position <= end) {
    result->insert(current_node->start_marker_ids.begin(), current_node->start_marker_ids.end());
    cache_node_position();
    move_to_successor();
  }
}

void MarkerIndex::Iterator::find_ending_in(const Point &start, const Point &end, MarkerIdSet *result) {
  reset();

  if (!current_node) return;

  seek_to_first_node_greater_than_or_equal_to(start);

  while (current_node && current_node_position <= end) {
    result->insert(current_node->end_marker_ids.begin(), current_node->end_marker_ids.end());
    cache_node_position();
    move_to_successor();
  }
}

void MarkerIndex::Iterator::find_boundaries_after(Point start, size_t max_count, MarkerIndex::BoundaryQueryResult *result) {
  reset();
  if (!current_node) return;

  while (true) {
    cache_node_position();

    if (start <= current_node_position) {
      if (left_ancestor_position < start) {
        result->containing_start.insert(
          result->containing_start.end(),
          current_node->left_marker_ids.begin(),
          current_node->left_marker_ids.end()
        );
      }

      if (current_node->left) {
        descend_left();
      } else {
        break;
      }
    } else {
      if (right_ancestor_position >= start) {
        result->containing_start.insert(
          result->containing_start.end(),
          current_node->right_marker_ids.begin(),
          current_node->right_marker_ids.end()
        );
      }

      if (current_node->right) {
        descend_right();
      } else {
        break;
      }
    }
  }
  std::sort(
    result->containing_start.begin(),
    result->containing_start.end(),
    [this](MarkerId a, MarkerId b) {
      int comparison = marker_index->compare(a, b);
      return comparison == 0 ? a < b : comparison == -1;
    }
  );

  if (current_node_position < start) move_to_successor();
  while (current_node && max_count > 0) {
    cache_node_position();
    result->boundaries.push_back({
      current_node_position,
      current_node->start_marker_ids,
      current_node->end_marker_ids
    });
    move_to_successor();
    max_count--;
  }
}

unordered_map<MarkerIndex::MarkerId, Range> MarkerIndex::Iterator::dump() {
  reset();

  unordered_map<MarkerId, Range> snapshot;

  if (!current_node) return snapshot;

  while (current_node && current_node->left) {
    cache_node_position();
    descend_left();
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

    cache_node_position();
    move_to_successor();
  }

  return snapshot;
}

void MarkerIndex::Iterator::ascend() {
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

void MarkerIndex::Iterator::descend_left() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  right_ancestor_position = current_node_position;
  current_node = current_node->left;
  current_node_position = left_ancestor_position.traverse(current_node->left_extent);
}

void MarkerIndex::Iterator::descend_right() {
  left_ancestor_position_stack.push_back(left_ancestor_position);
  right_ancestor_position_stack.push_back(right_ancestor_position);

  left_ancestor_position = current_node_position;
  current_node = current_node->right;
  current_node_position = left_ancestor_position.traverse(current_node->left_extent);
}

void MarkerIndex::Iterator::move_to_successor() {
  if (!current_node) return;

  if (current_node->right) {
    descend_right();
    while (current_node->left) {
      descend_left();
    }
  } else {
    while (current_node->parent && current_node->parent->right == current_node) {
      ascend();
    }
    ascend();
  }
}

void MarkerIndex::Iterator::seek_to_first_node_greater_than_or_equal_to(const Point &position) {
  while (true) {
    cache_node_position();
    if (position == current_node_position) {
      break;
    } else if (position < current_node_position) {
      if (current_node->left) {
        descend_left();
      } else {
        break;
      }
    } else { // position > current_node_position
      if (current_node->right) {
        descend_right();
      } else {
        break;
      }
    }
  }

  if (current_node_position < position) move_to_successor();
}

void MarkerIndex::Iterator::mark_right(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (left_ancestor_position < start_position
    && start_position <= current_node_position
    && right_ancestor_position <= end_position) {
    current_node->right_marker_ids.insert(id);
  }
}

void MarkerIndex::Iterator::mark_left(const MarkerId &id, const Point &start_position, const Point &end_position) {
  if (!current_node_position.is_zero() && start_position <= left_ancestor_position && current_node_position <= end_position) {
    current_node->left_marker_ids.insert(id);
  }
}

MarkerIndex::Node *MarkerIndex::Iterator::insert_left_child(const Point &position) {
  return current_node->left = new Node(current_node, position.traversal(left_ancestor_position));
}

MarkerIndex::Node *MarkerIndex::Iterator::insert_right_child(const Point &position) {
  return current_node->right = new Node(current_node, position.traversal(current_node_position));
}

void MarkerIndex::Iterator::check_intersection(const Point &start, const Point &end, MarkerIdSet *result) {
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

void MarkerIndex::Iterator::cache_node_position() const {
  marker_index->node_position_cache.insert({current_node, current_node_position});
}

MarkerIndex::MarkerIndex(unsigned seed)
  : random_engine{static_cast<default_random_engine::result_type>(seed)},
    random_distribution{1, INT_MAX - 1},
    root{nullptr},
    iterator{this} {}

MarkerIndex::~MarkerIndex() {
  if (root) delete_subtree(root);
}

int MarkerIndex::generate_random_number() {
  return random_distribution(random_engine);
}

void MarkerIndex::insert(MarkerId id, Point start, Point end) {
  Node *start_node = iterator.insert_marker_start(id, start, end);
  Node *end_node = iterator.insert_marker_end(id, start, end);

  node_position_cache.insert({start_node, start});
  node_position_cache.insert({end_node, end});

  start_node->start_marker_ids.insert(id);
  end_node->end_marker_ids.insert(id);

  if (start_node->priority == 0) {
    start_node->priority = generate_random_number();
    bubble_node_up(start_node);
  }

  if (end_node->priority == 0) {
    end_node->priority = generate_random_number();
    bubble_node_up(end_node);
  }

  start_nodes_by_id.insert({id, start_node});
  end_nodes_by_id.insert({id, end_node});
}

void MarkerIndex::set_exclusive(MarkerId id, bool exclusive) {
  if (exclusive) {
    exclusive_marker_ids.insert(id);
  } else {
    exclusive_marker_ids.erase(id);
  }
}

void MarkerIndex::remove(MarkerId id) {
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

  if (!start_node->is_marker_endpoint()) {
    delete_node(start_node);
  }

  if (end_node != start_node && !end_node->is_marker_endpoint()) {
    delete_node(end_node);
  }

  start_nodes_by_id.erase(id);
  end_nodes_by_id.erase(id);
}

bool MarkerIndex::has(MarkerId id) {
  return start_nodes_by_id.count(id) > 0;
}

MarkerIndex::SpliceResult MarkerIndex::splice(Point start, Point old_extent, Point new_extent) {
  node_position_cache.clear();

  SpliceResult invalidated;

  if (!root || (old_extent.is_zero() && new_extent.is_zero())) return invalidated;

  bool is_insertion = old_extent.is_zero();
  Node *start_node = iterator.insert_splice_boundary(start, false);
  Node *end_node = iterator.insert_splice_boundary(start.traverse(old_extent), is_insertion);

  start_node->priority = -1;
  bubble_node_up(start_node);
  end_node->priority = -2;
  bubble_node_up(end_node);

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
    get_starting_and_ending_markers_within_subtree(start_node->right, &starting_inside_splice, &ending_inside_splice);

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

  populate_splice_invalidation_sets(&invalidated, start_node, end_node, starting_inside_splice, ending_inside_splice);

  if (start_node->right) {
    delete_subtree(start_node->right);
    start_node->right = nullptr;
  }

  end_node->left_extent = start.traverse(new_extent);

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
    delete_node(end_node);
  } else if (end_node->is_marker_endpoint()) {
    end_node->priority = generate_random_number();
    bubble_node_down(end_node);
  } else {
    delete_node(end_node);
  }

  if (start_node->is_marker_endpoint()) {
    start_node->priority = generate_random_number();
    bubble_node_down(start_node);
  } else {
    delete_node(start_node);
  }

  return invalidated;
}

Point MarkerIndex::get_start(MarkerId id) const {
  auto result = start_nodes_by_id.find(id);
  if (result == start_nodes_by_id.end())
    return Point();
  else
    return get_node_position(result->second);
}

Point MarkerIndex::get_end(MarkerId id) const {
  auto result = end_nodes_by_id.find(id);
  if (result == end_nodes_by_id.end())
    return Point();
  else
    return get_node_position(result->second);
}

Range MarkerIndex::get_range(MarkerId id) const {
  return Range{get_start(id), get_end(id)};
}

int MarkerIndex::compare(MarkerId id1, MarkerId id2) const {
  switch (get_start(id1).compare(get_start(id2))) {
    case -1:
      return -1;
    case 1:
      return 1;
    default:
      return get_end(id2).compare(get_end(id1));
  }
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::find_intersecting(Point start, Point end) {
  MarkerIdSet result;
  iterator.find_intersecting(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::find_containing(Point start, Point end) {
  MarkerIdSet containing_start;
  iterator.find_intersecting(start, start, &containing_start);
  if (end == start) {
    return containing_start;
  } else {
    MarkerIdSet containing_end;
    iterator.find_intersecting(end, end, &containing_end);
    MarkerIdSet containing_start_and_end;
    for (MarkerId id : containing_start) {
      if (containing_end.count(id) > 0) {
        containing_start_and_end.insert(id);
      }
    }
    return containing_start_and_end;
  }
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::find_contained_in(Point start, Point end) {
  MarkerIdSet result;
  iterator.find_contained_in(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::find_starting_in(Point start, Point end) {
  MarkerIdSet result;
  iterator.find_starting_in(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::find_starting_at(Point position) {
  return find_starting_in(position, position);
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::find_ending_in(Point start, Point end) {
  MarkerIdSet result;
  iterator.find_ending_in(start, end, &result);
  return result;
}

flat_set<MarkerIndex::MarkerId> MarkerIndex::find_ending_at(Point position) {
  return find_ending_in(position, position);
}

MarkerIndex::BoundaryQueryResult MarkerIndex::find_boundaries_after(Point start, size_t max_count) {
  BoundaryQueryResult result;
  iterator.find_boundaries_after(start, max_count, &result);
  return result;
}

unordered_map<MarkerIndex::MarkerId, Range> MarkerIndex::dump() {
  return iterator.dump();
}

Point MarkerIndex::get_node_position(const Node *node) const {
  auto cache_entry = node_position_cache.find(node);
  if (cache_entry == node_position_cache.end()) {
    Point position = node->left_extent;
    const Node *current_node = node;
    while (current_node->parent) {
      if (current_node->parent->right == current_node) {
        position = current_node->parent->left_extent.traverse(position);
      }

      current_node = current_node->parent;
    }
    node_position_cache.insert({node, position});
    return position;
  } else {
    return cache_entry->second;
  }
}

void MarkerIndex::delete_node(Node *node) {
  node_position_cache.erase(node);
  node->priority = INT_MAX;

  bubble_node_down(node);

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

void MarkerIndex::delete_subtree(Node *node) {
  if (node->left) delete_subtree(node->left);
  if (node->right) delete_subtree(node->right);
  delete node;
}

void MarkerIndex::bubble_node_up(Node *node) {
  while (node->parent && node->priority < node->parent->priority) {
    if (node == node->parent->left) {
      rotate_node_right(node);
    } else {
      rotate_node_left(node);
    }
  }
}

void MarkerIndex::bubble_node_down(Node *node) {
  while (true) {
    int left_child_priority = (node->left) ? node->left->priority : INT_MAX;
    int right_child_priority = (node->right) ? node->right->priority : INT_MAX;

    if (left_child_priority < right_child_priority && left_child_priority < node->priority) {
      rotate_node_right(node->left);
    } else if (right_child_priority < node->priority) {
      rotate_node_left(node->right);
    } else {
      break;
    }
  }
}

void MarkerIndex::rotate_node_left(Node *rotation_pivot) {
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

  rotation_pivot->left_extent = rotation_root->left_extent.traverse(rotation_pivot->left_extent);

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

void MarkerIndex::rotate_node_right(Node *rotation_pivot) {
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

  rotation_root->left_extent = rotation_root->left_extent.traversal(rotation_pivot->left_extent);

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

void MarkerIndex::get_starting_and_ending_markers_within_subtree(const Node *node, MarkerIdSet *starting, MarkerIdSet *ending) {
  if (node == nullptr) {
    return;
  }

  get_starting_and_ending_markers_within_subtree(node->left, starting, ending);
  starting->insert(node->start_marker_ids.begin(), node->start_marker_ids.end());
  ending->insert(node->end_marker_ids.begin(), node->end_marker_ids.end());
  get_starting_and_ending_markers_within_subtree(node->right, starting, ending);
}

void MarkerIndex::populate_splice_invalidation_sets(SpliceResult *invalidated, const Node *start_node, const Node *end_node, const MarkerIdSet &starting_inside_splice, const MarkerIdSet &ending_inside_splice) {
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
