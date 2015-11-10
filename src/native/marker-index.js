import NativeMarkerIndex from '../../build/Release/marker_index'

NativeMarkerIndex.prototype.getRange = function (id) {
  return [this.getStart(id), this.getEnd(id)]
}

export default NativeMarkerIndex
