#include "text-slice.h"
#include "text-buffer.h"
#include "regex.h"
#include <algorithm>
#include <cassert>
#include <cwctype>
#include <sstream>
#include <unordered_map>
#include <vector>

using std::equal;
using std::move;
using std::pair;
using std::string;
using std::u16string;
using std::vector;
using MatchOptions = Regex::MatchOptions;
using MatchResult = Regex::MatchResult;
using SubsequenceMatch = TextBuffer::SubsequenceMatch;

uint32_t TextBuffer::MAX_CHUNK_SIZE_TO_COPY = 1024;

static Text EMPTY_TEXT;

struct TextBuffer::Layer {
  Layer *previous_layer;
  Patch patch;
  optional<Text> text;
  bool uses_patch;

  Point extent_;
  uint32_t size_;
  uint32_t snapshot_count;

  explicit Layer(Text &&text) :
    previous_layer{nullptr},
    text{move(text)},
    uses_patch{false},
    extent_{this->text->extent()},
    size_{this->text->size()},
    snapshot_count{0} {}

  explicit Layer(Layer *previous_layer) :
    previous_layer{previous_layer},
    patch{Patch()},
    uses_patch{true},
    extent_{previous_layer->extent()},
    size_{previous_layer->size()},
    snapshot_count{0} {}

  static inline Point previous_column(Point position) {
    return Point(position.row, position.column - 1);
  }

  bool is_above_layer(const Layer *layer) const {
    Layer *predecessor = previous_layer;
    while (predecessor) {
      if (predecessor == layer) return true;
      predecessor = predecessor->previous_layer;
    }
    return false;
  }

  uint16_t character_at(Point position) const {
    if (!uses_patch) return text->at(position);

    auto change = patch.get_change_starting_before_new_position(position);
    if (!change) return previous_layer->character_at(position);
    if (position < change->new_end) {
      return change->new_text->at(position.traversal(change->new_start));
    } else {
      return previous_layer->character_at(
        change->old_end.traverse(position.traversal(change->new_end))
      );
    }
  }

  ClipResult clip_position(Point position, bool splay = false) {
    if (!uses_patch) return text->clip_position(position);
    if (snapshot_count > 0) splay = false;

    auto preceding_change = splay ?
      patch.grab_change_starting_before_new_position(position) :
      patch.get_change_starting_before_new_position(position);
    if (!preceding_change) return previous_layer->clip_position(position);

    if (position < preceding_change->new_end) {
      uint32_t preceding_change_base_offset =
        previous_layer->clip_position(preceding_change->old_start).offset;
      uint32_t preceding_change_current_offset =
        preceding_change_base_offset +
        preceding_change->preceding_new_text_size -
        preceding_change->preceding_old_text_size;

      ClipResult position_within_preceding_change =
        preceding_change->new_text->clip_position(
          position.traversal(preceding_change->new_start)
        );

      if (position_within_preceding_change.offset == 0 && preceding_change->old_start.column > 0) {
        if (preceding_change->new_text->content.front() == '\n' &&
            previous_layer->character_at(previous_column(preceding_change->old_start)) == '\r') {
          return {
            previous_column(preceding_change->new_start),
            preceding_change_current_offset - 1
          };
        }
      }

      return {
        preceding_change->new_start.traverse(position_within_preceding_change.position),
        preceding_change_current_offset + position_within_preceding_change.offset
      };
    } else {
      ClipResult base_location = previous_layer->clip_position(
        preceding_change->old_end.traverse(position.traversal(preceding_change->new_end))
      );

      ClipResult result = {
        preceding_change->new_end.traverse(base_location.position.traversal(preceding_change->old_end)),
        base_location.offset +
          preceding_change->preceding_new_text_size -
          preceding_change->preceding_old_text_size +
          preceding_change->new_text->size() -
          preceding_change->old_text_size
      };

      if (result.position == preceding_change->new_end && base_location.offset < previous_layer->size()) {
        uint16_t previous_character = 0;
        if (preceding_change->new_text->size() > 0) {
          previous_character = preceding_change->new_text->content.back();
        } else if (preceding_change->old_start.column > 0) {
          previous_character = previous_layer->character_at(previous_column(preceding_change->old_start));
        }

        if (previous_character == '\r' && previous_layer->character_at(base_location.position) == '\n') {
          return {
            previous_column(preceding_change->new_end),
            result.offset - 1
          };
        }
      }

      return result;
    }
  }

  template <typename Callback>
  inline bool for_each_chunk_in_range(Point start, Point goal_position,
                                      const Callback &callback, bool splay = false) {
    // *goal_position = clip_position(*goal_position, splay).position;
    // Point current_position = clip_position(start, splay).position;
    Point current_position = start;

    if (!uses_patch) {
      TextSlice slice = TextSlice(*text).slice({current_position, goal_position});
      return !slice.empty() && callback(slice);
    }

    if (snapshot_count > 0) splay = false;

    Point base_position;
    auto change = splay ?
      patch.grab_change_starting_before_new_position(current_position) :
      patch.get_change_starting_before_new_position(current_position);
    if (!change) {
      base_position = current_position;
    } else if (current_position < change->new_end) {
      TextSlice slice = TextSlice(*change->new_text).slice({
        current_position.traversal(change->new_start),
        goal_position.traversal(change->new_start)
      });
      if (!slice.empty() && callback(slice)) return true;
      base_position = change->old_end;
      current_position = change->new_end;
    } else {
      base_position = change->old_end.traverse(current_position.traversal(change->new_end));
    }

    auto changes = splay ?
      patch.grab_changes_in_new_range(current_position, goal_position) :
      patch.get_changes_in_new_range(current_position, goal_position);
    for (const auto &change : changes) {
      if (base_position < change.old_start) {
        if (previous_layer->for_each_chunk_in_range(base_position, change.old_start, callback)) {
          return true;
        }
      }

      TextSlice slice = TextSlice(*change.new_text)
        .prefix(Point::min(change.new_end, goal_position).traversal(change.new_start));
      if (!slice.empty() && callback(slice)) return true;

      base_position = change.old_end;
      current_position = change.new_end;
    }

    if (current_position < goal_position) {
      return previous_layer->for_each_chunk_in_range(
        base_position,
        base_position.traverse(goal_position.traversal(current_position)),
        callback
      );
    }

    return false;
  }

  Point position_for_offset(uint32_t goal_offset) const {
    if (text) {
      return text->position_for_offset(goal_offset);
    } else {
      return patch.new_position_for_new_offset(
        goal_offset,
        [this](Point old_position) {
          return previous_layer->clip_position(old_position).offset;
        },
        [this](uint32_t old_offset) {
          return previous_layer->position_for_offset(old_offset);
        }
      );
    }
  }

  Point extent() const { return extent_; }

  uint32_t size() const { return size_; }

  u16string text_in_range(Range range, bool splay = false) {
    u16string result;
    for_each_chunk_in_range(
      clip_position(range.start).position,
      clip_position(range.end).position,
      [&result](TextSlice slice) {
        result.insert(result.end(), slice.begin(), slice.end());
        return false;
      },
      splay
    );
    return result;
  }

  vector<TextSlice> chunks_in_range(Range range) {
    vector<TextSlice> result;
    for_each_chunk_in_range(
      clip_position(range.start).position,
      clip_position(range.end).position,
      [&result](TextSlice slice) {
        result.push_back(slice);
        return false;
      }
    );
    if (result.empty()) result.push_back(TextSlice(EMPTY_TEXT));
    return result;
  }

  vector<pair<const char16_t *, uint32_t>> primitive_chunks() {
    vector<pair<const char16_t *, uint32_t>> result;
    for_each_chunk_in_range(Point(), Point::max(), [&result](TextSlice slice) {
      result.push_back({slice.data(), slice.size()});
      return false;
    });
    if (result.empty()) result.push_back({u"", 0});
    return result;

  }

  template <typename Callback>
  void scan_in_range(const Regex &regex, Range range, const Callback &callback, bool splay = false) {
    Regex::MatchData match_data(regex);
    range.start = clip_position(range.start).position;
    range.end = clip_position(range.end).position;

    uint32_t minimum_match_row = range.start.row;
    Range last_match{Point::max(), Point::max()};
    bool last_match_is_pending = false;
    bool done = false;
    Text chunk_continuation;
    TextSlice slice_to_search;
    Point chunk_start_position = range.start;
    Point last_search_end_position = range.start;
    Point slice_to_search_start_position = range.start;

    for_each_chunk_in_range(range.start, range.end, [&](TextSlice chunk) {
      Point chunk_end_position = chunk_start_position.traverse(chunk.extent());
      while (last_search_end_position < chunk_end_position) {
        if (last_search_end_position >= chunk_start_position) {
          TextSlice remaining_chunk = chunk
            .suffix(last_search_end_position.traversal(chunk_start_position));

          // When we find a match that ends with a CR at a chunk boundary, we wait to
          // report the match until we can see the next chunk. If the next chunk starts
          // with an LF, we decrement the end column because Points within CRLF line
          // endings are not valid.
          if (last_match_is_pending) {
            if (!remaining_chunk.empty() && remaining_chunk.front() == '\n') {
              chunk_continuation.splice(Point(), Point(), Text{u"\r"});
              slice_to_search_start_position.column--;
              last_match.end.column--;
            }

            last_match_is_pending = false;
            if (callback(last_match)) {
              done = true;
              return true;
            }
          }

          if (!chunk_continuation.empty()) {
            chunk_continuation.append(remaining_chunk.prefix(MAX_CHUNK_SIZE_TO_COPY));
            slice_to_search = TextSlice(chunk_continuation);
          } else {
            slice_to_search = remaining_chunk;
          }
        } else {
          slice_to_search = TextSlice(chunk_continuation);
        }

        Point slice_to_search_end_position =
          slice_to_search_start_position.traverse(slice_to_search.extent());

        int options = 0;
        if (slice_to_search_start_position.column == 0) options |= MatchOptions::IsBeginningOfLine;
        if (slice_to_search_end_position == range.end) {
          options |= MatchOptions::IsEndSearch;
          if (range.end == clip_position(Point{range.end.row, UINT32_MAX}).position) {
            options |= MatchOptions::IsEndOfLine;
          }
        }

        MatchResult match_result = regex.match(
          slice_to_search.data(),
          slice_to_search.size(),
          match_data,
          options
        );

        switch (match_result.type) {
          case MatchResult::Error:
            chunk_continuation.clear();
            return true;

          case MatchResult::None:
            last_search_end_position = slice_to_search_start_position.traverse(slice_to_search.extent());
            slice_to_search_start_position = last_search_end_position;
            minimum_match_row = slice_to_search_start_position.row;
            chunk_continuation.clear();
            break;

          case MatchResult::Partial:
            last_search_end_position = slice_to_search_start_position.traverse(slice_to_search.extent());
            if (chunk_continuation.empty() || match_result.start_offset > 0) {
              Point partial_match_position = slice_to_search.position_for_offset(match_result.start_offset,
                minimum_match_row - slice_to_search_start_position.row
              );
              slice_to_search_start_position = slice_to_search_start_position.traverse(partial_match_position);
              minimum_match_row = slice_to_search_start_position.row;
              chunk_continuation.assign(slice_to_search.suffix(partial_match_position));
            }
            break;

          case MatchResult::Full:
            Point match_start_position = slice_to_search.position_for_offset(
              match_result.start_offset,
              minimum_match_row - slice_to_search_start_position.row
            );
            Point match_end_position = slice_to_search.position_for_offset(
              match_result.end_offset,
              minimum_match_row - slice_to_search_start_position.row
            );
            last_match = Range{
              slice_to_search_start_position.traverse(match_start_position),
              slice_to_search_start_position.traverse(match_end_position)
            };

            last_search_end_position = last_match.end;
            if (match_end_position == match_start_position) {
              last_search_end_position.column++;
              if (clip_position(last_search_end_position).position == last_match.end) {
                last_search_end_position.column = 0;
                last_search_end_position.row++;
              }
            }
            minimum_match_row = last_search_end_position.row;

            slice_to_search_start_position = last_search_end_position;
            if (slice_to_search_start_position >= chunk_start_position) {
              chunk_continuation.clear();
            } else {
              chunk_continuation.assign(slice_to_search.suffix(match_end_position));
            }

            // If the match ends with a CR at the end of a chunk, continue looking
            // at the next chunk, in case that chunk starts with an LF.
            if (match_result.end_offset == slice_to_search.size() && slice_to_search.back() == '\r') {
              last_match_is_pending = true;
              continue;
            }

            if (callback(last_match)) {
              done = true;
              return true;
            }
        }
      }

      chunk_start_position = chunk_end_position;
      return false;
    }, splay);

    if (last_match_is_pending) {
      callback(last_match);
    } else if (!done && last_match.end != range.end) {
      static char16_t EMPTY[] = {0};
      unsigned options = MatchOptions::IsEndSearch;
      if (range.end.column == 0) options |= MatchOptions::IsBeginningOfLine;
      if (range.end == clip_position(Point{range.end.row, UINT32_MAX}).position) {
        options |= MatchOptions::IsEndOfLine;
      }
      MatchResult match_result = regex.match(EMPTY, 0, match_data, options);
      if (match_result.type == MatchResult::Partial || match_result.type == MatchResult::Full) {
        callback(Range{range.end, range.end});
      }
    }
  }

  optional<Range> find_in_range(const Regex &regex, Range range, bool splay = false) {
    optional<Range> result;
    scan_in_range(regex, range, [&result](Range match_range) -> bool {
      result = match_range;
      return true;
    }, splay);
    return result;
  }

  vector<Range> find_all_in_range(const Regex &regex, Range range, bool splay = false) {
    vector<Range> result;
    scan_in_range(regex, range, [&result](Range match_range) -> bool {
      result.push_back(match_range);
      return false;
    }, splay);
    return result;
  }

  unsigned find_and_mark_all_in_range(MarkerIndex &index, MarkerIndex::MarkerId first_id,
                                      bool exclusive, const Regex &regex, Range range, bool splay = false) {
    unsigned id = first_id;
    scan_in_range(regex, range, [&index, &id, exclusive](Range match_range) -> bool {
      index.insert(id, match_range.start, match_range.end);
      index.set_exclusive(id, exclusive);
      id++;
      return false;
    }, splay);
    return id - first_id;
  }

  struct SubsequenceMatchVariant {
    size_t query_index = 0;
    std::vector<uint32_t> match_indices;
    int16_t score = 0;

    bool operator<(const SubsequenceMatchVariant &other) const {
      return query_index < other.query_index;
    }
  };

  vector<SubsequenceMatch> find_words_with_subsequence_in_range(u16string query, const u16string &extra_word_characters, Range range) {
    const size_t MAX_WORD_LENGTH = 80;
    size_t query_index = 0;
    Point position;
    Point current_word_start;
    u16string current_word;
    u16string raw_query = query;

    if (query.size() > MAX_WORD_LENGTH) return vector<SubsequenceMatch>{};

    std::transform(query.begin(), query.end(), query.begin(), std::towlower);

    // First, find the start position of all words matching the given
    // subsequence.
    std::unordered_map<u16string, vector<Point>> substring_matches;

    for_each_chunk_in_range(
      clip_position(range.start).position,
      clip_position(range.end).position,
      [&] (TextSlice chunk) -> bool {
        for (uint16_t c : chunk) {
          bool is_word_character =
            std::iswalnum(c) ||
            std::find(extra_word_characters.begin(), extra_word_characters.end(), c) != extra_word_characters.end();

          if (is_word_character) {
            if (current_word.empty()) current_word_start = position;
            current_word += c;
            if (query_index < query.size() && towlower(c) == query[query_index]) {
              query_index++;
            }
          } else {
            if (!current_word.empty()) {
              if (query_index == query.size() && current_word.size() <= MAX_WORD_LENGTH) {
                substring_matches[current_word].push_back(current_word_start);
              }
              query_index = 0;
              current_word.clear();
            }
          }

          if (c == '\n') {
            position.row++;
            position.column = 0;
          } else {
            position.column++;
          }
        }

        return false;
      });

    if (!current_word.empty() && query_index == query.size() && current_word.size() <= MAX_WORD_LENGTH) {
      substring_matches[current_word].push_back(current_word_start);
    }

    // Next, calculate a score for each word indicating the quality of the
    // match against the query.

    static const unsigned consecutive_bonus = 5;
    static const unsigned subword_start_with_case_match_bonus = 10;
    static const unsigned subword_start_with_case_mismatch_bonus = 9;
    static const unsigned mismatch_penalty = 1;
    static const unsigned leading_mismatch_penalty = 3;

    vector<SubsequenceMatch> matches;

    for (auto entry : substring_matches) {
      const u16string &word = entry.first;
      const vector<Point> &start_positions = entry.second;

      vector<SubsequenceMatchVariant> match_variants {{}};
      vector<SubsequenceMatchVariant> new_match_variants;

      for (size_t i = 0; i < word.size(); i++) {
        uint16_t c = towlower(word[i]);

        for (auto match_variant = match_variants.begin(); match_variant != match_variants.end();) {
          if (match_variant->query_index < query.size()) {
            // If the current word character matches the next character of
            // the query for this match variant, create a *new* match variant
            // that consumes the matching character.
            if (c == query[match_variant->query_index]) {
              SubsequenceMatchVariant new_match = *match_variant;
              new_match.query_index++;

              if (i == 0 ||
                  !std::iswalnum(word[i - 1]) ||
                  (std::iswlower(word[i - 1]) && std::iswupper(word[i]))) {
                new_match.score += word[i] == raw_query[match_variant->query_index]
                  ? subword_start_with_case_match_bonus
                  : subword_start_with_case_mismatch_bonus;
              }

              if (!new_match.match_indices.empty() && new_match.match_indices.back() == i - 1) {
                new_match.score += consecutive_bonus;
              }

              new_match.match_indices.push_back(i);
              new_match_variants.push_back(new_match);
            }

            // For the current match variant, treat the current character as
            // a mismatch regardless of whether it matched above. This
            // reserves the chance for the next character to be consumed by a
            // match with higher overall value.
            if (i < 3) {
              match_variant->score -= leading_mismatch_penalty;
            } else {
              match_variant->score -= mismatch_penalty;
            }

            // If a match variant does *not* match the current character (and is therefore
            // ineligible for the consecutive match bonus on the next character), its
            // potential for future scoring is determined entirely by its `query_index`.
            //
            // These match variants are ordered by ascending `query_index`. If multiple
            // match variants have the same `query_index`, they are ordered by ascending
            // `score`.
            //
            // If there is another match variant with the same `query_index` and a greater
            // or equal `score`, discard the current match variant.
            auto next_match_variant = match_variant + 1;
            if (next_match_variant != match_variants.end() && next_match_variant->query_index == match_variant->query_index) {
              match_variant = match_variants.erase(match_variant);
            } else {
              ++match_variant;
            }
          } else {
            ++match_variant;
          }
        }

        // Add all of the newly-computed match variants to the list. Avoid creating duplicate
        // match variants with the same query index unless the new variant (which is
        // by definition eligible for the consecutive match bonus on the next character) has
        // a lower score than an existing variant. Maintain the invariant that match variants
        // are ordered by ascending `query_index` and ascending `score`.
        for (const SubsequenceMatchVariant &new_variant : new_match_variants) {
          auto existing_match_iter = std::lower_bound(match_variants.begin(), match_variants.end(), new_variant);
          if (existing_match_iter != match_variants.end() && new_variant.query_index == existing_match_iter->query_index) {
            if (new_variant.score >= existing_match_iter->score) {
              *existing_match_iter = new_variant;
              continue;
            }
          }
          match_variants.insert(existing_match_iter, new_variant);
        }
        new_match_variants.clear();
      }

      SubsequenceMatchVariant *best_match = nullptr;
      for (auto &match_variant : match_variants) {
        if (match_variant.query_index == query.size()) {
          if (!best_match || best_match->score < match_variant.score) {
            best_match = &match_variant;
          }
        }
      }

      matches.push_back(SubsequenceMatch{word, start_positions, best_match->match_indices, best_match->score});
    }

    std::sort(matches.begin(), matches.end(), [] (const SubsequenceMatch &a, const SubsequenceMatch &b) {
      // Doing it this way helps us avoid sorting ambiguity keeping the ordering the same across platforms.
      if (a.score > b.score) return true;
      if (b.score > a.score) return false;
      return a.word < b.word;
    });

    return matches;
  }

  bool is_modified(const Layer *base_layer) {
    if (size() != base_layer->size()) return true;

    bool result = false;
    uint32_t start_offset = 0;
    for_each_chunk_in_range(Point(), extent(), [&](TextSlice chunk) {
      if (chunk.text == &(*base_layer->text) ||
          equal(chunk.begin(), chunk.end(), base_layer->text->begin() + start_offset)) {
        start_offset += chunk.size();
        return false;
      }
      result = true;
      return true;
    });

    return result;
  }

  bool has_astral() {
    bool result = false;
    for_each_chunk_in_range(Point(), extent(), [&](TextSlice chunk) {
      for (auto ch : chunk) {
        if ((ch & 0xf800) == 0xd800) {
          result = true;
          return true;
        }
      }
      return false;
    });
    return result;
  }
};

TextBuffer::TextBuffer(u16string &&text) :
  base_layer{new Layer(move(text))},
  top_layer{base_layer} {}

TextBuffer::TextBuffer() :
  base_layer{new Layer(Text{})},
  top_layer{base_layer} {}

TextBuffer::~TextBuffer() {
  Layer *layer = top_layer;
  while (layer) {
    Layer *previous_layer = layer->previous_layer;
    delete layer;
    layer = previous_layer;
  }
}

TextBuffer::TextBuffer(const std::u16string &text) :
  TextBuffer{u16string{text.begin(), text.end()}} {}

void TextBuffer::reset(Text &&new_base_text) {
  bool has_snapshot = false;
  auto layer = top_layer;
  while (layer) {
    if (layer->snapshot_count > 0) {
      has_snapshot = true;
      break;
    }
    layer = layer->previous_layer;
  }

  if (has_snapshot) {
    set_text(move(new_base_text.content));
    flush_changes();
    return;
  }

  layer = top_layer->previous_layer;
  while (layer) {
    Layer *previous_layer = layer->previous_layer;
    delete layer;
    layer = previous_layer;
  }

  top_layer->extent_ = new_base_text.extent();
  top_layer->size_ = new_base_text.size();
  top_layer->text = move(new_base_text);
  top_layer->patch.clear();
  top_layer->uses_patch = false;
  base_layer = top_layer;
  top_layer->previous_layer = nullptr;
}

Patch TextBuffer::get_inverted_changes(const Snapshot *snapshot) const {
  vector<const Patch *> patches;
  Layer *layer = top_layer;
  while (layer != &snapshot->base_layer) {
    patches.insert(patches.begin(), &layer->patch);
    layer = layer->previous_layer;
  }

  Patch combination;
  bool left_to_right = true;
  for (const Patch *patch : patches) {
    combination.combine(*patch, left_to_right);
    left_to_right = !left_to_right;
  }

  TextSlice base{*snapshot->base_layer.text};
  Patch result;
  for (auto change : combination.get_changes()) {
    result.splice(
      change.old_start,
      change.new_end.traversal(change.new_start),
      change.old_end.traversal(change.old_start),
      *change.new_text,
      Text{base.slice({change.old_start, change.old_end})},
      change.new_text->size()
    );
  }
  return result;
}

void TextBuffer::serialize_changes(Serializer &serializer) {
  serializer.append(top_layer->size_);
  top_layer->extent_.serialize(serializer);
  if (top_layer == base_layer) {
    Patch().serialize(serializer);
    return;
  }

  if (top_layer->previous_layer == base_layer) {
    top_layer->patch.serialize(serializer);
    return;
  }

  vector<const Patch *> patches;
  Layer *layer = top_layer;
  while (layer != base_layer) {
    patches.insert(patches.begin(), &layer->patch);
    layer = layer->previous_layer;
  }

  Patch combination;
  bool left_to_right = true;
  for (const Patch *patch : patches) {
    combination.combine(*patch, left_to_right);
    left_to_right = !left_to_right;
  }
  combination.serialize(serializer);
}

bool TextBuffer::deserialize_changes(Deserializer &deserializer) {
  if (top_layer != base_layer || base_layer->previous_layer) return false;
  top_layer = new Layer(base_layer);
  top_layer->size_ = deserializer.read<uint32_t>();
  top_layer->extent_ = Point(deserializer);
  top_layer->patch = Patch(deserializer);
  return true;
}

const Text &TextBuffer::base_text() const {
  return *base_layer->text;
}

Point TextBuffer::extent() const {
  return top_layer->extent();
}

uint32_t TextBuffer::size() const {
  return top_layer->size();
}

optional<uint32_t> TextBuffer::line_length_for_row(uint32_t row) {
  if (row > extent().row) return optional<uint32_t>{};
  return top_layer->clip_position(Point{row, UINT32_MAX}, true).position.column;
}

const uint16_t *TextBuffer::line_ending_for_row(uint32_t row) {
  if (row > extent().row) return nullptr;

  static uint16_t LF[] = {'\n', 0};
  static uint16_t CRLF[] = {'\r', '\n', 0};
  static uint16_t NONE[] = {0};

  const uint16_t *result = NONE;
  top_layer->for_each_chunk_in_range(
    clip_position(Point(row, UINT32_MAX)).position,
    Point(row + 1, 0),
    [&result](TextSlice slice) {
      auto begin = slice.begin();
      if (begin == slice.end()) return false;
      result = (*begin == '\r') ? CRLF : LF;
      return true;
    }, true);
  return result;
}

void TextBuffer::with_line_for_row(uint32_t row, const std::function<void(const char16_t *, uint32_t)> &callback) {
  u16string result;
  uint32_t column = 0;
  uint32_t slice_count = 0;
  Point line_end = clip_position({row, UINT32_MAX}).position;
  top_layer->for_each_chunk_in_range({row, 0}, line_end, [&](TextSlice slice) -> bool {
    auto begin = slice.begin(), end = slice.end();
    size_t size = end - begin;
    slice_count++;
    column += size;
    if (slice_count == 1 && column == line_end.column) {
      callback(slice.data(), slice.size());
      return true;
    } else {
      result.insert(result.end(), begin, end);
      return false;
    }
  });

  if (slice_count != 1) {
    callback(result.c_str(), result.size());
  }
}

optional<u16string> TextBuffer::line_for_row(uint32_t row) {
  if (row > extent().row) return optional<u16string>{};
  return text_in_range({{row, 0}, {row, UINT32_MAX}});
}

ClipResult TextBuffer::clip_position(Point position) {
  return top_layer->clip_position(position, true);
}

Point TextBuffer::position_for_offset(uint32_t offset) {
  return top_layer->position_for_offset(offset);
}

u16string TextBuffer::text() {
  return top_layer->text_in_range(Range{Point(), extent()});
}

uint16_t TextBuffer::character_at(Point position) const {
  return top_layer->character_at(position);
}

u16string TextBuffer::text_in_range(Range range) {
  return top_layer->text_in_range(range, true);
}

vector<TextSlice> TextBuffer::chunks() const {
  return top_layer->chunks_in_range({{0, 0}, extent()});
}

void TextBuffer::set_text(u16string &&new_text) {
  set_text_in_range(Range{Point(0, 0), extent()}, move(new_text));
}

void TextBuffer::set_text(const u16string &new_text) {
  set_text_in_range(Range{Point(0, 0), extent()}, u16string(new_text));
}

void TextBuffer::set_text_in_range(Range old_range, u16string &&string) {
  if (top_layer == base_layer || top_layer->snapshot_count > 0) {
    top_layer = new Layer(top_layer);
  }

  auto start = clip_position(old_range.start);
  auto end = old_range.end == old_range.start ? start : clip_position(old_range.end);
  Point deleted_extent = end.position.traversal(start.position);
  Text new_text{move(string)};
  Point inserted_extent = new_text.extent();
  Point new_range_end = start.position.traverse(new_text.extent());
  uint32_t deleted_text_size = end.offset - start.offset;
  top_layer->extent_ = new_range_end.traverse(top_layer->extent_.traversal(end.position));
  top_layer->size_ += new_text.size() - deleted_text_size;
  top_layer->patch.splice(
    start.position,
    deleted_extent,
    inserted_extent,
    optional<Text>{},
    move(new_text),
    deleted_text_size
  );

  auto change = top_layer->patch.grab_change_starting_before_new_position(start.position);
  if (change && change->old_text_size == change->new_text->size()) {
    bool change_is_noop = true;
    auto new_text_iter = change->new_text->begin();
    top_layer->previous_layer->for_each_chunk_in_range(
      change->old_start,
      change->old_end,
      [&change_is_noop, &new_text_iter](TextSlice chunk) {
        auto new_text_end = new_text_iter + chunk.size();
        if (!std::equal(new_text_iter, new_text_end, chunk.begin())) {
          change_is_noop = false;
          return true;
        }
        new_text_iter = new_text_end;
        return false;
      });
    if (change_is_noop) {
      top_layer->patch.splice_old(change->old_start, Point(), Point());
    }
  }
}

optional<Range> TextBuffer::find(const Regex &regex, Range range) const {
  return top_layer->find_in_range(regex, range, false);
}

vector<Range> TextBuffer::find_all(const Regex &regex, Range range) const {
  return top_layer->find_all_in_range(regex, range, false);
}

unsigned TextBuffer::find_and_mark_all(MarkerIndex &index, MarkerIndex::MarkerId next_id,
                                       bool exclusive, const Regex &regex, Range range) const {
  return top_layer->find_and_mark_all_in_range(index, next_id, exclusive, regex, range, false);
}

bool TextBuffer::SubsequenceMatch::operator==(const SubsequenceMatch &other) const {
  return (
    word == other.word &&
    positions == other.positions &&
    match_indices == other.match_indices &&
    score == other.score
  );
}

vector<SubsequenceMatch> TextBuffer::find_words_with_subsequence_in_range(const u16string &query, const u16string &non_word_characters, Range range) const {
  return top_layer->find_words_with_subsequence_in_range(query, non_word_characters, range);
}

bool TextBuffer::is_modified() const {
  return top_layer->is_modified(base_layer);
}

bool TextBuffer::has_astral() {
  return top_layer->has_astral();
}

bool TextBuffer::is_modified(const Snapshot *snapshot) const {
  return top_layer->is_modified(&snapshot->base_layer);
}

string TextBuffer::get_dot_graph() const {
  Layer *layer = top_layer;
  vector<Layer *> layers;
  while (layer) {
    layers.push_back(layer);
    layer = layer->previous_layer;
  }

  std::stringstream result;
  result << "graph { label=\"--- buffer ---\" }\n";
  for (auto begin = layers.rbegin(), iter = begin, end = layers.rend();
       iter != end; ++iter) {
    auto layer = *iter;
    auto index = iter - begin;
    result << "graph { label=\"layer " << index << " (snapshot count " << layer->snapshot_count;
    if (layer == base_layer) result << ", base";
    if (layer->uses_patch) result << ", uses_patch";
    result << "):\" }\n";
    if (layer->text) result << "graph { label=\"text:\n" << *layer->text << "\" }\n";
    if (index > 0) result << layer->patch.get_dot_graph();
  }
  return result.str();
}

size_t TextBuffer::layer_count() const {
  size_t result = 1;
  const Layer *layer = top_layer;
  while (layer->previous_layer) {
    result++;
    layer = layer->previous_layer;
  }
  return result;
}

TextBuffer::Snapshot *TextBuffer::create_snapshot() {
  top_layer->snapshot_count++;
  base_layer->snapshot_count++;
  return new Snapshot(*this, *top_layer, *base_layer);
}

void TextBuffer::flush_changes() {
  if (!top_layer->text) {
    top_layer->text = Text{text()};
    base_layer = top_layer;
    consolidate_layers();
  }
}

uint32_t TextBuffer::Snapshot::size() const {
  return layer.size();
}

Point TextBuffer::Snapshot::extent() const {
  return layer.extent();
}

uint32_t TextBuffer::Snapshot::line_length_for_row(uint32_t row) const {
  return layer.clip_position(Point{row, UINT32_MAX}).position.column;
}

u16string TextBuffer::Snapshot::text_in_range(Range range) const {
  return layer.text_in_range(range);
}

u16string TextBuffer::Snapshot::text() const {
  return layer.text_in_range({{0, 0}, extent()});
}

vector<TextSlice> TextBuffer::Snapshot::chunks_in_range(Range range) const {
  return layer.chunks_in_range(range);
}

vector<TextSlice> TextBuffer::Snapshot::chunks() const {
  return layer.chunks_in_range({{0, 0}, extent()});
}

vector<pair<const char16_t *, uint32_t>> TextBuffer::Snapshot::primitive_chunks() const {
  return layer.primitive_chunks();
}

optional<Range> TextBuffer::Snapshot::find(const Regex &regex, Range range) const {
  return layer.find_in_range(regex, range, false);
}

vector<Range> TextBuffer::Snapshot::find_all(const Regex &regex, Range range) const {
  return layer.find_all_in_range(regex, range, false);
}

vector<SubsequenceMatch> TextBuffer::Snapshot::find_words_with_subsequence_in_range(std::u16string query, const std::u16string &extra_word_characters, Range range) const {
  return layer.find_words_with_subsequence_in_range(query, extra_word_characters, range);
}

const Text &TextBuffer::Snapshot::base_text() const {
  return *base_layer.text;
}

TextBuffer::Snapshot::Snapshot(TextBuffer &buffer, TextBuffer::Layer &layer,
                               TextBuffer::Layer &base_layer)
  : buffer{buffer}, layer{layer}, base_layer{base_layer} {}

void TextBuffer::Snapshot::flush_preceding_changes() {
  if (!layer.text) {
    layer.text = Text{text()};
    if (layer.is_above_layer(buffer.base_layer)) buffer.base_layer = &layer;
    buffer.consolidate_layers();
  }
}

TextBuffer::Snapshot::~Snapshot() {
  assert(layer.snapshot_count > 0);
  layer.snapshot_count--;
  base_layer.snapshot_count--;
  if (layer.snapshot_count == 0 || base_layer.snapshot_count == 0) {
    buffer.consolidate_layers();
  }
}

void TextBuffer::consolidate_layers() {
  Layer *layer = top_layer;
  vector<Layer *> mutable_layers;
  bool needed_by_layer_above = false;

  while (layer) {
    if (needed_by_layer_above || layer->snapshot_count > 0) {
      squash_layers(mutable_layers);
      mutable_layers.clear();
      needed_by_layer_above = true;
    } else {
      if (layer == base_layer) {
        squash_layers(mutable_layers);
        mutable_layers.clear();
      }

      if (layer->text) layer->uses_patch = false;
      mutable_layers.push_back(layer);
    }

    if (!layer->uses_patch) needed_by_layer_above = false;
    layer = layer->previous_layer;
  }

  squash_layers(mutable_layers);
}

void TextBuffer::squash_layers(const vector<Layer *> &layers) {
  size_t layer_index = 0;
  size_t layer_count = layers.size();
  if (layer_count < 2) return;

  // Find the highest layer that has already computed its text.
  optional<Text> text;
  for (layer_index = 0; layer_index < layer_count; layer_index++) {
    if (layers[layer_index]->text) {
      text = move(*layers[layer_index]->text);
      break;
    }
  }

  // Incorporate into that text the patches from all the layers above.
  if (text) {
    layer_index--;
    for (; layer_index + 1 > 0; layer_index--) {
      for (auto change : layers[layer_index]->patch.get_changes()) {
        text->splice(
          change.new_start,
          change.old_end.traversal(change.old_start),
          *change.new_text
        );
      }
    }
  }

  // If there is another layer below these layers, combine their patches into
  // into one. Otherwise, this is the new base layer, so we don't need a patch.
  Patch patch;
  Layer *previous_layer = layers.back()->previous_layer;

  if (previous_layer) {
    layer_index = layer_count - 1;
    patch = move(layers[layer_index]->patch);
    layer_index--;

    bool left_to_right = true;
    for (; layer_index + 1 > 0; layer_index--) {
      patch.combine(layers[layer_index]->patch, left_to_right);
      left_to_right = !left_to_right;
    }
  } else {
    assert(text);
  }

  layers[0]->previous_layer = previous_layer;
  layers[0]->text = move(text);
  layers[0]->patch = move(patch);

  for (layer_index = 1; layer_index < layer_count; layer_index++) {
    delete layers[layer_index];
  }
}
