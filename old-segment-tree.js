'use babel';

import Random from 'random-seed';
import SegmentTreeNode from './segment-tree-node';

const NOT_DONE = {done: false};
const DONE = {done: true};

export default class SegmentTree {
  constructor(randomSeed = Date.now()) {
    this.randomSeed = randomSeed;
    this.randomGenerator = new Random(randomSeed);
    this.root = null;
  }

  buildIterator() {
    return new SegmentTreeIterator(this);
  }

  buildIteratorAtStart() {
    return new SegmentTreeIterator(this, true);
  }

  splice(outputStart, removedCount, addedCount) {
    let outputEnd = outputStart + removedCount;
    let spliceDelta = addedCount - removedCount;

    let startNode = this.insertSpliceBoundary(outputStart, true);
    let endNode = this.insertSpliceBoundary(outputEnd, false);
    startNode.priority = -1;
    this.bubbleNodeUp(startNode);
    endNode.priority = -2;
    this.bubbleNodeUp(endNode);

    startNode.right = null;
    startNode.inputExtent = startNode.inputLeftExtent;
    startNode.outputExtent = startNode.outputLeftExtent;

    endNode.outputLeftExtent += spliceDelta;
    endNode.outputExtent += spliceDelta;

    startNode.priority = this.generateRandom();
    this.bubbleNodeDown(startNode);
    endNode.priority = this.generateRandom();
    this.bubbleNodeDown(endNode);
  }

  insertSpliceBoundary(boundaryOutputIndex, insertingChangeStart) {
    let insertingChangeEnd = !insertingChangeStart;

    let node = this.root;

    if (!node) {
      this.root = new SegmentTreeNode(null, boundaryOutputIndex, boundaryOutputIndex);
      this.root.isChangeStart = insertingChangeStart;
      this.root.isChangeEnd = insertingChangeEnd;
      return this.root;
    }

    let inputOffset = 0;
    let outputOffset = 0;
    let maxInputIndex = Infinity;
    let nodeInputIndex, nodeOutputIndex;
    let closestStartNode, closestEndNode;

    while (true) {
      nodeInputIndex = inputOffset + node.inputLeftExtent;
      nodeOutputIndex = outputOffset + node.outputLeftExtent;

      if (node.isChangeStart) {
        let node = visitChangeStart();
        if (node) {
          return node;
        }
      } else {
        let node = visitChangeEnd();
        if (node) {
          return node;
        }
      }
    }

    function visitChangeStart() {
      if (boundaryOutputIndex < nodeOutputIndex) {
        if (node.left) {
          closestEndNode = null;
          descendLeft();
        } else {
          return insertLeftNode();
        }
      } else { // boundaryOutputIndex >= nodeOutputIndex
        if (node.right) {
          closestStartNode = node;
          descendRight();
        } else {
          if (insertingChangeStart) {
            return node;
          } else { // insertingChangeEnd
            if (closestEndNode) {
              return closestEndNode;
            } else {
              return insertRightNode();
            }
          }
        }
      }
    }

    function visitChangeEnd() {
      if (boundaryOutputIndex <= nodeOutputIndex) {
        if (node.left) {
          closestEndNode = node;
          descendLeft();
        } else {
          if (insertingChangeStart) {
            if (closestStartNode) {
              return closestStartNode;
            } else {
              return insertLeftNode();
            }
          } else { // insertingChangeEnd
            return node;
          }
        }
      } else { // boundaryOutputIndex > nodeOutputIndex
        if (node.right) {
          closestStartNode = null;
          descendRight();
        } else {
          return insertRightNode();
        }
      }
    }

    function descendLeft() {
      maxInputIndex = nodeInputIndex;
      node = node.left;
    }

    function descendRight() {
      inputOffset += node.inputLeftExtent;
      outputOffset += node.outputLeftExtent;
      node = node.right;
    }

    function insertLeftNode() {
      let outputLeftExtent = boundaryOutputIndex - outputOffset;
      let inputLeftExtent = Math.min(outputLeftExtent, node.inputLeftExtent);
      let newNode = new SegmentTreeNode(node, inputLeftExtent, outputLeftExtent);
      newNode.isChangeStart = insertingChangeStart;
      newNode.isChangeEnd = insertingChangeEnd;
      node.left = newNode;
      return newNode;
    }

    function insertRightNode() {
      let outputLeftExtent = boundaryOutputIndex - nodeOutputIndex;
      let inputLeftExtent = Math.min(outputLeftExtent, maxInputIndex - nodeInputIndex);
      let newNode = new SegmentTreeNode(node, inputLeftExtent, outputLeftExtent);
      newNode.isChangeStart = insertingChangeStart;
      newNode.isChangeEnd = insertingChangeEnd;
      node.right = newNode;
      return newNode;
    }
  }

  bubbleNodeUp(node) {
    while (node.parent && node.priority < node.parent.priority) {
      if (node === node.parent.left) {
        this.rotateNodeRight(node);
      } else {
        this.rotateNodeLeft(node);
      }
    }
  }

  bubbleNodeDown(node) {
    while (true) {
      let leftChildPriority = node.left ? node.left.priority : Infinity;
      let rightChildPriority = node.right ? node.right.priority : Infinity;

      if (leftChildPriority < rightChildPriority && leftChildPriority < node.priority) {
        this.rotateNodeRight(node.left);
      } else if (rightChildPriority < node.priority) {
        this.rotateNodeLeft(node.right);
      } else {
        break;
      }
    }
  }

  rotateNodeLeft(pivot) {
    let root = pivot.parent;

    if (root.parent) {
      if (root === root.parent.left) {
        root.parent.left = pivot;
      } else {
        root.parent.right = pivot;
      }
    } else {
      this.root = pivot;
    }
    pivot.parent = root.parent;

    root.right = pivot.left;
    if (root.right) {
      root.right.parent = root;
    }

    pivot.left = root;
    pivot.left.parent = pivot;

    pivot.inputLeftExtent = root.inputLeftExtent + pivot.inputLeftExtent;
    pivot.inputExtent = pivot.inputLeftExtent + (pivot.right ? pivot.right.inputExtent : 0);
    root.inputExtent = root.inputLeftExtent + (root.right ? root.right.inputExtent : 0);

    pivot.outputLeftExtent = root.outputLeftExtent + pivot.outputLeftExtent;
    pivot.outputExtent = pivot.outputLeftExtent + (pivot.right ? pivot.right.outputExtent : 0);
    root.outputExtent = root.outputLeftExtent + (root.right ? root.right.outputExtent : 0);
  }

  rotateNodeRight(pivot) {
    let root = pivot.parent;

    if (root.parent) {
      if (root === root.parent.left) {
        root.parent.left = pivot;
      } else {
        root.parent.right = pivot;
      }
    } else {
      this.root = pivot;
    }
    pivot.parent = root.parent;

    root.left = pivot.right;
    if (root.left) {
      root.left.parent = root;
    }

    pivot.right = root;
    pivot.right.parent = pivot;

    root.inputLeftExtent = root.inputLeftExtent - pivot.inputLeftExtent;
    root.inputExtent = root.inputExtent - pivot.inputLeftExtent;
    pivot.inputExtent = pivot.inputLeftExtent + root.inputExtent;

    root.outputLeftExtent = root.outputLeftExtent - pivot.outputLeftExtent;
    root.outputExtent = root.outputExtent - pivot.outputLeftExtent;
    pivot.outputExtent = pivot.outputLeftExtent + root.outputExtent;
  }

  generateRandom() {
    return this.randomGenerator.random();
  }

  toHTML() {
    return this.root.toHTML();
  }
}

class SegmentTreeIterator {
  constructor(tree, rewind) {
    this.tree = tree;
    this.inputOffset = 0;
    this.outputOffset = 0;
    this.inputOffsetStack = [];
    this.outputOffsetStack = [];
    this.setNode(tree.root);

    if (rewind && this.node) {
      while (this.node.left) {
        this.descendLeft();
      }
    }
  }

  getInputStart() {
    return this.inputStart;
  }

  getInputEnd() {
    return this.inputEnd;
  }

  getOutputStart() {
    return this.outputStart;
  }

  getOutputEnd() {
    return this.outputEnd;
  }

  inChange() {
    return !!this.node && this.node.isChangeEnd;
  }

  setNode(node) {
    this.node = node;

    if (node) {
      if (node.left) {
        this.inputStart = this.inputOffset + node.left.inputExtent;
        this.outputStart = this.outputOffset + node.left.outputExtent;
      } else {
        this.inputStart = this.inputOffset;
        this.outputStart = this.outputOffset;
      }

      this.inputEnd = this.inputOffset + node.inputLeftExtent;
      this.outputEnd = this.outputOffset + node.outputLeftExtent;
    } else {
      this.inputStart = 0;
      this.inputEnd = Infinity;
      this.outputStart = 0;
      this.outputEnd = Infinity;
    }
  }

  next() {
    if (!this.node) {
      return DONE;
    }

    if (this.node.right) {
      this.descendRight();
      while (this.node.left) {
        this.descendLeft();
      }
      return NOT_DONE;
    } else {
      while (this.node.parent && this.node.parent.right === this.node) {
        this.ascend();
      }
      if (this.node.parent) {
        this.ascend();
        return NOT_DONE;
      } else {
        return DONE;
      }
    }
  }

  ascend() {
    this.inputOffset = this.inputOffsetStack.pop();
    this.outputOffset = this.outputOffsetStack.pop();
    this.setNode(this.node.parent);
  }

  descendLeft() {
    this.inputOffsetStack.push(this.inputOffset);
    this.outputOffsetStack.push(this.outputOffset);
    this.setNode(this.node.left);
  }

  descendRight() {
    this.inputOffsetStack.push(this.inputOffset);
    this.outputOffsetStack.push(this.outputOffset);
    this.inputOffset += this.node.inputLeftExtent;
    this.outputOffset += this.node.outputLeftExtent;
    this.setNode(this.node.right);
  }
}
