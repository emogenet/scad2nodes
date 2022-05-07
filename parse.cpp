
// stuff we need
#include <array>
#include <stack>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#include <unordered_map>

// DFA states for the parser, constants and names
#define PARSER_STATES           \
  x(InOpArgs)                   \
  x(InOpName)                   \
  x(AfterOpEnd)                 \
  x(WaitingForOp)               \
  x(WaitingForOpArgs)           \
  x(WaitingForOpBodyOrOpEnd)    \
  x(InvalidAndLastParserState)  \

#define x(arg) k##arg,
  enum ParserState {
    PARSER_STATES
  };
#undef x

static const char *getParserStateName(
  const ParserState &parserState
) {
#define x(arg) if(k##arg==parserState) return "k" #arg;
  PARSER_STATES
#undef x
  return "unkndown parser state";
}

// operator names found in CSG file, constants, names matcher
#define OP_TYPE_NAMES   \
  x(cube)               \
  x(hull)               \
  x(text)               \
  x(color)              \
  x(group)              \
  x(union)              \
  x(circle)             \
  x(import)             \
  x(offset)             \
  x(render)             \
  x(sphere)             \
  x(square)             \
  x(polygon)            \
  x(cylinder)           \
  x(minkowski)          \
  x(difference)         \
  x(multmatrix)         \
  x(polyhedron)         \
  x(intersection)       \
  x(linear_extrude)     \
  x(rotate_extrude)     \
  x(no_such_type_known) \

enum OpType {
  #define x(arg) k_##arg,
    OP_TYPE_NAMES
  #undef x
};

static auto match(
  const char *s0,
  const std::vector<char> &s1
) {
  return 0==strcmp(s0, &(s1[0]));
}

static auto getOpType(
  const std::vector<char> &str
) {
  #define x(arg) if(match(#arg, str)) return k_##arg;
    OP_TYPE_NAMES
  #undef x
  printf("unknown keyword %s\n", &(str[0]));
  return k_no_such_type_known;
}

// speck128_128: 128bit block, 128 bit key encryption primitive
#define HASH_SIZE 32
static auto speck128_128(
  uint64_t       *out,   // (O) 128 bit result, will only get written to
  uint64_t const *inp,   // (I) 128 bit data block
  uint64_t const *key    // (I) 128 bit key
)
{
  // load arguments
  uint64_t y = inp[0];
  uint64_t x = inp[1];
  uint64_t b = key[0];
  uint64_t a = key[1];

  // algorithm specifics
  #define SPECK_ROUNDS 32
  #define ROR(x, r) ((x >> r) | (x << (64 - r)))
  #define ROL(x, r) ((x << r) | (x >> (64 - r)))
  #define R(x, y, k) (x = ROR(x, 8), x += y, x ^= k, y = ROL(y, 3), y ^= x)

  // apply speck rounds to input using key
  R(x, y, b);
  for(int i=0; i<(SPECK_ROUNDS-1); ++i) {
    R(a, b, i);
    R(x, y, b);
  }

  // output
  out[0] = y;
  out[1] = x;
}

// speck-based collision-resistant 256-bit hash
static auto speck_and_add(
  uint8_t hash[HASH_SIZE], // (I/O) hash, 256 bits==32 bytes
  const uint8_t *input,    // (I) data to hash
  const size_t &size       // (I) data size
) {

  // two random initial encryption keys
  uint8_t keys[] = {
    0xd0, 0xae, 0x67, 0x52, 0x12, 0x8a, 0xe2, 0x5d, 0x35, 0x00, 0x3c, 0x7a, 0x2c, 0x8c, 0x07, 0xb6, // 128 bits
    0xb4, 0xea, 0x05, 0x3e, 0xf9, 0x05, 0xbf, 0xe8, 0x44, 0x40, 0xcf, 0xda, 0x92, 0x0b, 0x0c, 0x8e, // 256 bits
  };

  // xor the incoming hash into the keys
  for(int i=0; i<sizeof(keys); ++i) {
    keys[i] ^= hash[i];
  }

  // hash buffer block by block with the two keys
  auto p = input;
  auto swap = false;
  size_t left = size;
  while(0<left) {

    // prepare one input block
    uint64_t inp[2];  // 128bits==16 bytes
    size_t n = std::min(left, sizeof(inp));
    memset(inp, 0x27, sizeof(inp)); // initialize with padding
    memcpy(inp, p, n);              // copy input in

    // encrypt input block into two encrypted blocks (with two different keys)
    uint64_t out[4];  // 256 bits
    speck128_128(((swap ? 0: 2) + out), inp, (0 + (uint64_t*)keys));
    speck128_128(((swap ? 2: 0) + out), inp, (2 + (uint64_t*)keys));

    // xor resulting encrypted buffers into the keys
    for(int i=0; i<sizeof(keys); ++i) {
      keys[i] ^= ((uint8_t*)out)[i];
    }

    // move on to next block
    swap ^= true;
    left -= n;
    p += n;
  }

  // copy result to hash
  memcpy(hash, keys, sizeof(keys));
}

// print out 256 bit hex hash (debug)
static auto hexHash(
  const std::array<uint8_t, HASH_SIZE> &hash
) {
  std::string r;
  for(const auto &c:hash) {
    char buf[8];
    sprintf(buf, "%02X", (int)c);
    r += std::string(buf);
  }
  return r;
}

// CSG operator node (primitives, transforms, booleans, etc ...)
struct Operator {

  OpType type;
  int ref_count;
  int start_line;
  uint8_t tree_marker;
  std::vector<char> args;
  std::vector<char> name;
  std::vector<Operator *> children;

  mutable bool hash_ready;
  mutable uint64_t uniqueId;
  mutable std::array<uint8_t, HASH_SIZE> hash;

  // compute the hash of the operator: hash(hash(name), hash(args), hash(subtree))
  const std::array<uint8_t, HASH_SIZE> &getHash() const {

    // only compute hash once
    if(!hash_ready) {

      // lambda to add a string to the hash
      auto hash_and_add = [&](
        std::array<uint8_t, HASH_SIZE> &hash,
        const std::vector<char> &buffer
      ) {
          speck_and_add(
            (uint8_t*)&(hash[0]),
            (uint8_t*)&(buffer[0]),
            buffer.size()
          );
      };

      // hash initial value
      memset(&(hash[0]), 0x56, HASH_SIZE);
      hash_ready = true;

      // add name, args and sub-tree
      hash_and_add(hash, name);
      hash_and_add(hash, args);
      for(const auto &child:children) {
        const auto &childHash = child->getHash();
        speck_and_add(
          (uint8_t*)&(hash[0]),
          (uint8_t*)&(childHash[0]),
          childHash.size()
        );
      }
    }

    // yield the hash
    return hash;
  }

  Operator(
    const std::vector<char> &_name,
    const std::vector<char> &_args,
    const uint8_t _tree_marker,
    const int _start_line
  ) : name(_name),
      args(_args),
      ref_count(1),
      uniqueId(0ULL),
      hash_ready(false),
      tree_marker(_tree_marker),
      start_line(_start_line)
  {
    type = getOpType(name);
  }
};

// print start of line indentation at level "depth"
static auto indentPrint(
  int depth
) {
  while(depth--) {
    putchar(' ');
    putchar(' ');
  }
}

// print out tree as parsed (debug)
static void show(
  const Operator *op,
  int depth = 0
) {
  indentPrint(depth);
  printf(
    "%s(%s) { // %p\n",
    &(op->name[0]),
    &(op->args[0]),
    op
  );
  for(const auto &c:op->children) {
    show(c, 1+depth);
  }
  indentPrint(depth);
  printf("}\n");
}

// unique id pool for node labelling
static auto getUniqueId() {
  static uint64_t counter;
  return ++counter;
}

// emit python representation of a tree of nodes
static uint64_t emit(
  const Operator *op
) {

  // only emit if not already done
  if(0==op->uniqueId) {

    // get all children id's (and emit them if needed)
    std::vector<int> ids;
    for(const auto &c:op->children) {
      ids.push_back(emit(c));
    }

    // emit node's specifics
    op->uniqueId = getUniqueId();
    printf(
      "n_%06d = Node(\"%s\", args = '%s', inputNodes = [",
      (int)(op->uniqueId),
      &(op->name[0]),
      &(op->args[0])
    );
    for(auto &id:ids) {
      printf("n_%06d, ", (int)id);
    }
    printf("], code_line=%d);\n", op->start_line);
    //printf("]);\n");
  }

  // return the node's unique id
  return op->uniqueId;
}

// simplify a tree by removing dead code
static Operator *remove_dead_code(
  Operator *op
) {

  // for all children
  size_t i = 0;
  while(i<op->children.size()) {

    // remove dead code from child
    op->children[i] = remove_dead_code(op->children[i]);

    // if entire child is gone, remove it altogether from children array
    if(0==op->children[i]) {
      std::swap(op->children.back(), op->children[i]);
      op->children.pop_back();
      continue;
    }
    ++i;
  }

  // see if node itself is dead
  auto t = op->type;
  auto dead = (
    (0==op->children.size()) && (
      (k_linear_extrude == t)||
      (k_intersection == t)  ||
      (k_difference == t)    ||
      (k_multmatrix == t)    ||
      (k_minkowski == t)     ||
      (k_color == t)         ||
      (k_offset == t)        ||
      (k_render == t)        ||
      (k_group == t)         ||
      (k_union == t)         ||
      (k_hull == t)
    )
  );

  // yes, kill it and return 0
  if(dead) {
    delete op;
    return 0;
  }

  // see if node is a NOP
  auto nop = (
    (1==op->children.size()) && (
      (k_intersection == t)  ||
      (k_difference == t)    ||
      (k_minkowski == t)     ||
      (k_render == t)        ||
      (k_group == t)         ||
      (k_union == t)
    )
  );

  // yes, replace it by its unique child
  if(nop) {
    auto c = op->children[0];
    delete op;
    op = c;
  }
  return op;
}

// hash table for merkle-tree style tree compression
using Value = Operator *;
using Key = const std::array<uint8_t, HASH_SIZE>;
using Hash = struct {
  auto operator()(
    const Key &key
  ) const {
    std::hash<std::string> hasher;
    return hasher(
      std::string(
        (const char *)&(key[0]),
        key.size()
      )
    );
  }
};
using Map = std::unordered_map<Key, Value, Hash>;

// try to replace identical subtrees
Operator *compress(
  Map &map,
  Operator *node
) {

  // compress children
  for(auto &c:node->children) {
    c = compress(map, c);
  }

  // compute node merkle tree hash, replace if exists, insert if it doesn't
  const auto &hash = node->getHash();
  auto it = map.find(hash);
  if(map.end()==it) {
    map[hash] = node;
//printf("node %s is original\n", &(node->name[0]));
  } else {
//printf("node %s is a dupe of %s\n", &(node->name[0]), &(it->second->name[0]));
//printf("DUPE:\n"); show(node);
//printf("ORIG (refcount=%d):\n", (int)it->second->ref_count);
//show(it->second);
    ++(it->second->ref_count);
    node = it->second;
  }
  return node;
}

// a simple 4x4 matrix
struct Mat4 {

  // data
  double m[4][4];

  // construct to identity
  Mat4() {
    memset(m, 0, sizeof(m));
    m[0][0] = 1.0;
    m[1][1] = 1.0;
    m[2][2] = 1.0;
    m[3][3] = 1.0;
  }

  // accessor
  double *operator[](
    size_t i
  ) {
    return &(m[i][0]);
  }

  // post-multiply by a matrix in a multmatrix argument string
  void postMul(
    const std::vector<char> &args
  ) {

    // parse multmatrix args
    double n[4][4];
    auto nbRead = sscanf(
      &(args[0]),
      "[[%lf, %lf, %lf, %lf], [%lf, %lf, %lf, %lf], [%lf, %lf, %lf, %lf], [%lf, %lf, %lf, %lf]]",
      &(n[0][0]), &(n[0][1]), &(n[0][2]), &(n[0][3]),
      &(n[1][0]), &(n[1][1]), &(n[1][2]), &(n[1][3]),
      &(n[2][0]), &(n[2][1]), &(n[2][2]), &(n[2][3]),
      &(n[3][0]), &(n[3][1]), &(n[3][2]), &(n[3][3])
    );
    if(16 != nbRead) {
      abort();
    }

    // concatenate to current matrix
    double r[4][4];
    memset(r, 0, sizeof(r));
    for(int j=0; j<4; ++j) {
      for(int i=0; i<4; ++i) {
        for(int k=0; k<4; ++k) {
          r[i][j] += (m[i][k] * n[k][j]);
        }
      }
    }
    memcpy(m, r, sizeof(r));
  }

};

// concatenate matrices in sub-tree
Operator *concat(
  Operator *op
) {

  // roll up all children that are strings of matrices
  Mat4 mat;
  auto c = op;
  int count = 0;
  while(
    (1==c->children.size()) &&
    (k_multmatrix == c->type)
  ) {
    mat.postMul(c->args);
    c = c->children[0];
    ++count;
  }

  if(1<count) {

    op->children[0] = c;

    char buf[1024];
    sprintf(
      buf,
      "[[%f, %f, %f, %f], [%f, %f, %f, %f], [%f, %f, %f, %f], [%f, %f, %f, %f]]",
      mat[0][0], mat[0][1], mat[0][2], mat[0][3],
      mat[1][0], mat[1][1], mat[1][2], mat[1][3],
      mat[2][0], mat[2][1], mat[2][2], mat[2][3],
      mat[3][0], mat[3][1], mat[3][2], mat[3][3]
    );

    size_t n = strlen(buf);
    op->args.resize(1 + n);
    memcpy(&(op->args[0]), buf, n);
    op->args.back() = 0;
  }

  for(auto &c:op->children) {
    c = concat(c);
  }
  return op;
}

static auto parseSTDIN() {

  // parser state
  int lineNumber = 0;
  uint8_t tree_marker = 0;
  std::vector<char> opArgs;
  std::vector<char> opName;
  std::vector<char> currLine;
  auto parserState = kWaitingForOp;

  // tree state
  Operator *top = 0;
  std::stack<Operator *> stack;

  // parser error handler
  #define ERROR(msg) error(msg, __LINE__)
  auto error = [&](
    const char *msg,
    int parserLine
  ) {

    // print error message
    printf(
      "\n"
      "error at lineNo=%d, charNo=%d, parserLine=%d, parserState=%d (%s)\n",
      lineNumber,
      (int)currLine.size(),
      (int)parserLine,
      (int)parserState,
      getParserStateName(parserState)
    );

    // show where error message occured
    currLine.push_back(0);
    printf("%s\n", &(currLine[0]));

    for(int i=0; i<currLine.size()-1; ++i) {
      putchar(' ');
    }
    putchar('^');
    putchar('\n');

    for(int i=0; i<currLine.size()-1; ++i) {
      putchar(' ');
    }
    putchar('|');
    putchar('\n');

    for(int i=0; i<currLine.size()-1; ++i) {
      putchar(' ');
    }
    printf("here: %s\n", msg);

    // bail
    exit(1);
  };

  // DFA main code
  auto processChar = [&](
    int c
  ) {

    // save original char (debug)
    // auto original_char = c;

    // simplify DFA by replacing space-like things by a space
    if(
      ('\t' == c) ||
      ('\r' == c) ||
      ('\f' == c)
    ) {
      c = ' ';
    }

    // simplify DFA by handling EOL outside of it
    if('\n'!=c) {
      currLine.push_back(c);
    } else {
      currLine.clear();
      ++lineNumber;
      c = ' ';
    }

    // debug
    // printf("state=%s got '%c' (was '%c')\n", getParserStateName(parserState), c, original_char);

    // deterministic finite automaton parser, each switch case is a state
    switch(parserState) {
      case kWaitingForOp:
        if (
          ('A'<=c && c<='Z')  ||
          ('a'<=c && c<='z')  ||
          ('_'==c)
        ) {
          opName.clear();
          opName.push_back(c);
          parserState = kInOpName;
        } else if (' '==c) {
          parserState = kWaitingForOp;
        } else if (
          ('!'==c) ||
          ('%'==c) ||
          ('#'==c)
        ) {
          tree_marker = c;
          parserState = kWaitingForOp;
        } else {
          ERROR("expected: [_a-zA-Z\\s!%#]");
        }
        break;
      case kInOpName:
        if (
          ('0'<=c && c<='9')  ||
          ('A'<=c && c<='Z')  ||
          ('a'<=c && c<='z')  ||
          ('_'==c)
        ) {
          opName.push_back(c);
        } else if ('('==c) {
          opArgs.clear();
          opName.push_back(0);
          parserState = kInOpArgs;
        } else if (' '==c) {
          opName.push_back(0);
          parserState = kWaitingForOpArgs;
        } else {
          ERROR("expected: [(_0-9a-zA-Z\\s]");
        }
        break;
      case kWaitingForOpArgs:
        if ('('==c) {
          opArgs.clear();
          parserState = kInOpArgs;
        } else if (' '==c) {
          parserState = kWaitingForOpArgs;
        } else {
          ERROR("expected: [(\\s]");
        }
        break;
      case kInOpArgs:
        if (')'==c) {   // TODO: account for parens in strings and parens nesting

          opArgs.push_back(0);
          parserState = kWaitingForOpBodyOrOpEnd;

          auto newOp = new Operator(opName, opArgs, tree_marker, lineNumber);
          tree_marker = 0;

          if(stack.empty()) {
            if(0!=top) {
              ERROR("stack empty but top is not NULL!");
              abort();
            }
            top = newOp;
          } else {
            stack.top()->children.push_back(newOp);
          }
          stack.push(newOp);

        } else {
          opArgs.push_back(char(c));
        }
        break;
      case kWaitingForOpBodyOrOpEnd:
        if (' '==c) {
          parserState = kWaitingForOpBodyOrOpEnd;
        } else if (';'==c) {
          parserState = kAfterOpEnd;
          stack.pop();
        } else if ('{'==c) {
          parserState = kWaitingForOp;
        } else {
          ERROR("unexpected");
        }
        break;
      case kAfterOpEnd:
        if (
          ('A'<=c && c<='Z')  ||
          ('a'<=c && c<='z')  ||
          ('_'==c)
        ) {
          opName.clear();
          parserState = kInOpName;
          opName.push_back(c);
        } else if (' '==c) {
          parserState = kAfterOpEnd;
        } else if ('}'==c) {
          parserState = kAfterOpEnd;
          stack.pop();
        } else {
          ERROR("unexpected");
        }
        break;
      case kInvalidAndLastParserState:
        abort();
    }
  };

  // push an initial group
  processChar('g');
  processChar('r');
  processChar('o');
  processChar('u');
  processChar('p');
  processChar('(');
  processChar(')');
  processChar('{');
  processChar('\n');

  // process STDIN char by char
  while(1) {
    auto c = getchar();
    auto original_char = c;
    if(c<0) {
      break;
    }
    processChar(c);
  }

  // close initial group and return it
  processChar('}');
  return top;
}

int main() {
  Map map;
  auto top = parseSTDIN();
  top = remove_dead_code(top);
  top = concat(top);
  top = compress(map, top);
  printf("output = n_%06d\n", (int)emit(top));
  return 0;
}

