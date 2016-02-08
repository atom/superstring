let idCounter = 0

export default class Node {
  constructor(parent, inputLeftExtent, outputLeftExtent) {
    this.parent = parent
    this.left = null
    this.right = null
    this.inputLeftExtent = inputLeftExtent
    this.outputLeftExtent = outputLeftExtent
    this.inputExtent = inputLeftExtent
    this.outputExtent = outputLeftExtent

    this.id = ++idCounter
    this.priority = 0
    this.isChangeStart = false
    this.isChangeEnd = false
    this.newText = null
    this.metadata = null
  }
}
