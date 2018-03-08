/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Collection of scoring classes that can be extended and provided to the
// CTCBeamSearchDecoder to incorporate additional scoring logic (such as a
// language model).
//
// To build a custom scorer extend and implement the pure virtual methods from
// BeamScorerInterface. The default CTC decoding behavior is implemented
// through BaseBeamScorer.

#ifndef TENSORFLOW_CORE_UTIL_CTC_CTC_BEAM_SCORER_H_
#define TENSORFLOW_CORE_UTIL_CTC_CTC_BEAM_SCORER_H_

#include "tensorflow/core/util/ctc/ctc_beam_entry.h"
#include "tensorflow/core/util/ctc/ctc_vocabulary.h"

namespace tensorflow {
namespace ctc {

using namespace ctc_beam_search;

// Base implementation of a beam scorer used by default by the decoder that can
// be subclassed and provided as an argument to CTCBeamSearchDecoder, if complex
// scoring is required. Its main purpose is to provide a thin layer for
// integrating language model scoring easily.
template <typename CTCBeamState>
class BaseBeamScorer {
 public:
  virtual ~BaseBeamScorer() {}
  // State initialization.
  virtual void InitializeState(CTCBeamState* root) const {}
  // ExpandState is called when expanding a beam to one of its children.
  // Called at most once per child beam. In the simplest case, no state
  // expansion is done.
  virtual void ExpandState(const CTCBeamState& from_state, int from_label,
                           CTCBeamState* to_state, int to_label) const {}
  // ExpandStateEnd is called after decoding has finished. Its purpose is to
  // allow a final scoring of the beam in its current state, before resorting
  // and retrieving the TopN requested candidates. Called at most once per beam.
  virtual void ExpandStateEnd(CTCBeamState* state) const {}
  // GetStateExpansionScore should be an inexpensive method to retrieve the
  // (cached) expansion score computed within ExpandState. The score is
  // multiplied (log-addition) with the input score at the current step from
  // the network.
  //
  // The score returned should be a log-probability. In the simplest case, as
  // there's no state expansion logic, the expansion score is zero.
  virtual float GetStateExpansionScore(const CTCBeamState& state,
                                       float previous_score) const {
    return previous_score;
  }
  // GetStateEndExpansionScore should be an inexpensive method to retrieve the
  // (cached) expansion score computed within ExpandStateEnd. The score is
  // multiplied (log-addition) with the final probability of the beam.
  //
  // The score returned should be a log-probability.
  virtual float GetStateEndExpansionScore(const CTCBeamState& state) const {
    return 0;
  }
}; // BaseBeamScorer

class TrieBeamScorer : public BaseBeamScorer<TrieBeamState> {
 public:
  virtual ~TrieBeamScorer() {
    delete vocabulary;
    delete trieRoot;
  }
  TrieBeamScorer(std::vector<std::vector<char>> vocab_list) {
    vocabulary = new Vocabulary(vocab_list);

    trieRoot = new TrieNode(26);
    for (std::vector<char> word : vocab_list) {
        trieRoot->Insert(word);
    }
  }

  TrieBeamScorer(const char *dictionary_path) {
    vocabulary = new Vocabulary(dictionary_path);
    std::vector<std::vector<char>> vocab_list = vocabulary->GetVocabList();

    trieRoot = new TrieNode(26);
    for (std::vector<char> word : vocab_list) {
      trieRoot->Insert(word);
    }
  }

  // State initialization
  void InitializeState(TrieBeamState* root) const override {
    root->incomplete_word_trie_node = trieRoot;
  }
  // ExpandState is called when expanding a beam to one of its children.
  // Called at most once per child beam. In the simplest case, no state
  // expansion is done.
  void ExpandState(const TrieBeamState& from_state, int from_label,
                           TrieBeamState* to_state, int to_label)
                           const override {
    // In this case the prefix has a trie, indicating that it is a valid prefix within out dictionary
    // Ensure that from state has a trie node
    TrieNode *node;
    if ((node = from_state.incomplete_word_trie_node) == nullptr) {
      std::cout << "from state nullptr" << std::endl;
      return;
    }
    // ensure that to state is a child of the from_state
    if ((node = node->GetChildAt(to_label)) == nullptr) {
      std::cout << "no to_label" << std::endl;
      return;
    }

    CopyState(to_state, from_state);
    to_state->incomplete_word += to_label;
    to_state->incomplete_word_trie_node = node;
  }
  // ExpandStateEnd is called after decoding has finished. Its purpose is to
  // allow a final scoring of the beam in its current state, before resorting
  // and retrieving the TopN requested candidates. Called at most once per beam.
  void ExpandStateEnd(TrieBeamState* state) const override {}
  // GetStateExpansionScore should be an inexpensive method to retrieve the
  // (cached) expansion score computed within ExpandState. The score is
  // multiplied (log-addition) with the input score at the current step from
  // the network.
  //
  // The score returned should be a log-probability. In the simplest case, as
  // there's no state expansion logic, the expansion score is zero.
  float GetStateExpansionScore(const TrieBeamState& state,
                                       float previous_score) const override {
    return previous_score;
  }
  // GetStateEndExpansionScore should be an inexpensive method to retrieve the
  // (cached) expansion score computed within ExpandStateEnd. The score is
  // multiplied (log-addition) with the final probability of the beam.
  //
  // The score returned should be a log-probability.
  float GetStateEndExpansionScore(const TrieBeamState& state) const override {
    return 0;
  }

 private:
  Vocabulary *vocabulary;
  TrieNode *trieRoot;

  void CopyState(TrieBeamState* to_state, const TrieBeamState& from_state) const {
    to_state->incomplete_word_trie_node = from_state.incomplete_word_trie_node;
    to_state->incomplete_word = from_state.incomplete_word;
  }
}; // TrieBeamState

}  // namespace ctc
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_UTIL_CTC_CTC_BEAM_SCORER_H_
