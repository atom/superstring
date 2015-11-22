import Random from 'random-seed'
import {ZERO_POINT, INFINITY_POINT, traverse, traversalDistance, compare as comparePoints, min as minPoint} from './point-helpers'
import {getExtent, getPrefix, getSuffix} from './text-helpers'
import Iterator from './iterator'
import Node from './node'

export default class Patch {
  constructor (seed = Date.now()) {
    this.randomGenerator = new Random(seed)
    this.root = null
  }

  spliceText (start, replacedExtent, replacementText) {
    this.splice(start, replacedExtent, getExtent(replacementText), {text: replacementText})
  }

  splice (outputStart, replacedExtent, replacementExtent, options) {
    let outputOldEnd = traverse(outputStart, replacedExtent)
    let outputNewEnd = traverse(outputStart, replacementExtent)

    let {startNode, prefix} = this.insertSpliceStart(outputStart)
    let {endNode, suffix, suffixExtent} = this.insertSpliceEnd(outputOldEnd)
    startNode.priority = -1
    this.bubbleNodeUp(startNode)
    endNode.priority = -2
    this.bubbleNodeUp(endNode)

    startNode.right = null
    startNode.inputExtent = startNode.inputLeftExtent
    startNode.outputExtent = startNode.outputLeftExtent

    let endNodeOutputRightExtent = traversalDistance(endNode.outputExtent, endNode.outputLeftExtent)
    endNode.outputLeftExtent = traverse(outputNewEnd, suffixExtent)
    endNode.outputExtent = traverse(endNode.outputLeftExtent, endNodeOutputRightExtent)
    endNode.changeText = prefix + options.text + suffix

    startNode.priority = this.generateRandom()
    this.bubbleNodeDown(startNode)
    endNode.priority = this.generateRandom()
    this.bubbleNodeDown(endNode)
  }

  insertSpliceStart (spliceOutputStart) {
    let {startNode, endNode, startNodeOutputPosition} = this.insertSpliceBoundary(spliceOutputStart, true)

    let prefix
    if (comparePoints(spliceOutputStart, startNodeOutputPosition) === 0) {
      prefix = ''
    } else {
      prefix = getPrefix(endNode.changeText, traversalDistance(spliceOutputStart, startNodeOutputPosition))
    }

    return {startNode, prefix}
  }

  insertSpliceEnd (spliceOutputEnd) {
    let {startNode, endNode, startNodeOutputPosition, endNodeOutputPosition} = this.insertSpliceBoundary(spliceOutputEnd, false)

    let suffix, suffixExtent
    if (comparePoints(spliceOutputEnd, endNodeOutputPosition) === 0) {
      suffix = ''
      suffixExtent = ZERO_POINT
    } else {
      suffix = getSuffix(endNode.changeText, traversalDistance(spliceOutputEnd, startNodeOutputPosition))
      suffixExtent = traversalDistance(endNodeOutputPosition, spliceOutputEnd)
    }

    return {endNode, suffix, suffixExtent}
  }

  insertSpliceBoundary (boundaryOutputPosition, insertingChangeStart) {
    let node = this.root

    if (!node) {
      this.root = new Node(null, boundaryOutputPosition, boundaryOutputPosition)
      this.root.isChangeStart = insertingChangeStart
      return buildInsertedNodeResult(this.root)
    }

    let inputOffset = ZERO_POINT
    let outputOffset = ZERO_POINT
    let maxInputPosition = INFINITY_POINT
    let nodeInputPosition, nodeOutputPosition
    let containingStartNode, containingEndNode
    let containingStartNodeOutputPosition, containingEndNodeOutputPosition

    while (true) {
      nodeInputPosition = traverse(inputOffset, node.inputLeftExtent)
      nodeOutputPosition = traverse(outputOffset, node.outputLeftExtent)

      if (node.isChangeStart) {
        let result = visitChangeStart()
        if (result) return result
      } else {
        let result = visitChangeEnd()
        if (result) return result
      }
    }

    function visitChangeStart() {
      if (comparePoints(boundaryOutputPosition, nodeOutputPosition) < 0) { // boundaryOutputPosition < nodeOutputPosition
        containingEndNode = null
        containingEndNodeOutputPosition = null

        if (node.left) {
          descendLeft()
          return null
        } else {
          return insertLeftNode()
        }
      } else { // boundaryOutputPosition >= nodeOutputPosition
        containingStartNode = node
        containingStartNodeOutputPosition = nodeOutputPosition

        if (node.right) {
          descendRight()
          return null
        } else {
          if (insertingChangeStart || containingEndNode) {
            return {
              startNode: containingStartNode,
              endNode: containingEndNode,
              startNodeOutputPosition: containingStartNodeOutputPosition,
              endNodeOutputPosition: containingEndNodeOutputPosition
            }
          } else {
            return insertRightNode()
          }
        }
      }
    }

    function visitChangeEnd () {
      if (comparePoints(boundaryOutputPosition, nodeOutputPosition) <= 0) { // boundaryOutputPosition <= nodeOutputPosition
        containingEndNode = node
        containingEndNodeOutputPosition = nodeOutputPosition

        if (node.left) {
          descendLeft()
          return null
        } else {
          if (!insertingChangeStart || containingStartNode) {
            return {
              startNode: containingStartNode,
              endNode: containingEndNode,
              startNodeOutputPosition: containingStartNodeOutputPosition,
              endNodeOutputPosition: containingEndNodeOutputPosition
            }
          } else {
            return insertLeftNode()
          }
        }
      } else { // boundaryOutputPosition > nodeOutputPosition
        containingStartNode = null
        containingStartNodeOutputPosition = null

        if (node.right) {
          descendRight()
          return null
        } else {
          return insertRightNode()
        }
      }
    }

    function descendLeft () {
      maxInputPosition = nodeInputPosition
      node = node.left
    }

    function descendRight () {
      inputOffset = traverse(inputOffset, node.inputLeftExtent)
      outputOffset = traverse(outputOffset, node.outputLeftExtent)
      node = node.right
    }

    function insertLeftNode () {
      let outputLeftExtent = traversalDistance(boundaryOutputPosition, outputOffset)
      let inputLeftExtent = minPoint(outputLeftExtent, node.inputLeftExtent)
      let newNode = new Node(node, inputLeftExtent, outputLeftExtent)
      newNode.isChangeStart = insertingChangeStart
      node.left = newNode
      return buildInsertedNodeResult(newNode)
    }

    function insertRightNode () {
      let outputLeftExtent = traversalDistance(boundaryOutputPosition, nodeOutputPosition)
      let inputLeftExtent = minPoint(outputLeftExtent, traversalDistance(maxInputPosition, nodeInputPosition))
      let newNode = new Node(node, inputLeftExtent, outputLeftExtent)
      newNode.isChangeStart = insertingChangeStart
      node.right = newNode
      return buildInsertedNodeResult(newNode)
    }

    function buildInsertedNodeResult (insertedNode) {
      if (insertingChangeStart) {
        return {
          startNode: insertedNode,
          startNodeOutputPosition: boundaryOutputPosition,
          endNode: containingEndNode,
          endNodeOutputPosition: containingEndNodeOutputPosition
        }
      } else {
        return {
          startNode: containingStartNode,
          startNodeOutputPosition: containingStartNodeOutputPosition,
          endNode: insertedNode,
          endNodeOutputPosition: boundaryOutputPosition
        }
      }
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
    while (true) {
      let leftChildPriority = node.left ? node.left.priority : Infinity
      let rightChildPriority = node.right ? node.right.priority : Infinity

      if (leftChildPriority < rightChildPriority && leftChildPriority < node.priority) {
        this.rotateNodeRight(node.left)
      } else if (rightChildPriority < node.priority) {
        this.rotateNodeLeft(node.right)
      } else {
        break
      }
    }
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

  buildIterator () {
    return new Iterator(this)
  }

  getChanges () {
    let changes = []
    let iterator = this.buildIterator()
    while (iterator.currentNode && iterator.currentNode.left) {
      iterator.descendLeft()
    }

    while (!iterator.next().done) {
      if (iterator.inChange()) {
        changes.push({
          start: iterator.getOutputStart(),
          replacedExtent: traversalDistance(iterator.getInputEnd(), iterator.getInputStart()),
          replacementText: iterator.getChangeText()
        })
      }
    }

    return changes
  }
}
