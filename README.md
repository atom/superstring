# atom-patch [![Build Status](https://travis-ci.org/atom/atom-patch.svg?branch=master)](https://travis-ci.org/atom/atom-patch)

This data structure efficiently represents a transformation from input to output text, and it's useful for aggregating and combining changes that occur at different points in time and space.

## Contributing

```bash
# clone this repository
git clone https://github.com/atom/atom-patch

cd atom-patch
npm install
```

Use `npm test` or `npm run tdd` to run the test suite.

### Recompiling Patch's Flatbuffer Schema

`Patch` uses [flat buffers](https://google.github.io/flatbuffers/) to represent its serialized state. If you want to make any change to the underlying schema you have to download and compile `flatc` first:

```bash
# clone flatbuffers repository and checkout the version tested with this library
git clone https://github.com/google/flatbuffers
git checkout 959866b

# compile flatbuffers
pushd flatbuffers/build/XCode/
xcodebuild
popd
```

This will create a `flatc` executable in flatbuffers top level directory. You can recompile `src/serialization-schema.fbs` by running:

```bash
cd atom-patch
../flatbuffers/flatc -o src --js serialization-schema.fbs
```

After you do that, please make sure to to change the generated file's last line to:

```diff
// Exports for Node.js and RequireJS
- this.Serialization = Serialization;
+ module.exports = Serialization;
```

Please, note that we have included a patched version of the flatbuffers javascript library under vendor/flatbuffers.js because the original one has the same problem.
