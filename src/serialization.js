import FlatBuffers from '../vendor/flatbuffers'
import Schema from './serialization-schema_generated'

export function deserializeChanges (serializedChanges) {
  let buffer = new FlatBuffers.ByteBuffer(serializedChanges.bytes)
  buffer.setPosition(serializedChanges.position)
  let patch = Schema.Patch.getRootAsPatch(buffer)
  let changes = []
  for (var i = 0; i < patch.changesLength(); i++) {
    let serializedChange = patch.changes(i)
    let oldStart = serializedChange.oldStart()
    let newStart = serializedChange.newStart()
    let oldExtent = serializedChange.oldExtent()
    let newExtent = serializedChange.newExtent()
    let change = {
      oldStart: {row: oldStart.row(), column: oldStart.column()},
      newStart: {row: newStart.row(), column: newStart.column()},
      oldExtent: {row: oldExtent.row(), column: oldExtent.column()},
      newExtent: {row: newExtent.row(), column: newExtent.column()}
    }
    let newText = serializedChange.newText()
    let oldText = serializedChange.oldText()
    if (newText != null) change.newText = newText
    if (oldText != null) change.oldText = oldText

    changes.push(change)
  }

  return changes
}

export function serializeChanges (changesToSerialize) {
  let builder = new FlatBuffers.Builder(1)
  let changes = changesToSerialize.map(({oldStart, newStart, oldExtent, newExtent, oldText, newText}) => {
    let serializedNewText, serializedOldText
    if (newText != null) serializedNewText = builder.createString(newText)
    if (oldText != null) serializedOldText = builder.createString(oldText)
    Schema.Change.startChange(builder)
    Schema.Change.addOldStart(builder, Schema.Point.createPoint(builder, oldStart.row, oldStart.column))
    Schema.Change.addNewStart(builder, Schema.Point.createPoint(builder, newStart.row, newStart.column))
    Schema.Change.addOldExtent(builder, Schema.Point.createPoint(builder, oldExtent.row, oldExtent.column))
    Schema.Change.addNewExtent(builder, Schema.Point.createPoint(builder, newExtent.row, newExtent.column))
    if (serializedNewText) Schema.Change.addNewText(builder, serializedNewText)
    if (serializedOldText) Schema.Change.addOldText(builder, serializedOldText)
    return Schema.Change.endChange(builder)
  })

  let changesVector = Schema.Patch.createChangesVector(builder, changes)
  Schema.Patch.startPatch(builder)
  Schema.Patch.addChanges(builder, changesVector)
  builder.finish(Schema.Patch.endPatch(builder))
  let buffer = builder.dataBuffer()
  return {position: buffer.position(), bytes: buffer.bytes()}
}
