// ECE/CS 5544 Assignment 2 starter unifiedpass.cpp
// Lean starter: buildable scaffolds, minimal solved logic.

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Scalar/LoopPassManager.h>
#include <llvm/ADT/BitVector.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/IRBuilder.h>
using namespace llvm;

namespace {

std::string getShortValueName(const Value* V) {
  if (!V) return "(null)";
  if (V->hasName()) return "%" + V->getName().str();
  if (const auto* C = dyn_cast<ConstantInt>(V)) return std::to_string(C->getSExtValue());
  std::string S;
  raw_string_ostream OS(S);
  V->printAsOperand(OS, false);
  return S;
}

// -------------------- Available Expressions (starter) --------------------

struct Expr {
  Instruction::BinaryOps opcode;
  Value* lhs;
  Value* rhs;
  auto operator<=>(const Expr&) const = default;

  static Expr fromBO(const BinaryOperator& BO) {
    return {BO.getOpcode(), BO.getOperand(0), BO.getOperand(1)};
  }
};

raw_ostream& operator<<(raw_ostream& OS, const Expr& E) {
  OS << getShortValueName(E.lhs) << " ";
  switch (E.opcode) {
    case Instruction::Add: OS << "+"; break;
    case Instruction::Sub: OS << "-"; break;
    case Instruction::Mul: OS << "*"; break;
    case Instruction::SDiv:
    case Instruction::UDiv: OS << "/"; break;
    default: OS << "(op)"; break;
  }
  OS << " " << getShortValueName(E.rhs);
  return OS;
}

template <typename T>
void printBitSet(raw_ostream& OS, StringRef label, const BitVector& bits, const std::vector<T>& universe) {
  OS << "  " << label << ": { ";
  bool first = true;
  for (unsigned i = 0; i < bits.size(); ++i) {
    if (!bits.test(i)) continue;
    if (!first) OS << "; ";
    first = false;
    OS << universe[i];
  }
  OS << " }\n";
}

struct AvailablePass : PassInfoMixin<AvailablePass> {
  struct BlockState {
    BitVector in;
    BitVector out;
    BitVector gen;
    BitVector kill;
  };

  static BitVector meetIntersect(const std::vector<BitVector>& ins) {
    if (ins.empty()) return {};
    BitVector out = ins[0];
    for (size_t i = 1; i < ins.size(); ++i) out &= ins[i];
    return out;
  }

  PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
    outs() << "=== ";
    F.printAsOperand(outs(), false);
    outs() << " ===\n";

    std::vector<Expr> universe;
    for (auto& BB : F) {
      for (auto& I : BB) {
        if (auto* BO = dyn_cast<BinaryOperator>(&I)) universe.push_back(Expr::fromBO(*BO));
      }
    }
    std::sort(universe.begin(), universe.end());
    universe.erase(std::unique(universe.begin(), universe.end()), universe.end());

    DenseMap<const BasicBlock*, BlockState> st;
    std::vector<BasicBlock*> order;
    order.push_back(&F.getEntryBlock());
    for (size_t i = 0; i < order.size(); ++i) {
      for (BasicBlock* succ : successors(order[i])) {
        if (std::find(order.begin(), order.end(), succ) == order.end()) order.push_back(succ);
      }
    }

    BitVector all(universe.size(), true);
    for (BasicBlock* BB : order) {
      BlockState bs;
      bs.in = BitVector(universe.size(), false);
      bs.out = all;
      bs.gen = BitVector(universe.size(), false);
      bs.kill = BitVector(universe.size(), false);

      for (Instruction& I : *BB) {
        if (auto* BO = dyn_cast<BinaryOperator>(&I)) {
          Expr e = Expr::fromBO(*BO);
          auto it = std::lower_bound(universe.begin(), universe.end(), e);
          if (it != universe.end() && *it == e) bs.gen.set(static_cast<unsigned>(it - universe.begin()));
        }
        if (!I.getType()->isVoidTy()) {
          for (size_t i = 0; i < universe.size(); ++i) {
            if (universe[i].lhs == &I || universe[i].rhs == &I) bs.kill.set(static_cast<unsigned>(i));
          }
        }
      }

      BitVector notGen = bs.gen;
      bs.kill &= notGen.flip();
      st[BB] = bs;
    }

    bool changed = true;
    while (changed) {
      changed = false;
      for (BasicBlock* BB : order) {
        std::vector<BitVector> predOuts;
        if (BB == &F.getEntryBlock()) predOuts.push_back(BitVector(universe.size(), false));
        for (BasicBlock* pred : predecessors(BB)) predOuts.push_back(st[pred].out);
        if (predOuts.empty()) predOuts.push_back(BitVector(universe.size(), false));

        st[BB].in = meetIntersect(predOuts);

        // TODO(student): implement transfer OUT = GEN U (IN - KILL).
        BitVector newOut = st[BB].in;
        BitVector negKill = st[BB].kill;
        newOut &= negKill.flip();
        newOut |= st[BB].gen;
        /*errs() << "Transfer function for available expr is executing!\n";*/ // Used for logging/debugging

        if (newOut != st[BB].out) {
          st[BB].out = newOut;
          changed = true;
        }
      }
    }

    for (BasicBlock* BB : order) {
      outs() << "BB: ";
      BB->printAsOperand(outs(), false);
      outs() << "\n";
      printBitSet(outs(), "gen", st[BB].gen, universe);
      printBitSet(outs(), "kill", st[BB].kill, universe);
      printBitSet(outs(), "IN", st[BB].in, universe);
      printBitSet(outs(), "OUT", st[BB].out, universe);
    }
    return PreservedAnalyses::all();
  }
};

// -------------------- Liveness/Reaching (stubs) --------------------

struct Var { // Repurposed the struct 'Expr' from AvailablePass
  Value* var;
  auto operator<=>(const Var&) const = default;

  //changed to track function args too
  Var(Value* V) : var(V) {}       
  Var(Instruction& I) : var(&I) {} 
};

raw_ostream& operator<<(raw_ostream& OS, const Var& V) { // This lets us print variables in console
  OS << getShortValueName(V.var);
  return OS;
}

struct LivenessPass : PassInfoMixin<LivenessPass> {

    struct BlockState {
        BitVector in;
        BitVector out;
        BitVector use;
        BitVector def;
    };

    static BitVector meetUnion(const std::vector<BitVector>& ins) {
        if (ins.empty()) return {};
        BitVector out = ins[0];
        for (size_t i = 1; i < ins.size(); ++i) out |= ins[i];
        return out;
    }

    // Returns true if this LLVM Value should be treated like a variable
    //nullptr, const, lables not tracked only instr result and functions args
    static bool isTrackableValue(Value* V) {
        if (V == nullptr)       { return false;     }
        if (isa<Constant>(V))   { return false;     }

        if (isa<BasicBlock>(V)) {  return false;      }
           

        if (isa<Instruction>(V)) { return true;        }
           

        if (isa<Argument>(V))    { return true;    }


        return false;
    }
    PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
        outs() << "=== ";
        F.printAsOperand(outs(), false);
        outs() << " ===\n";

        // Universe definition: set of defined variables in the block a.k.a. the LHS
        std::vector<Var> universe;
        for (Argument& A : F.args()) {
            universe.push_back(Var(&A));
        }
        for (auto& BB : F) {
            for (auto& I : BB) {
                if (!I.getType()->isVoidTy())
                    universe.push_back(Var(I));
            }
        }
        std::sort(universe.begin(), universe.end());
        universe.erase(std::unique(universe.begin(), universe.end()), universe.end());

        // Order definition: order of processing the basic blocks a.k.a. forward/top-down
        DenseMap<const BasicBlock*, BlockState> st;
        std::vector<BasicBlock*> order;
        order.push_back(&F.getEntryBlock());
        for (size_t i = 0; i < order.size(); ++i) {
            for (BasicBlock* succ : successors(order[i])) {
                if (std::find(order.begin(), order.end(), succ) == order.end()) order.push_back(succ);
            }
        }

        for (BasicBlock* BB : order) {
            BlockState bs;
            bs.in = BitVector(universe.size(), false);
            bs.out = BitVector(universe.size(), false);
            bs.use = BitVector(universe.size(), false);
            bs.def = BitVector(universe.size(), false);

            for (Instruction& I : *BB) {
                //make the USE
                // skip anything that isnt a real value like labels
                for (Use& U : I.operands()) {
                    Value* V = U.get();
                    if (!isTrackableValue(V))
                        continue;

                    //find var in universe
                    //if not found skip
                    Var vv(V);
                    auto it = std::lower_bound(universe.begin(), universe.end(), vv);
                    if (it == universe.end() || it->var != V)
                        continue;

                    unsigned idx = static_cast<unsigned>(it - universe.begin());

                    // Only add to USE if not already defined in this block
                    if (!bs.def.test(idx))
                        bs.use.set(idx);
                }

                //make the DEF
                //only get instr that produce a val
                if (!I.getType()->isVoidTy()) {
                    Var dv(I);
                    //find it in universe
                    auto it = std::lower_bound(universe.begin(), universe.end(), dv);
                    //if exist them mark it as defined
                    if (it != universe.end() && it->var == &I) {
                        unsigned idx = static_cast<unsigned>(it - universe.begin());
                        bs.def.set(idx);
                    }
                }
            }

            st[BB] = std::move(bs);
        }
        
        bool changed = true;
        while (changed) {
            changed = false;

            // iterate reverse
            for (auto itBB = order.rbegin(); itBB != order.rend(); ++itBB) {
                BasicBlock* BB = *itBB;

                // OUT[B] = union of IN[succ]
                std::vector<BitVector> succIns;
                for (BasicBlock* succ : successors(BB)) {
                    succIns.push_back(st[succ].in);
                }
                if (succIns.empty()) {
                    succIns.push_back(BitVector(universe.size(), false)); 
                }

                BitVector newOut = meetUnion(succIns);

                // IN[B] = USE[B] U (OUT[B] - DEF[B])
                BitVector newIn = newOut;
                BitVector negDef = st[BB].def;
                newIn &= negDef.flip();      // OUT & ~DEF
                newIn |= st[BB].use;         // union USE

                if (newOut != st[BB].out || newIn != st[BB].in) {
                    st[BB].out = std::move(newOut);
                    st[BB].in = std::move(newIn);
                    changed = true;
                }
            }
        }

        // print
        outs() << "[starter] liveness: implement backward dataflow (use/def, IN/OUT)\n";
        for (BasicBlock* BB : order) {

            outs() << "BB: ";
            BB->printAsOperand(outs(), false);
            outs() << "\n";
            printBitSet(outs(), "use", st[BB].use, universe);
            printBitSet(outs(), "def", st[BB].def, universe);
            printBitSet(outs(), "IN", st[BB].in, universe);
            printBitSet(outs(), "OUT", st[BB].out, universe);
        }

        return PreservedAnalyses::all();
    }
    /*
    
    return PreservedAnalyses::all();
  }
  */
};

struct ReachingPass : PassInfoMixin<ReachingPass> {
  struct BlockState {
    BitVector in;
    BitVector out;
    BitVector gen;
    BitVector kill;
  };

  static BitVector meetUnion(const std::vector<BitVector>& outs) { // Repurposed the function 'meetIntersect' from AvailablePass
    if (outs.empty()) return {};
    BitVector in = outs[0];
    for (size_t i = 1; i < outs.size(); ++i) in |= outs[i];
    return in;
  }

  PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
    //outs() << "=== ";
    //F.printAsOperand(outs(), false);
    //outs() << " ===\n";
    //outs() << "[starter] reaching: implement forward dataflow (gen/kill, IN/OUT)\n";

    outs() << "=== ";
    F.printAsOperand(outs(), false);
    outs() << " ===\n";

    // Universe definition: set of defined variables in the block a.k.a. the LHS
    std::vector<Var> universe;
    for (auto& BB : F) for (auto& I : BB) universe.push_back(Var(I));
    std::sort(universe.begin(), universe.end());
    universe.erase(std::unique(universe.begin(), universe.end()), universe.end());

    // Order definition: order of processing the basic blocks a.k.a. forward/top-down
    DenseMap<const BasicBlock*, BlockState> st;
    std::vector<BasicBlock*> order;
    order.push_back(&F.getEntryBlock());
    for (size_t i = 0; i < order.size(); ++i) {
      for (BasicBlock* succ : successors(order[i])) {
        if (std::find(order.begin(), order.end(), succ) == order.end()) order.push_back(succ);
      }
    }

    // Blockstate definition: define and initialize the IN, OUT, GEN and KILL set at block-level
    BitVector all(universe.size(), true);
    for (BasicBlock* BB : order) {
      BlockState bs;
      bs.in = BitVector(universe.size(), false);
      bs.out = BitVector(universe.size(), false);
      bs.gen = BitVector(universe.size(), false);
      bs.kill = BitVector(universe.size(), false);

      for (Instruction& I : *BB) {
        if (!I.getType()->isVoidTy()) {
          Var V = Var(I);
          auto it = std::lower_bound(universe.begin(), universe.end(), V);
          if (it != universe.end() && *it == V) bs.gen.set(static_cast<unsigned>(it - universe.begin()));
        }
        if (!I.getType()->isVoidTy()) {
          for (size_t i = 0; i < universe.size(); ++i) {
            if (universe[i].var == &I) bs.kill.set(static_cast<unsigned>(i));
          }
        }
      }

      BitVector notGen = bs.gen;
      bs.kill &= notGen.flip();
      st[BB] = bs;
    }

    bool changed = true;
    while (changed) {
      changed = false;
      for (BasicBlock* BB : order) {
        std::vector<BitVector> predOuts;
        // Boundary Condition
        if (BB == &F.getEntryBlock()) predOuts.push_back(BitVector(universe.size(), false));
        // Initialization
        for (BasicBlock* pred : predecessors(BB)) predOuts.push_back(st[pred].out);
        if (predOuts.empty()) predOuts.push_back(BitVector(universe.size(), false));

        st[BB].in = meetUnion(predOuts);

        // Transfer function: OUT = GEN U (IN - KILL).
        BitVector newOut = st[BB].in;
        BitVector negKill = st[BB].kill;
        newOut &= negKill.flip();
        newOut |= st[BB].gen;
        /*errs() << "Transfer function for reaching defs is executing!\n";*/ // Used for logging/debugging

        if (newOut != st[BB].out) {
          st[BB].out = newOut;
          changed = true;
        }
      }
    }

    for (BasicBlock* BB : order) {
      outs() << "BB: ";
      BB->printAsOperand(outs(), false);
      outs() << "\n";
      printBitSet(outs(), "gen", st[BB].gen, universe);
      printBitSet(outs(), "kill", st[BB].kill, universe);
      printBitSet(outs(), "IN", st[BB].in, universe);
      printBitSet(outs(), "OUT", st[BB].out, universe);
    }
    return PreservedAnalyses::all();
  }
};

// -------------------- Constant Propagation 3-point (starter) --------------------

struct ConstantPropPass : PassInfoMixin<ConstantPropPass> {
  enum class Kind { Top, Const, Bottom };  // Top=unknown, Bottom=NAC

  struct LVal {
    Kind kind = Kind::Top;
    int64_t c = 0;
    static LVal top() { return {Kind::Top, 0}; }
    static LVal constant(int64_t v) { return {Kind::Const, v}; }
    static LVal bottom() { return {Kind::Bottom, 0}; }
    bool operator==(const LVal& o) const { return kind == o.kind && c == o.c; }
    bool operator!=(const LVal& o) const { return !(*this == o); }
  };

  using CPState = DenseMap<const Value*, LVal>;
  struct BlockState {
    CPState in;
    CPState out;
  };
  
  // Enumerated MEET operator encapsulated below
  static LVal meetVal(LVal a, LVal b) {
    if (a.kind == Kind::Top) return b;
    if (b.kind == Kind::Top) return a;
    if (a.kind == Kind::Bottom || b.kind == Kind::Bottom) return LVal::bottom();
    return (a.c == b.c) ? a : LVal::bottom();
  }
  
  /*// Enumerated TRANSFER operator encapsulated below - INCOMPLETE and UNUSED
  static LVal transferVal(LVal a, LVal b) {
    if (a.kind == Kind::Bottom || b.kind == Kind::Bottom) return LVal::bottom();
    if (a.kind == Kind::Top || b.kind == Kind::Top) return LVal::top();
    return LVal::constant(a.c);
  }*/

  // Confirms if the value is CONST or UNDEF
  static LVal evalValue(const Value* V, const CPState& st) {
    if (const auto* CI = dyn_cast<ConstantInt>(V)) return LVal::constant(CI->getSExtValue());
    auto it = st.find(V);
    if (it == st.end()) return LVal::top();
    return it->second;
  }
  // Definitions of variables by binary operators with both constant operands are detected
  static LVal evalBinary(const BinaryOperator& BO, const CPState& st) {
    LVal l = evalValue(BO.getOperand(0), st);
    LVal r = evalValue(BO.getOperand(1), st);
    if (l.kind == Kind::Bottom || r.kind == Kind::Bottom) return LVal::bottom(); // Added for cases with either being Bottom or NAC
    else if (l.kind != Kind::Const || r.kind != Kind::Const) return LVal::top();

    // Starter example: only Add is implemented.
    if (BO.getOpcode() == Instruction::Add) return LVal::constant(l.c + r.c);
    // TODO(student): extend to Sub/Mul/Div and policy for unsupported ops.
    else if (BO.getOpcode() == Instruction::Sub) return LVal::constant(l.c - r.c);
    else if (BO.getOpcode() == Instruction::Mul) return LVal::constant(l.c * r.c);
    else if (BO.getOpcode() == Instruction::UDiv) return LVal::constant(l.c / r.c);
    else if (BO.getOpcode() == Instruction::SDiv) return LVal::constant(l.c / r.c);
    else return LVal::bottom();
  }
  
  // Definitions of variables by binary operators with both constant operands are detected
  static LVal evalICMP(const ICmpInst& II, const CPState& st) {
    LVal l = evalValue(II.getOperand(0), st);
    LVal r = evalValue(II.getOperand(1), st);
    if (l.kind == Kind::Bottom || r.kind == Kind::Bottom) return LVal::bottom(); // Added for cases with either being Bottom or NAC
    else if (l.kind != Kind::Const || r.kind != Kind::Const) return LVal::top();

    // Only other case is if both are constants
    else return LVal::constant((int)(l.c == r.c));
  }
  
  // The below function logs the properties (predecessor block, value) of a PHINode instance
  static void inspectPHINode(const PHINode *phi) {
    unsigned numIncoming = phi->getNumIncomingValues();
    for (unsigned i = 0; i < numIncoming; ++i) {
        Value *val = phi->getIncomingValue(i);
        BasicBlock *block = phi->getIncomingBlock(i);

        errs() << "Incoming from block: " << block->getName().str()
                  << " with value: ";
        if (val->hasName())
            errs() << val->getName().str();
        else
            errs() << "(unnamed)";
        errs() << "\n";
    }
  }
  
  // Conflicting definitions arising from different paths are evaluated for constantness
  static LVal evalPhi(const PHINode& Phi, const DenseMap<const BasicBlock*, BlockState>& states) {
    //(void)Phi;
    //(void)states;
    // TODO(student): merge incoming values from predecessor OUT states.
    /*inspectPHINode(&Phi);*/ // Used for logging/debugging
    Value* val1 = Phi.getIncomingValue(0);
    Value* val2 = Phi.getIncomingValue(1);
    //BasicBlock* block1 = Phi.getIncomingBlock(0);
    //BasicBlock* block2 = Phi.getIncomingBlock(1);
    //BlockState st1 = states.lookup(block1);
    //BlockState st2 = states.lookup(block2);
    BlockState st = states.lookup(Phi.getParent());
    LVal a = evalValue(val1, st.in);
    LVal b = evalValue(val2, st.in);
    return meetVal(a, b);
    // Incorrect attempt below (Ignore)
    /*int i = 0;
    LVal calcIn = LVal::top();
    unsigned numIncoming = Phi.getNumIncomingValues();
    do{
      Value* val = Phi.getIncomingValue(i);
      const BasicBlock* block = Phi.getIncomingBlock(i);
      BlockState bs = states[block];
      calcIn = meetVal(calcIn, bs.out.lookup(val));
      auto it = states.find(block);
      if (it != states.end()) {
          const BlockState& state = it->second;
          calcIn = meetVal(calcIn, state.out.lookup(val));
      }
      BlockState bs = states.lookup(block);
      calcIn = meetVal(calcIn, bs.in.lookup(val));
      i++;
    }while(i < numIncoming);
    return calcIn;*/
    //return LVal::top();
  }

  // Checks if the instruction in a block is an assignment and applies transfer function on IN, else no change.
  static CPState transferBlock(BasicBlock& BB, const CPState& in,
                               const DenseMap<const BasicBlock*, BlockState>& states) {
    CPState out = in;
    for (Instruction& I : BB) {
      if (I.getType()->isVoidTy()) continue;

      if (auto* P = dyn_cast<PHINode>(&I)) {
        out[&I] = evalPhi(*P, states);
      } else if (auto* BO = dyn_cast<BinaryOperator>(&I)) {
        out[&I] = evalBinary(*BO, out);
      } else {
        // TODO(student): handle icmp/select/loads/stores etc.
        if (auto *icmp = llvm::dyn_cast<llvm::ICmpInst>(&I)) {
            out[&I] = evalICMP(*icmp, out);
        } else if (auto *store = dyn_cast<llvm::StoreInst>(&I)) {
            out[&I] = evalValue(store->getValueOperand(), out);
        }
        else out[&I] = LVal::top();
      }
    }
    return out;
  }

  static bool sameState(const CPState& a, const CPState& b, const std::vector<const Value*>& domain) {
    for (const Value* V : domain) {
      LVal av = a.lookup(V);
      LVal bv = b.lookup(V);
      if (av != bv) return false;
    }
    return true;
  }

  static void printState(raw_ostream& OS, StringRef label, const CPState& st,
                         const std::vector<const Value*>& domain, bool showTop = false) {
    OS << "  " << label << ": { ";
    bool first = true;
    for (const Value* V : domain) {
      LVal v = st.lookup(V);
      if (!showTop && v.kind == Kind::Top) continue;
      if (!first) OS << "; ";
      first = false;
      V->printAsOperand(OS, false);
      if (v.kind == Kind::Const) OS << " = " << v.c;
      else if (v.kind == Kind::Bottom) OS << " = NAC";
      else OS << " = TOP";
    }
    OS << " }\n";
  }

  PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
    outs() << "=== ";
    F.printAsOperand(outs(), false);
    outs() << " ===\n";

    // Domain creation
    std::vector<const Value*> domain;
    for (auto& BB : F) {
      for (auto& I : BB) {
        if (!I.getType()->isVoidTy()) domain.push_back(&I);
      }
    }

    // Direction of analysis - Forward
    std::vector<BasicBlock*> order;
    order.push_back(&F.getEntryBlock());
    for (size_t i = 0; i < order.size(); ++i) {
      for (BasicBlock* succ : successors(order[i])) {
        if (std::find(order.begin(), order.end(), succ) == order.end()) order.push_back(succ);
      }
    }

    // Initialize all blocks' INs and OUTs for domain as UNDEF or T 
    DenseMap<const BasicBlock*, BlockState> st;
    for (BasicBlock* BB : order) {
      BlockState bs;
      for (const Value* V : domain) {
        bs.in[V] = LVal::top();
        bs.out[V] = LVal::top();
      }
      st[BB] = std::move(bs);
    }

    // Analysis pass starts here
    bool changed = true;
    while (changed) {
      changed = false;
      for (BasicBlock* BB : order) {
        // Initialize a new blockstate called New IN
        CPState newIn;
        for (const Value* V : domain) newIn[V] = LVal::top(); // REDUNDANT

        // If the block has predecessors, calculate New IN as MEET of a PRED OUTs
        bool hasPred = false;
        for (BasicBlock* pred : predecessors(BB)) {
          hasPred = true;
          for (const Value* V : domain) newIn[V] = meetVal(newIn.lookup(V), st[pred].out.lookup(V));
        }
        // Else the block has no predecessors, initialize New IN for the domain as UNDEF or T
        if (!hasPred) {
          for (const Value* V : domain) newIn[V] = LVal::top();
        }

        // Save the New IN as block's IN. Then apply transfer function to calculate block's OUT and save it too.
        st[BB].in = newIn;
        CPState newOut = transferBlock(*BB, newIn, st);
        if (!sameState(st[BB].out, newOut, domain)) {
          st[BB].out = std::move(newOut);
          changed = true;
        }
        /*for (BasicBlock* BB : order) {
          outs() << "BB: ";
          BB->printAsOperand(outs(), false);
          outs() << "\n";
          printState(outs(), "IN", st[BB].in, domain);
          printState(outs(), "OUT", st[BB].out, domain);
        }*/
      }
    }

    for (BasicBlock* BB : order) {
      outs() << "BB: ";
      BB->printAsOperand(outs(), false);
      outs() << "\n";
      printState(outs(), "IN", st[BB].in, domain);
      printState(outs(), "OUT", st[BB].out, domain);
    }

    return PreservedAnalyses::all();
  }
};

struct DominatorsPass : PassInfoMixin<DominatorsPass> {

    PreservedAnalyses run(Function& F, FunctionAnalysisManager& FAM) {
        auto& LI = FAM.getResult<LoopAnalysis>(F);

        outs() << "=== Dominators for ";
        F.printAsOperand(outs(), false);
        outs() << " ===\n";

        std::vector<BasicBlock*> blocks;
        for (auto& BB : F) blocks.push_back(&BB);
        int N = blocks.size();

        DenseMap<BasicBlock*, int> blockIdx;
        for (int i = 0; i < N; i++) {
            blockIdx[blocks[i]] = i;
        }

        DenseMap<BasicBlock*, BitVector> dom;
        for (int i = 0; i < N; i++) {
            dom[blocks[i]] = BitVector(N, true);
        }

        BasicBlock* entry = &F.getEntryBlock();
        dom[entry] = BitVector(N, false);
        dom[entry].set(blockIdx[entry]);

        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i < N; i++) {
                BasicBlock* B = blocks[i];
                if (B == entry) continue;

                BitVector newDom(N, true);
                bool hasPred = false;

                for (BasicBlock* P : predecessors(B)) {
                    if (!hasPred) {
                        newDom = dom[P];
                        hasPred = true;
                    }
                    else {
                        newDom &= dom[P];
                    }
                }

                if (!hasPred) {
                    newDom = BitVector(N, false);
                }

                newDom.set(i);

                if (newDom != dom[B]) {
                    dom[B] = newDom;
                    changed = true;
                }
            }
        }

        auto printBB = [&](BasicBlock* BB) {
            if (!BB) {
                outs() << "(none)";
            }
            else if (BB->hasName()) {
                outs() << BB->getName();
            }
            else {
                BB->printAsOperand(outs(), false);
            }
            };

        auto getIDom = [&](BasicBlock* B) -> BasicBlock* {
            if (B == entry) return nullptr;

            int bIdx = blockIdx[B];
            BasicBlock* idom = nullptr;

            for (int j = 0; j < N; j++) {
                if (j == bIdx) continue;
                if (!dom[B].test(j)) continue;

                bool isImmediate = true;
                for (int k = 0; k < N; k++) {
                    if (k == j || k == bIdx) continue;
                    if (dom[B].test(k) && dom[blocks[k]].test(j)) {
                        isImmediate = false;
                        break;
                    }
                }

                if (isImmediate) {
                    idom = blocks[j];
                    break;
                }
            }

            return idom;
            };

        for (Loop* L : LI) {
            SmallVector<BasicBlock*, 8> toPrint;

            for (BasicBlock* B : L->blocks()) {
                toPrint.push_back(B);
            }

            SmallVector<BasicBlock*, 8> exitBlocks;
            L->getExitBlocks(exitBlocks);
            for (BasicBlock* EB : exitBlocks) {
                if (std::find(toPrint.begin(), toPrint.end(), EB) == toPrint.end()) {
                    toPrint.push_back(EB);
                }
            }

            for (BasicBlock* B : toPrint) {
                if (B == entry) continue;

                BasicBlock* idom = getIDom(B);
                printBB(B);
                outs() << " is dominated by ";
                printBB(idom);
                outs() << "\n";
            }
        }

        return PreservedAnalyses::all();
    }
};


struct DeadCodeEliminationPass : PassInfoMixin<DeadCodeEliminationPass> {
    static bool isLive(Instruction* I) {
        return I->isTerminator() ||
            isa<DbgInfoIntrinsic>(I) ||
            isa<LandingPadInst>(I) ||
            I->mayHaveSideEffects();
    }

    PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
        errs() << "RUNNING DCE ON " << F.getName() << "\n";

        bool changedAny = false;
        bool changed = true;

        while (changed) {
            changed = false;

            DenseSet<Instruction*> live;
            SmallVector<Instruction*, 64> worklist;

            for (BasicBlock& BB : F) {
                for (Instruction& I : BB) {
                    if (isLive(&I)) {
                        live.insert(&I);
                        worklist.push_back(&I);
                    }
                }
            }

            while (!worklist.empty()) {
                Instruction* I = worklist.pop_back_val();

                for (Use& U : I->operands()) {
                    if (Instruction* OpI = dyn_cast<Instruction>(U.get())) {
                        if (!live.count(OpI)) {
                            live.insert(OpI);
                            worklist.push_back(OpI);
                        }
                    }
                }
            }

            SmallVector<Instruction*, 32> toRemove;
            for (BasicBlock& BB : F) {
                for (Instruction& I : BB) {
                    if (!live.count(&I) && I.use_empty()) {
                        toRemove.push_back(&I);
                    }
                }
            }

            for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
                Instruction* I = *it;
                errs() << "Removing Instruction: ";
                I->print(errs());
                errs() << "\n";
                I->eraseFromParent();
                changed = true;
                changedAny = true;
            }
        }

        return changedAny ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

struct LoopInvariantCodeMotionPass : PassInfoMixin<LoopInvariantCodeMotionPass> {
    struct DomInfo {
        std::vector<BasicBlock*> blocks;
        DenseMap<BasicBlock*, int> blockIdx;
        DenseMap<BasicBlock*, BitVector> dom;
    };

    static DomInfo computeDominators(Function& F) {
        DomInfo DI;
        for (auto& BB : F) {
            DI.blockIdx[&BB] = static_cast<int>(DI.blocks.size());
            DI.blocks.push_back(&BB);
        }
        int N = static_cast<int>(DI.blocks.size());
        BasicBlock* entry = &F.getEntryBlock();
        for (BasicBlock* BB : DI.blocks)
            DI.dom[BB] = BitVector(N, true);
        DI.dom[entry] = BitVector(N, false);
        DI.dom[entry].set(DI.blockIdx[entry]);

        bool changed = true;
        while (changed) {
            changed = false;
            for (BasicBlock* B : DI.blocks) {
                if (B == entry) continue;
                BitVector newDom(N, true);
                bool hasPred = false;
                for (BasicBlock* P : predecessors(B)) {
                    if (!hasPred) { newDom = DI.dom[P]; hasPred = true; }
                    else newDom &= DI.dom[P];
                }
                if (!hasPred) newDom = BitVector(N, false);
                newDom.set(DI.blockIdx[B]);
                if (newDom != DI.dom[B]) { DI.dom[B] = newDom; changed = true; }
            }
        }
        return DI;
    }

    static bool blockDominates(BasicBlock* A, BasicBlock* B, const DomInfo& DI) {
        auto itA = DI.blockIdx.find(A);
        auto itB = DI.blockIdx.find(B);
        if (itA == DI.blockIdx.end() || itB == DI.blockIdx.end()) return false;
        return DI.dom.lookup(B).test(itA->second);
    }

    static bool isHoistableInstruction(Instruction* I) {
        return !isa<PHINode>(I) &&
            !isa<LandingPadInst>(I) &&
            !I->isTerminator() &&
            !I->mayReadFromMemory() &&
            isSafeToSpeculativelyExecute(I);
    }

    static bool operandIsLoopInvariant(Value* V, Loop* L,
        const SmallPtrSetImpl<Instruction*>& invariantSet) {
        if (isa<Constant>(V) || isa<Argument>(V)) return true;
        if (auto* I = dyn_cast<Instruction>(V))
            return !L->contains(I) || invariantSet.contains(I);
        return true;
    }

    static bool processLoop(Loop* L, Function& F) {
        bool changedAny = false;

        // Process inner loops first (bottom-up)
        for (Loop* SubL : *L)
            changedAny |= processLoop(SubL, F);

        BasicBlock* preheader = L->getLoopPreheader();
        if (!preheader) {
            errs() << "  Skipping loop (no preheader)\n";
            return changedAny;
        }

        DomInfo DI = computeDominators(F);

        // Find all loop-invariant instructions to fixpoint
        SmallPtrSet<Instruction*, 32> invariantSet;
        bool changed = true;
        while (changed) {
            changed = false;
            for (BasicBlock* BB : L->blocks()) {
                for (Instruction& I : *BB) {
                    if (invariantSet.contains(&I)) continue;
                    if (!isHoistableInstruction(&I)) continue;
                    bool allInvariant = true;
                    for (Value* Op : I.operands()) {
                        if (!operandIsLoopInvariant(Op, L, invariantSet)) {
                            allInvariant = false;
                            break;
                        }
                    }
                    if (allInvariant) { invariantSet.insert(&I); changed = true; }
                }
            }
        }

        // Collect invariants in loop order to preserve dependencies
        SmallVector<Instruction*, 32> toHoist;
        for (BasicBlock* BB : L->blocks()) {
            for (Instruction& I : *BB) {
                if (!invariantSet.contains(&I)) continue;
                toHoist.push_back(&I);
            }
        }

        // Hoist all invariants to preheader (conservative approach)
        for (Instruction* I : toHoist) {
            errs() << "Hoisting to preheader: ";
            I->print(errs());
            errs() << "\n";
            I->moveBefore(preheader->getTerminator()->getIterator());
            changedAny = true;
        }

        return changedAny;
    }

    PreservedAnalyses run(Function& F, FunctionAnalysisManager& FAM) {
        errs() << "RUNNING LICM ON " << F.getName() << "\n";
        auto& LI = FAM.getResult<LoopAnalysis>(F);
        bool changed = false;
        for (Loop* L : LI)
            changed |= processLoop(L, F);
        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

}  // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "UnifiedPass", "v0.3-starter", [](PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager& FPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  if (Name == "available") {
                    FPM.addPass(AvailablePass());
                    return true;
                  }
                  if (Name == "liveness") {
                    FPM.addPass(LivenessPass());
                    return true;
                  }
                  if (Name == "reaching") {
                    FPM.addPass(ReachingPass());
                    return true;
                  }
                  if (Name == "doms") {
                      FPM.addPass(DominatorsPass());
                      return true;
                  }
                  if (Name == "dead-code-elimination") {
                      FPM.addPass(DeadCodeEliminationPass());
                      return true;
                  }
                  if (Name == "loop-invariant-code-motion") {
                      FPM.addPass(LoopInvariantCodeMotionPass());
                      return true;
                  }
                  if (Name == "constantprop") {
                    FPM.addPass(ConstantPropPass());
                    return true;
                  }
                  return false;
                });
          }};
}
