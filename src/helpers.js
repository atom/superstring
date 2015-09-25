export function addToSet (target, source) {
  source.forEach(target.add, target)
}
