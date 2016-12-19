import NativeMarkerIndex from '../../build/Release/marker_index'

NativeMarkerIndex.prototype.getRange = function (id) {
  return [this.getStart(id), this.getEnd(id)]
}

NativeMarkerIndex.prototype.findStartingAt = function (position) {
  return this.findStartingIn(position, position)
}

NativeMarkerIndex.prototype.findEndingAt = function (position) {
  return this.findEndingIn(position, position)
}

export default NativeMarkerIndex
