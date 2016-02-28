import {ZERO_POINT, traverse, traversalDistance, min as minPoint, isZero as isZeroPoint, compare as comparePoints} from './point-helpers'
import {getExtent, characterIndexForPoint} from './text-helpers'
import Iterator from './iterator'

export default class Patch {
  static compose (patches) {
    let composedPatch = new Patch()
    for (let index = 0; index < patches.length; index++) {
      let changes = patches[index].getChanges()
      if ((index & 1) === 0) { // flip
        for (let i = 0; i < changes.length; i++) {
          let {newStart, oldExtent, newExtent, oldText, newText} = changes[i]
          composedPatch.splice(newStart, oldExtent, newExtent, {oldText, newText})
        }
      } else { // flop
        for (let i = changes.length - 1; i >= 0; i--) {
          let {oldStart, oldExtent, newExtent, oldText, newText} = changes[i]
          composedPatch.splice(oldStart, oldExtent, newExtent, {oldText, newText})
        }
      }
    }

    return composedPatch.getChanges()
  }

  constructor (params = {}) {
    this.root = null
    this.nodesCount = 0
    this.iterator = this.buildIterator()
    this.cachedChanges = null
  }

  buildIterator () {
    return new Iterator(this)
  }

  rebalance () {
    this.transformTreeToVine()
    this.transformVineToBalancedTree()
  }

  transformTreeToVine () {
    let pseudoRoot = this.root
    while (pseudoRoot != null) {
      let leftChild = pseudoRoot.left
      let rightChild = pseudoRoot.right
      if (leftChild != null) {
        this.rotateNodeRight(leftChild)
        pseudoRoot = leftChild
      } else {
        pseudoRoot = rightChild
      }
    }
  }

  transformVineToBalancedTree() {
    let n = this.nodesCount
    let m = Math.pow(2, Math.floor(Math.log2(n + 1))) - 1
    this.performRebalancingRotations(n - m)
    while (m > 1) {
      m = Math.floor(m / 2)
      this.performRebalancingRotations(m)
    }
  }

  performRebalancingRotations (count) {
    let root = this.root
    for (var i = 0; i < count; i++) {
      if (root == null) return
      let rightChild = root.right
      if (rightChild == null) return
      root = rightChild.right
      this.rotateNodeLeft(rightChild)
    }
  }

  spliceWithText (newStart, oldText, newText) {
    this.splice(newStart, getExtent(oldText), getExtent(newText), {oldText, newText})
  }

  splice (newStart, oldExtent, newExtent, options) {
    if (isZeroPoint(oldExtent) && isZeroPoint(newExtent)) return

    let oldEnd = traverse(newStart, oldExtent)
    let newEnd = traverse(newStart, newExtent)

    let {node: startNode, intersectsOldText: startNodeIntersectsOldText} =
      this.iterator.insertSpliceBoundary(newStart)
    startNode.isChangeStart = true
    this.splayNode(startNode)

    let {node: endNode, intersectsOldText: endNodeIntersectsOldText} =
      this.iterator.insertSpliceBoundary(oldEnd, startNode)
    endNode.isChangeEnd = true
    this.splayNode(endNode)
    if (endNode.left !== startNode) this.rotateNodeRight(startNode)

    let intersectingChanges = this.collectChangesInSubtree(startNode.right)
    startNode.right = null
    startNode.inputExtent = startNode.inputLeftExtent
    startNode.outputExtent = startNode.outputLeftExtent

    endNode.outputExtent = traverse(newEnd, traversalDistance(endNode.outputExtent, endNode.outputLeftExtent))
    endNode.outputLeftExtent = newEnd
    endNode.newText = options && options.newText
    endNode.oldText = options && options.oldText && this.trimOldText(
      options.oldText, endNode.oldText,
      intersectingChanges, startNodeIntersectsOldText, endNodeIntersectsOldText
    )

    if (endNode.isChangeStart) {
      let rightAncestor = this.bubbleNodeDown(endNode)
      if (endNode.newText != null) rightAncestor.newText = endNode.newText + rightAncestor.newText
      if (endNode.oldText != null) rightAncestor.oldText = endNode.oldText + rightAncestor.oldText
      this.deleteNode(endNode)
    } else if (comparePoints(endNode.outputLeftExtent, startNode.outputLeftExtent) === 0
        && comparePoints(endNode.inputLeftExtent, startNode.inputLeftExtent) === 0) {
      startNode.isChangeStart = endNode.isChangeStart
      this.deleteNode(endNode)
    }

    if (startNode.isChangeStart && startNode.isChangeEnd) {
      let rightAncestor = this.bubbleNodeDown(startNode) || this.root
      if (startNode.newText != null) rightAncestor.newText = startNode.newText + rightAncestor.newText
      if (startNode.oldText != null) rightAncestor.oldText = startNode.oldText + rightAncestor.oldText
      this.deleteNode(startNode)
    }

    this.cachedChanges = null
  }

  collectChangesInSubtree (node, outputDistance = ZERO_POINT, changes = []) {
    if (node == null) return changes

    this.collectChangesInSubtree(node.left, outputDistance, changes)
    let change = {}
    let outputLeftExtent = traverse(outputDistance, node.outputLeftExtent)
    if (node.isChangeStart) change.start = outputLeftExtent
    if (node.isChangeEnd) {
      change.end = outputLeftExtent
      change.oldText = node.oldText
    }
    changes.push(change)
    this.collectChangesInSubtree(node.right, outputLeftExtent, changes)

    return changes
  }

  trimOldText (newOldText, previousOldText, intersectingChanges, startNodeIntersectsOldText, endNodeIntersectsOldText) {
    if (intersectingChanges.length > 0) {
      let text = ""
      let previousChangeEnd = ZERO_POINT
      for (let change of intersectingChanges) {
        if (change.start) {
          text += newOldText.substring(
            characterIndexForPoint(newOldText, previousChangeEnd),
            characterIndexForPoint(newOldText, change.start)
          )
          previousChangeEnd = null
        }
        if (change.end) {
          text += change.oldText
          previousChangeEnd = change.end
        }
      }

      if (previousChangeEnd) text += newOldText.substring(characterIndexForPoint(newOldText, previousChangeEnd))
      if (previousOldText) text += previousOldText
      return text
    } else if (previousOldText == null && !startNodeIntersectsOldText && !endNodeIntersectsOldText) {
      return newOldText
    } else {
      return previousOldText
    }
  }

  getChanges () {
    if (this.cachedChanges == null) {
      this.cachedChanges = this.iterator.getChanges()
    }

    return this.cachedChanges
  }

  deleteNode (node) {
    this.bubbleNodeDown(node)
    if (node.parent) {
      if (node.parent.left === node) {
        node.parent.left = null
      } else {
        node.parent.right = null
        node.parent.inputExtent = node.parent.inputLeftExtent
        node.parent.outputExtent = node.parent.outputLeftExtent
        let ancestor = node.parent
        while (ancestor.parent && ancestor.parent.right === ancestor) {
          ancestor.parent.inputExtent = traverse(ancestor.parent.inputLeftExtent, ancestor.inputExtent)
          ancestor.parent.outputExtent = traverse(ancestor.parent.outputLeftExtent, ancestor.outputExtent)
          ancestor = ancestor.parent
        }
      }

      this.splayNode(node.parent)
    } else {
      this.root = null
    }

    this.nodesCount--
  }

  bubbleNodeDown (node) {
    let rightAncestor

    while (true) {
      if (node.left) {
        this.rotateNodeRight(node.left)
      } else if (node.right) {
        rightAncestor = node.right
        this.rotateNodeLeft(node.right)
      } else {
        break
      }
    }

    return rightAncestor
  }

  splayNode (node) {
    if (node == null) return

    while (true) {
      if (this.isNodeLeftChild(node.parent) && this.isNodeRightChild(node)) { // zig-zag
        this.rotateNodeLeft(node)
        this.rotateNodeRight(node)
      } else if (this.isNodeRightChild(node.parent) && this.isNodeLeftChild(node)) { // zig-zag
        this.rotateNodeRight(node)
        this.rotateNodeLeft(node)
      } else if (this.isNodeLeftChild(node.parent) && this.isNodeLeftChild(node)) { // zig-zig
        this.rotateNodeRight(node.parent)
        this.rotateNodeRight(node)
      } else if (this.isNodeRightChild(node.parent) && this.isNodeRightChild(node)) { // zig-zig
        this.rotateNodeLeft(node.parent)
        this.rotateNodeLeft(node)
      } else { // zig
        if (this.isNodeLeftChild(node)) {
          this.rotateNodeRight(node)
        } else if (this.isNodeRightChild(node)) {
          this.rotateNodeLeft(node)
        }

        return
      }
    }
  }

  isNodeLeftChild (node) {
    return node != null && node.parent != null && node.parent.left === node
  }

  isNodeRightChild (node) {
    return node != null && node.parent != null && node.parent.right === node
  }

  rotateNodeLeft (pivot) {
    let root = pivot.parent

    if (root.parent) {
      if (root === root.parent.left) {
        root.parent.left = pivot
      } else {
        root.parent.right = pivot
      }
    } else {
      this.root = pivot
    }
    pivot.parent = root.parent

    root.right = pivot.left
    if (root.right) {
      root.right.parent = root
    }

    pivot.left = root
    pivot.left.parent = pivot

    pivot.inputLeftExtent = traverse(root.inputLeftExtent, pivot.inputLeftExtent)
    pivot.inputExtent = traverse(pivot.inputLeftExtent, (pivot.right ? pivot.right.inputExtent : ZERO_POINT))
    root.inputExtent = traverse(root.inputLeftExtent, (root.right ? root.right.inputExtent : ZERO_POINT))

    pivot.outputLeftExtent = traverse(root.outputLeftExtent, pivot.outputLeftExtent)
    pivot.outputExtent = traverse(pivot.outputLeftExtent, (pivot.right ? pivot.right.outputExtent : ZERO_POINT))
    root.outputExtent = traverse(root.outputLeftExtent, (root.right ? root.right.outputExtent : ZERO_POINT))
  }

  rotateNodeRight (pivot) {
    let root = pivot.parent

    if (root.parent) {
      if (root === root.parent.left) {
        root.parent.left = pivot
      } else {
        root.parent.right = pivot
      }
    } else {
      this.root = pivot
    }
    pivot.parent = root.parent

    root.left = pivot.right
    if (root.left) {
      root.left.parent = root
    }

    pivot.right = root
    pivot.right.parent = pivot

    root.inputLeftExtent = traversalDistance(root.inputLeftExtent, pivot.inputLeftExtent)
    root.inputExtent = traversalDistance(root.inputExtent, pivot.inputLeftExtent)
    pivot.inputExtent = traverse(pivot.inputLeftExtent, root.inputExtent)

    root.outputLeftExtent = traversalDistance(root.outputLeftExtent, pivot.outputLeftExtent)
    root.outputExtent = traversalDistance(root.outputExtent, pivot.outputLeftExtent)
    pivot.outputExtent = traverse(pivot.outputLeftExtent, root.outputExtent)
  }
}
