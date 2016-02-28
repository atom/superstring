# atom-patch [![Build Status](https://travis-ci.org/atom/atom-patch.svg?branch=master)](https://travis-ci.org/atom/atom-patch)

This data structure efficiently represents a transformation from input to output text, and it's useful for aggregating and combining changes that occur at different points in time and space.

## Contributing

```bash
# clone this repository
git clone https://github.com/atom/atom-patch

# clone flatbuffers repository and checkout the version tested with this library
git clone https://github.com/google/flatbuffers
git checkout 959866b

# compile flatbuffers
pushd flatbuffers/build/XCode/
xcodebuild
popd

cd atom-patch
npm install
```

Please, note that we have included a patched version of the flatbuffers
javascript library under `vendor/flatbuffers.js`, because the original one
doesn't play well with Node.js `require`.

### Recompiling Patch's Flatbuffer Schema

You can recompile `src/serialization-schema.fbs` by running:

```bash
pushd src
../../flatbuffers/flatc --js serialization-schema.fbs
popd
```

Then, you will have to change that file's last line to:

```diff
// Exports for Node.js and RequireJS
- this.Serialization = Serialization;
+ module.exports = Serialization;
```

### Testing

You can use `npm test` or `npm run tdd` to run the test suite.
