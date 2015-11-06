#include "marker-index.h"
#include <climits>
#include <random>
#include <stdlib.h>

using std::default_random_engine;

MarkerIndex::MarkerIndex(unsigned seed)
  : random_engine {static_cast<default_random_engine::result_type>(seed)},
    random_distribution{1, INT_MAX},
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

void MarkerIndex::BubbleNodeUp(Node *node) {
  while (node->parent && node->priority < node->parent->priority) {
    if (node == node->parent->left) {
      RotateNodeRight(node);
    } else {
      RotateNodeLeft(node);
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
