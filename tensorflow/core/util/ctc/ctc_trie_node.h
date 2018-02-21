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

/* Code adapted from https://github.com/timediv/tensorflow-with-kenlm */

#ifndef TENSORFLOW_CORE_UTIL_CTC_CTC_TRIE_NODE_H_
#define TENSORFLOW_CORE_UTIL_CTC_CTC_TRIE_NODE_H_

#include <algorithm>
#include <iostream>
#include <vector>
#include "ctc_vocabulary.h"

namespace tensorflow {
namespace ctc {
  
class TrieNode {
  public:
    TrieNode(int label) : label(label),
      prefixCount(0) {
      // std::cout << "New node:  " << label << std::endl;
    }

    ~TrieNode() {
      childLabels.clear();
      children.clear();
    }

    // we're building the trie from a SparseTensorValue
    // each insertion is a dense vector of int labels
    void Insert(std::vector<int> word) {
      if (word.empty()) return;
      prefixCount++;
      int wordChar = word.at(0);
      if (wordChar <= 26 && wordChar >=0 ) {
        // search for child node in word vector
        TrieNode *child;
        int ind = ChildLabelSearch(wordChar);
        if (ind < 0) {
          child = new TrieNode(wordChar);
          childLabels.push_back(wordChar);
          children.push_back(child);
        } else {
          child = children.at(ind);
        }
        word.erase(word.begin());
        child->Insert(word);
      }
    }

    int GetLabel() {
      return label;
    }

    void WriteToStream(std::ofstream& out) {
      out << label << " " << prefixCount << std::endl;
      // recursive call
      for (TrieNode* c : children) {
        c->WriteToStream(out);
      }
    }

    static void ReadFromStream(std::ifstream& in, TrieNode* &obj) {
      int label, prefix_count;
      in >> label >> prefix_count;

      obj = new TrieNode(label);
      obj->ReadNode(in);

      std::vector<int> cLabs;
      std::vector<TrieNode*> childs;
      for (int i = 0; i < prefix_count; ++i) {
        TrieNode* c;
        ReadFromStream(in, c);
        obj->childLabels.push_back(c->label);
        obj->children.push_back(c);
      }
    }

  private:
    int label;
    int prefixCount;
    std::vector<int> childLabels;
    std::vector<TrieNode*> children;

    // TODO: sort insertion into child vector
    int ChildLabelSearch(int label) {
      int i=0;
      for (int l : childLabels) {
        if (l == label) {
          return i;
        }
        ++i;
      }
      return -1;
    }

    void ReadNode(std::ifstream& in) {
      in >> label;
      in >> prefixCount;
    }

}; // TrieNode
} // namespace ctc
} // namespace tensorflow

#endif
