import Random from 'random-seed'
import {ZERO_POINT, INFINITY_POINT, traverse, traversalDistance, min as minPoint, isZero as isZeroPoint, compare as comparePoints} from './point-helpers'
import {getExtent} from './text-helpers'
import Iterator from './iterator'

export default class Patch {
  constructor (params = {}) {
    this.combineChanges = (params.combineChanges != null) ? Boolean(params.combineChanges) : true
    this.randomGenerator = new Random(params.seed || Date.now())
    this.root = null
    this.iterator = this.buildIterator()
  }

  buildIterator () {
    return new Iterator(this)
  }

  spliceWithText (start, replacedExtent, replacementText) {
    this.splice(start, replacedExtent, getExtent(replacementText), {text: replacementText})
  }

  splice (outputStart, replacedExtent, replacementExtent, options) {
    if (isZeroPoint(replacedExtent) && isZeroPoint(replacementExtent)) return

    let outputOldEnd = traverse(outputStart, replacedExtent)
    let outputNewEnd = traverse(outputStart, replacementExtent)

    let startNode = this.iterator.insertSpliceBoundary(outputStart)
    startNode.isChangeStart = true

    let endNode = this.iterator.insertSpliceBoundary(outputOldEnd, startNode)
    endNode.isChangeEnd = true

    startNode.priority = -1
    this.bubbleNodeUp(startNode)
    endNode.priority = -2
    this.bubbleNodeUp(endNode)

    startNode.right = null
    startNode.inputExtent = startNode.inputLeftExtent
    startNode.outputExtent = startNode.outputLeftExtent

    endNode.outputExtent = traverse(outputNewEnd, traversalDistance(endNode.outputExtent, endNode.outputLeftExtent))
    endNode.outputLeftExtent = outputNewEnd
    endNode.changeText = options && options.text

    if (endNode.isChangeStart && this.combineChanges) {
      endNode.priority = Infinity
      let rightAncestor = this.bubbleNodeDown(endNode)
      if (endNode.changeText != null) {
        rightAncestor.changeText = endNode.changeText + rightAncestor.changeText
      }
      this.deleteNode(endNode)
    } else if (comparePoints(endNode.outputLeftExtent, startNode.outputLeftExtent) === 0
        && comparePoints(endNode.inputLeftExtent, startNode.inputLeftExtent) === 0) {
      startNode.isChangeStart = endNode.isChangeStart
      this.deleteNode(endNode)
    } else {
      endNode.priority = this.generateRandom()
      this.bubbleNodeDown(endNode)
    }

    if (startNode.isChangeStart && startNode.isChangeEnd && this.combineChanges) {
      startNode.priority = Infinity
      let rightAncestor = this.bubbleNodeDown(startNode)
      if (startNode.changeText != null) {
        rightAncestor.changeText = startNode.changeText + rightAncestor.changeText
      }
      this.deleteNode(startNode)
    } else {
      startNode.priority = this.generateRandom()
      this.bubbleNodeDown(startNode)
    }
  }

  spliceInput (inputStart, replacedExtent, replacementExtent) {
    let replacedExtentIsZero = isZeroPoint(replacedExtent)

    if (replacedExtentIsZero && isZeroPoint(replacementExtent)) {
      let outputStart = this.translateInputPosition(inputStart)
      return {start: outputStart, replacedExtent, replacementExtent}
    }

    let inputOldEnd = traverse(inputStart, replacedExtent)
    let inputNewEnd = traverse(inputStart, replacementExtent)

    let startNode = this.iterator.insertSpliceInputBoundary(inputStart, true, replacedExtentIsZero)
    let endNode = this.iterator.insertSpliceInputBoundary(inputOldEnd, false, replacedExtentIsZero)

    startNode.priority = -1
    this.bubbleNodeUp(startNode)
    endNode.priority = -2
    this.bubbleNodeUp(endNode)

    startNode.right = null
    startNode.inputExtent = startNode.inputLeftExtent
    startNode.outputExtent = startNode.outputLeftExtent
    startNode.isChangeStart = false

    let outputStart = startNode.outputLeftExtent
    let outputReplacedExtent = traversalDistance(endNode.outputLeftExtent, startNode.outputLeftExtent)

    let endNodeInputRightExtent = traversalDistance(endNode.inputExtent, endNode.inputLeftExtent)
    let endNodeOutputRightExtent = traversalDistance(endNode.outputExtent, endNode.outputLeftExtent)
    endNode.inputLeftExtent = inputNewEnd
    endNode.inputExtent = traverse(endNode.inputLeftExtent, endNodeInputRightExtent)
    endNode.outputLeftExtent = traverse(startNode.outputLeftExtent, replacementExtent)
    endNode.outputExtent = traverse(endNode.outputLeftExtent, endNodeOutputRightExtent)
    endNode.isChangeEnd = false
    endNode.changeText = null

    let outputReplacementExtent = traversalDistance(endNode.outputLeftExtent, startNode.outputLeftExtent)

    if (startNode.isChangeEnd) {
      startNode.priority = this.generateRandom()
      this.bubbleNodeDown(startNode)
    } else {
      this.deleteNode(startNode)
    }

    if (endNode.isChangeStart) {
      endNode.priority = this.generateRandom()
      this.bubbleNodeDown(endNode)
    } else {
      this.deleteNode(endNode)
    }

    return {
      start: outputStart,
      replacedExtent: outputReplacedExtent,
      replacementExtent: outputReplacementExtent
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
    return minPoint(traverse(this.iterator.getOutputStart(), overshoot), this.iterator.getOutputEnd())
  }

  translateOutputPosition (outputPosition) {
    this.iterator.seekToOutputPosition(outputPosition)
    let overshoot = traversalDistance(outputPosition, this.iterator.getOutputStart())
    return minPoint(traverse(this.iterator.getInputStart(), overshoot), this.iterator.getInputEnd())
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
    } else {
      this.root = null
    }
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
    return this.randomGenerator.random()
  }
}
