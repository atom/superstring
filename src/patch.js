import Random from 'random-seed'
import {ZERO_POINT, INFINITY_POINT, traverse, traversalDistance, min as minPoint, isZero as isZeroPoint, compare as comparePoints} from './point-helpers'
import {getExtent} from './text-helpers'
import Iterator from './iterator'

export default class Patch {
  constructor (params = {}) {
    this.combineChanges = (params.combineChanges != null) ? Boolean(params.combineChanges) : true
    this.batchMode = (params.batchMode != null) ? Boolean(params.batchMode) : false
    if (params.seed) {
      let randomGenerator = new Random(params.seed)
      this.generateRandom = randomGenerator.random.bind(randomGenerator)
    }

    this.root = null
    this.nodesCount = 0
    this.iterator = this.buildIterator()
  }

  buildIterator () {
    return new Iterator(this)
  }

  rebalance () {
    this.createBackbone()
    this.createBalancedTree()
  }

  createBackbone () {
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

  createBalancedTree() {
    let n = this.nodesCount
    let m = Math.pow(2, Math.floor(Math.log2(n + 1))) - 1
    this.performRebalancingRotations(n - m)
    while (m > 1) {
      m /= 2
      this.performRebalancingRotations(m - 1)
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

  spliceWithText (start, oldExtent, newText, options) {
    let metadata = options ? options.metadata : null
    this.splice(start, oldExtent, getExtent(newText), {text: newText, metadata})
  }

  splice (outputStart, oldExtent, newExtent, options) {
    if (isZeroPoint(oldExtent) && isZeroPoint(newExtent)) return

    let outputOldEnd = traverse(outputStart, oldExtent)
    let outputNewEnd = traverse(outputStart, newExtent)

    let startNode = this.iterator.insertSpliceBoundary(outputStart)
    startNode.isChangeStart = true
    if (this.batchMode) this.splayNode(startNode)

    let endNode = this.iterator.insertSpliceBoundary(outputOldEnd, startNode)
    endNode.isChangeEnd = true
    if (this.batchMode) this.splayNode(endNode)
    if (options && options.metadata) endNode.metadata = options.metadata

    if (this.batchMode) {
      if (startNode !== endNode && endNode.left !== startNode) this.rotateNodeRight(startNode)
    } else {
      startNode.priority = -1
      this.bubbleNodeUp(startNode)
      endNode.priority = -2
      this.bubbleNodeUp(endNode)
    }

    startNode.right = null
    startNode.inputExtent = startNode.inputLeftExtent
    startNode.outputExtent = startNode.outputLeftExtent

    endNode.outputExtent = traverse(outputNewEnd, traversalDistance(endNode.outputExtent, endNode.outputLeftExtent))
    endNode.outputLeftExtent = outputNewEnd
    endNode.newText = options && options.text

    if (endNode.isChangeStart && this.combineChanges) {
      endNode.priority = Infinity
      let rightAncestor = this.bubbleNodeDown(endNode)
      if (endNode.newText != null) {
        rightAncestor.newText = endNode.newText + rightAncestor.newText
      }
      this.deleteNode(endNode)
    } else if (comparePoints(endNode.outputLeftExtent, startNode.outputLeftExtent) === 0
        && comparePoints(endNode.inputLeftExtent, startNode.inputLeftExtent) === 0) {
      startNode.isChangeStart = endNode.isChangeStart
      this.deleteNode(endNode)
    } else if (!this.batchMode) {
      endNode.priority = this.generateRandom()
      this.bubbleNodeDown(endNode)
    }

    if (startNode.isChangeStart && startNode.isChangeEnd && this.combineChanges) {
      startNode.priority = Infinity
      let rightAncestor = this.bubbleNodeDown(startNode) || endNode
      if (startNode.newText != null) {
        rightAncestor.newText = startNode.newText + rightAncestor.newText
      }
      this.deleteNode(startNode)
    } else if (!this.batchMode) {
      startNode.priority = this.generateRandom()
      this.bubbleNodeDown(startNode)
    }
  }

  spliceInput (inputStart, oldExtent, newExtent) {
    let oldExtentIsZero = isZeroPoint(oldExtent)

    if (oldExtentIsZero && isZeroPoint(newExtent)) {
      let outputStart = this.translateInputPosition(inputStart)
      return {start: outputStart, oldExtent, newExtent}
    }

    let inputOldEnd = traverse(inputStart, oldExtent)
    let inputNewEnd = traverse(inputStart, newExtent)

    let startNode = this.iterator.insertSpliceInputBoundary(inputStart, true, oldExtentIsZero)
    if (this.batchMode) this.splayNode(startNode)
    let endNode = this.iterator.insertSpliceInputBoundary(inputOldEnd, false, oldExtentIsZero)
    if (this.batchMode) this.splayNode(endNode)

    if (this.batchMode) {
      if (startNode !== endNode && endNode.left !== startNode) this.rotateNodeRight(startNode)
    } else {
      startNode.priority = -1
      this.bubbleNodeUp(startNode)
      endNode.priority = -2
      this.bubbleNodeUp(endNode)
    }

    startNode.right = null
    startNode.inputExtent = startNode.inputLeftExtent
    startNode.outputExtent = startNode.outputLeftExtent
    startNode.isChangeStart = false

    let outputStart = startNode.outputLeftExtent
    let outputOldExtent = traversalDistance(endNode.outputLeftExtent, startNode.outputLeftExtent)

    let endNodeInputRightExtent = traversalDistance(endNode.inputExtent, endNode.inputLeftExtent)
    let endNodeOutputRightExtent = traversalDistance(endNode.outputExtent, endNode.outputLeftExtent)
    endNode.inputLeftExtent = inputNewEnd
    endNode.inputExtent = traverse(endNode.inputLeftExtent, endNodeInputRightExtent)
    endNode.outputLeftExtent = traverse(startNode.outputLeftExtent, newExtent)
    endNode.outputExtent = traverse(endNode.outputLeftExtent, endNodeOutputRightExtent)
    endNode.isChangeEnd = false
    endNode.newText = null

    let outputNewExtent = traversalDistance(endNode.outputLeftExtent, startNode.outputLeftExtent)

    if (startNode.isChangeEnd) {
      if (!this.batchMode) {
        startNode.priority = this.generateRandom()
        this.bubbleNodeDown(startNode)
      }
    } else {
      this.deleteNode(startNode)
    }

    if (endNode.isChangeStart) {
      if (!this.batchMode) {
        endNode.priority = this.generateRandom()
        this.bubbleNodeDown(endNode)
      }
    } else {
      this.deleteNode(endNode)
    }

    return {
      start: outputStart,
      oldExtent: outputOldExtent,
      newExtent: outputNewExtent
    }
  }

  isChangedAtInputPosition (inputPosition) {
    this.iterator.seekToInputPosition(inputPosition)
    return this.iterator.inChange()
  }

  isChangedAtOutputPosition (outputPosition) {
    this.iterator.seekToOutputPosition(outputPosition)
    return this.iterator.inChange()
  }

  translateInputPosition (inputPosition) {
    this.iterator.seekToInputPosition(inputPosition)
    let overshoot = traversalDistance(inputPosition, this.iterator.getInputStart())
    let outputPosition = minPoint(traverse(this.iterator.getOutputStart(), overshoot), this.iterator.getOutputEnd())
    this.splayNode(this.iterator.getCurrentNode())
    return outputPosition
  }

  translateOutputPosition (outputPosition) {
    this.iterator.seekToOutputPosition(outputPosition)
    let overshoot = traversalDistance(outputPosition, this.iterator.getOutputStart())
    let inputPosition = minPoint(traverse(this.iterator.getInputStart(), overshoot), this.iterator.getInputEnd())
    this.splayNode(this.iterator.getCurrentNode())
    return inputPosition
  }

  getChanges () {
    return this.iterator.getChanges()
  }

  deleteNode (node) {
    node.priority = Infinity
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

      if (this.batchMode) this.splayNode(node.parent)
    } else {
      this.root = null
    }

    this.nodesCount--
  }

  bubbleNodeUp (node) {
    while (node.parent && node.priority < node.parent.priority) {
      if (node === node.parent.left) {
        this.rotateNodeRight(node)
      } else {
        this.rotateNodeLeft(node)
      }
    }
  }

  bubbleNodeDown (node) {
    let rightAncestor

    while (true) {
      let leftChildPriority = node.left ? node.left.priority : Infinity
      let rightChildPriority = node.right ? node.right.priority : Infinity

      if (leftChildPriority < rightChildPriority && leftChildPriority < node.priority) {
        this.rotateNodeRight(node.left)
      } else if (rightChildPriority < node.priority) {
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

  generateRandom () {
    return Math.random()
  }
}
