//===------------------------------------------------------------*- C++ -*-===//
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/SCF.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/IR/Function.h"
#include "mlir/IR/Module.h"
#include "mlir/IR/StandardTypes.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Translation.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/raw_ostream.h"

#include "EmitHLSCpp.h"

using namespace std;
using namespace mlir;

//===----------------------------------------------------------------------===//
// Some Base Classes
//
// These classes should be factored out, and can be inherited by emitters
// targeting various backends (e.g., Xilinx Vivado HLS, Intel FPGAs, etc.).
//===----------------------------------------------------------------------===//

namespace {
/// This class maintains the mutable state that cross-cuts and is shared by the
/// various emitters.
class HLSCppEmitterState {
public:
  explicit HLSCppEmitterState(raw_ostream &os) : os(os) {}

  /// The stream to emit to.
  raw_ostream &os;

  bool encounteredError = false;
  unsigned currentIndent = 0;

private:
  HLSCppEmitterState(const HLSCppEmitterState &) = delete;
  void operator=(const HLSCppEmitterState &) = delete;
};
} // namespace

namespace {
/// This is the base class for all of the HLSCpp Emitter components.
class HLSCppEmitterBase {
public:
  explicit HLSCppEmitterBase(HLSCppEmitterState &state)
      : state(state), os(state.os) {}

  InFlightDiagnostic emitError(Operation *op, const Twine &message) {
    state.encounteredError = true;
    return op->emitError(message);
  }

  raw_ostream &indent() { return os.indent(state.currentIndent); }

  void addIndent() { state.currentIndent += 2; }
  void reduceIndent() { state.currentIndent -= 2; }

  // All of the mutable state we are maintaining.
  HLSCppEmitterState &state;

  /// The stream to emit to.
  raw_ostream &os;

private:
  HLSCppEmitterBase(const HLSCppEmitterBase &) = delete;
  void operator=(const HLSCppEmitterBase &) = delete;
};
} // namespace

namespace {
/// This class is a visitor for SSACFG operation nodes.
template <typename ConcreteType, typename ResultType, typename... ExtraArgs>
class HLSCppVisitorBase {
public:
  ResultType dispatchVisitor(Operation *op, ExtraArgs... args) {
    auto *thisCast = static_cast<ConcreteType *>(this);
    return TypeSwitch<Operation *, ResultType>(op)
        .template Case<
            // Memref-related statements.
            AllocOp, LoadOp, StoreOp,
            // Affine statements (with region).
            AffineForOp, AffineIfOp, AffineParallelOp,
            // Affine statements (without region).
            AffineApplyOp, AffineMaxOp, AffineMinOp, AffineLoadOp,
            AffineStoreOp,
            // Unary expressions.
            AbsFOp, CeilFOp, NegFOp, CosOp, SinOp, TanhOp, SqrtOp, RsqrtOp,
            ExpOp, Exp2Op, LogOp, Log2Op, Log10Op,
            // Float binary expressions.
            CmpFOp, AddFOp, SubFOp, MulFOp, DivFOp, RemFOp,
            // Integer binary expressions.
            CmpIOp, AddIOp, SubIOp, MulIOp, SignedDivIOp, SignedRemIOp,
            UnsignedDivIOp, UnsignedRemIOp, XOrOp, AndOp, OrOp, ShiftLeftOp,
            SignedShiftRightOp, UnsignedShiftRightOp,
            // Special operations.
            AffineYieldOp, ConstantOp, ReturnOp>(
            [&](auto opNode) -> ResultType {
              return thisCast->visitOp(opNode, args...);
            })
        .Default([&](auto opNode) -> ResultType {
          return thisCast->visitInvalidOp(op, args...);
        });
  }

  /// This callback is invoked on any invalid operations.
  ResultType visitInvalidOp(Operation *op, ExtraArgs... args) {
    op->emitOpError("is unsupported operation.");
    abort();
  }

  /// This callback is invoked on any operations that are not handled by the
  /// concrete visitor.
  ResultType visitUnhandledOp(Operation *op, ExtraArgs... args) {
    return ResultType();
  }

#define HANDLE(OPTYPE)                                                         \
  ResultType visitOp(OPTYPE op, ExtraArgs... args) {                           \
    return static_cast<ConcreteType *>(this)->visitUnhandledOp(op, args...);   \
  }

  // Memref related statements.
  HANDLE(AllocOp);
  HANDLE(LoadOp);
  HANDLE(StoreOp);

  // Affine statements (with region).
  HANDLE(AffineForOp);
  HANDLE(AffineIfOp);
  HANDLE(AffineParallelOp);

  // Affine statements (without region).
  HANDLE(AffineApplyOp);
  HANDLE(AffineMaxOp);
  HANDLE(AffineMinOp);
  HANDLE(AffineLoadOp);
  HANDLE(AffineStoreOp);

  // Unary expressions.
  HANDLE(AbsFOp);
  HANDLE(CeilFOp);
  HANDLE(NegFOp);
  HANDLE(CosOp);
  HANDLE(SinOp);
  HANDLE(TanhOp);
  HANDLE(SqrtOp);
  HANDLE(RsqrtOp);
  HANDLE(ExpOp);
  HANDLE(Exp2Op);
  HANDLE(LogOp);
  HANDLE(Log2Op);
  HANDLE(Log10Op);

  // Float binary expressions.
  HANDLE(CmpFOp);
  HANDLE(AddFOp);
  HANDLE(SubFOp);
  HANDLE(MulFOp);
  HANDLE(DivFOp);
  HANDLE(RemFOp);

  // Integer binary expressions.
  HANDLE(CmpIOp);
  HANDLE(AddIOp);
  HANDLE(SubIOp);
  HANDLE(MulIOp);
  HANDLE(SignedDivIOp);
  HANDLE(SignedRemIOp);
  HANDLE(UnsignedDivIOp);
  HANDLE(UnsignedRemIOp);
  HANDLE(XOrOp);
  HANDLE(AndOp);
  HANDLE(OrOp);
  HANDLE(ShiftLeftOp);
  HANDLE(SignedShiftRightOp);
  HANDLE(UnsignedShiftRightOp);

  // Special operations.
  HANDLE(AffineYieldOp);
  HANDLE(ConstantOp);
  HANDLE(ReturnOp);
#undef HANDLE
};
} // namespace

//===----------------------------------------------------------------------===//
// Utils
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// ModuleEmitter Class Definition
//===----------------------------------------------------------------------===//

namespace {
class ModuleEmitter : public HLSCppEmitterBase {
public:
  explicit ModuleEmitter(HLSCppEmitterState &state)
      : HLSCppEmitterBase(state) {}

  // Memref-related statement emitters.
  void emitAlloc(AllocOp *op);
  void emitLoad(LoadOp *op);
  void emitStore(StoreOp *op);

  // Affine statement emitters.
  void emitAffineFor(AffineForOp *op);
  void emitAffineIf(AffineIfOp *op) {}
  void emitAffineParallel(AffineParallelOp *op) {}

  // Standard expression emitters.
  void emitBinary(Operation *op, const char *syntax);
  void emitUnary(Operation *op, const char *syntax);

  // MLIR module emitter.
  void emitModule(ModuleOp module);

private:
  DenseMap<Value, SmallString<8>> nameTable;
  SmallString<8> getName(Value val);
  SmallString<8> addName(Value val, bool isPtr);
  void emitValue(Value val, bool isPtr);

  void emitRegion(Region &region);

  void emitOperation(Operation *op);
  void emitFunction(FuncOp func);
};
} // namespace

//===----------------------------------------------------------------------===//
// ExprVisitor Class
//===----------------------------------------------------------------------===//

namespace {
class ExprVisitor : public HLSCppVisitorBase<ExprVisitor, bool> {
public:
  ExprVisitor(ModuleEmitter &emitter) : emitter(emitter) {}

  using HLSCppVisitorBase::visitOp;
  // Float binary expressions.
  bool visitOp(CmpFOp op);
  bool visitOp(AddFOp op) { return emitter.emitBinary(op, "+"), true; }
  bool visitOp(SubFOp op) { return emitter.emitBinary(op, "-"), true; }
  bool visitOp(MulFOp op) { return emitter.emitBinary(op, "*"), true; }
  bool visitOp(DivFOp op) { return emitter.emitBinary(op, "/"), true; }
  bool visitOp(RemFOp op) { return emitter.emitBinary(op, "%"), true; }

  // Integer binary expressions.
  bool visitOp(CmpIOp op);
  bool visitOp(AddIOp op) { return emitter.emitBinary(op, "+"), true; }
  bool visitOp(SubIOp op) { return emitter.emitBinary(op, "-"), true; }
  bool visitOp(MulIOp op) { return emitter.emitBinary(op, "*"), true; }
  bool visitOp(SignedDivIOp op) { return emitter.emitBinary(op, "/"), true; }
  bool visitOp(SignedRemIOp op) { return emitter.emitBinary(op, "/"), true; }
  bool visitOp(UnsignedDivIOp op) { return emitter.emitBinary(op, "%"), true; }
  bool visitOp(UnsignedRemIOp op) { return emitter.emitBinary(op, "%"), true; }
  bool visitOp(XOrOp op) { return emitter.emitBinary(op, "^"), true; }
  bool visitOp(AndOp op) { return emitter.emitBinary(op, "&"), true; }
  bool visitOp(OrOp op) { return emitter.emitBinary(op, "|"), true; }
  bool visitOp(ShiftLeftOp op) { return emitter.emitBinary(op, "<<"), true; }
  bool visitOp(SignedShiftRightOp op) {
    return emitter.emitBinary(op, ">>"), true;
  }
  bool visitOp(UnsignedShiftRightOp op) {
    return emitter.emitBinary(op, ">>"), true;
  }

  // Unary expressions.
  bool visitOp(AbsFOp op) { return emitter.emitUnary(op, "abs"), true; }
  bool visitOp(CeilFOp op) { return emitter.emitUnary(op, "ceil"), true; }
  bool visitOp(NegFOp op) { return emitter.emitUnary(op, "-"), true; }
  bool visitOp(CosOp op) { return emitter.emitUnary(op, "cos"), true; }
  bool visitOp(SinOp op) { return emitter.emitUnary(op, "sin"), true; }
  bool visitOp(TanhOp op) { return emitter.emitUnary(op, "tanh"), true; }
  bool visitOp(SqrtOp op) { return emitter.emitUnary(op, "sqrt"), true; }
  bool visitOp(RsqrtOp op) { return emitter.emitUnary(op, "1.0 / sqrt"), true; }
  bool visitOp(ExpOp op) { return emitter.emitUnary(op, "exp"), true; }
  bool visitOp(Exp2Op op) { return emitter.emitUnary(op, "exp2"), true; }
  bool visitOp(LogOp op) { return emitter.emitUnary(op, "log"), true; }
  bool visitOp(Log2Op op) { return emitter.emitUnary(op, "log2"), true; }
  bool visitOp(Log10Op op) { return emitter.emitUnary(op, "log10"), true; }

  // Special operations.
  bool visitOp(AffineYieldOp op) { return true; }
  bool visitOp(ConstantOp op) { return true; }
  bool visitOp(ReturnOp op) { return true; }

private:
  ModuleEmitter &emitter;
};
} // namespace

bool ExprVisitor::visitOp(CmpFOp op) {
  switch (op.getPredicate()) {
  case CmpFPredicate::OEQ:
  case CmpFPredicate::UEQ:
    return emitter.emitBinary(op, "=="), true;
  case CmpFPredicate::ONE:
  case CmpFPredicate::UNE:
    return emitter.emitBinary(op, "!="), true;
  case CmpFPredicate::OLT:
  case CmpFPredicate::ULT:
    return emitter.emitBinary(op, "<"), true;
  case CmpFPredicate::OLE:
  case CmpFPredicate::ULE:
    return emitter.emitBinary(op, "<="), true;
  case CmpFPredicate::OGT:
  case CmpFPredicate::UGT:
    return emitter.emitBinary(op, ">"), true;
  case CmpFPredicate::OGE:
  case CmpFPredicate::UGE:
    return emitter.emitBinary(op, ">="), true;
  default:
    return true;
  }
}

bool ExprVisitor::visitOp(CmpIOp op) {
  switch (op.getPredicate()) {
  case CmpIPredicate::eq:
    return emitter.emitBinary(op, "=="), true;
  case CmpIPredicate::ne:
    return emitter.emitBinary(op, "!="), true;
  case CmpIPredicate::slt:
  case CmpIPredicate::ult:
    return emitter.emitBinary(op, "<"), true;
  case CmpIPredicate::sle:
  case CmpIPredicate::ule:
    return emitter.emitBinary(op, "<="), true;
  case CmpIPredicate::sgt:
  case CmpIPredicate::ugt:
    return emitter.emitBinary(op, ">"), true;
  case CmpIPredicate::sge:
  case CmpIPredicate::uge:
    return emitter.emitBinary(op, ">="), true;
  }
}

//===----------------------------------------------------------------------===//
// StmtVisitor Class
//===----------------------------------------------------------------------===//

namespace {
class StmtVisitor : public HLSCppVisitorBase<StmtVisitor, bool> {
public:
  StmtVisitor(ModuleEmitter &emitter) : emitter(emitter) {}

  using HLSCppVisitorBase::visitOp;
  // Memref related statements.
  bool visitOp(AllocOp op) { return emitter.emitAlloc(&op), true; }
  bool visitOp(LoadOp op) { return emitter.emitLoad(&op), true; }
  bool visitOp(StoreOp op) { return emitter.emitStore(&op), true; }

  // Affine statements (with region).
  bool visitOp(AffineForOp op) { return emitter.emitAffineFor(&op), true; }
  bool visitOp(AffineIfOp op) { return emitter.emitAffineIf(&op), true; }
  bool visitOp(AffineParallelOp op) {
    return emitter.emitAffineParallel(&op), true;
  }

  // Affine statements (without region).
  bool visitOp(AffineApplyOp op) { return true; }
  bool visitOp(AffineMaxOp op) { return true; }
  bool visitOp(AffineMinOp op) { return true; }
  bool visitOp(AffineLoadOp op) { return true; }
  bool visitOp(AffineStoreOp op) { return true; }

private:
  ModuleEmitter &emitter;
};
} // namespace

//===----------------------------------------------------------------------===//
// ModuleEmitter Class Implementation
//===----------------------------------------------------------------------===//

SmallString<8> ModuleEmitter::getName(Value val) {
  if (val.getKind() == Value::Kind::BlockArgument)
    return nameTable[val];

  // If value is the result of a constant operation, the constant number will be
  // returned rather than the value name.
  else if (auto constOp = dyn_cast<ConstantOp>(val.getDefiningOp())) {
    auto valAttr = constOp.getValue();
    switch (valAttr.getType().getKind()) {
    case StandardTypes::F32:
    case StandardTypes::F64:
      return StringRef(to_string(valAttr.cast<FloatAttr>().getValueAsDouble()));

    case StandardTypes::Index:
    case StandardTypes::Integer:
      return StringRef(to_string(valAttr.cast<IntegerAttr>().getInt()));

    default:
      emitError(val.getDefiningOp(), "has unsupported type.");
      break;
    }
  } else
    return nameTable[val];
}

SmallString<8> ModuleEmitter::addName(Value val, bool isPtr = false) {
  // Temporary naming rule.
  SmallString<8> newName;
  if (isPtr)
    newName += "*";
  newName += StringRef("val" + to_string(nameTable.size()));

  // It seems currently addName method is only used by emitValue method,
  // which has already checked whether the current value has be declared.
  nameTable[val] = newName;
  return newName;
}

void ModuleEmitter::emitValue(Value val, bool isPtr = false) {
  // Value has been declared before.
  auto valName = getName(val);
  if (!valName.empty()) {
    os << valName;
    return;
  }

  // Handle memref type.
  auto valType = val.getType();
  if (auto memType = valType.dyn_cast<MemRefType>())
    valType = memType.getElementType();

  // Emit value type for declaring a new value.
  switch (valType.getKind()) {
  // Handle float types.
  case StandardTypes::F32:
    os << "float ";
    break;
  case StandardTypes::F64:
    os << "double ";
    break;

  // Handle integer types.
  case StandardTypes::Index:
    os << "int ";
    break;
  case StandardTypes::Integer: {
    auto intType = valType.cast<IntegerType>();
    os << "ap_";
    if (intType.getSignedness() == IntegerType::SignednessSemantics::Unsigned)
      os << "u";
    os << "int<" << intType.getWidth() << "> ";
    break;
  }
  default:
    emitError(val.getDefiningOp(), "has unsupported type.");
    break;
  }

  // Add the new value to nameTable and emit its name.
  os << addName(val, isPtr);
  return;
}

void ModuleEmitter::emitAlloc(AllocOp *op) {
  indent();
  emitValue(op->getResult());
  for (auto &shape : op->getType().getShape())
    os << "[" << shape << "];\n";
}

void ModuleEmitter::emitLoad(LoadOp *op) {
  indent();
  emitValue(op->getResult());
  os << " = " << getName(op->getOperand(0));
  for (auto indice : llvm::drop_begin(op->getOperands(), 1))
    os << "[" << getName(indice) << "];\n";
}

void ModuleEmitter::emitStore(StoreOp *op) {
  indent();
  os << getName(op->getOperand(1));
  for (auto indice : llvm::drop_begin(op->getOperands(), 2))
    os << "[" << getName(indice) << "]";
  os << " = " << getName(op->getOperand(0)) << ";\n";
}

void ModuleEmitter::emitAffineFor(mlir::AffineForOp *op) {
  indent();
  os << "for (";
  auto iterVar = op->getInductionVar();

  // Emit lower bound.
  emitValue(iterVar);
  if (op->hasConstantLowerBound())
    os << " = " << op->getConstantLowerBound() << "; ";

  // Emit upper bound.
  if (op->hasConstantUpperBound())
    os << getName(iterVar) << " < " << op->getConstantUpperBound() << "; ";

  // Emit increase step.
  os << getName(iterVar) << " += " << op->getStep() << ") {\n";

  emitRegion(op->getRegion());
  indent();
  os << "}\n";
}

void ModuleEmitter::emitBinary(Operation *op, const char *syntax) {
  indent();
  emitValue(op->getResult(0));
  os << " = " << getName(op->getOperand(0));
  os << " " << syntax << " ";
  os << getName(op->getOperand(1)) << ";\n";
}

void ModuleEmitter::emitUnary(Operation *op, const char *syntax) {
  indent();
  emitValue(op->getResult(0));
  os << " = " << syntax << "(" << getName(op->getOperand(0)) << ");\n";
}

void ModuleEmitter::emitRegion(Region &region) {
  // This method assumes the region owned by the targeted operation only
  // contains one MLIR block.
  addIndent();
  for (auto &op : region.getBlocks().front()) {
    emitOperation(&op);
  }
  reduceIndent();
}

void ModuleEmitter::emitOperation(Operation *op) {
  if (ExprVisitor(*this).dispatchVisitor(op))
    return;

  if (StmtVisitor(*this).dispatchVisitor(op))
    return;

  emitError(op, "can't be correctly emitted.");
}

void ModuleEmitter::emitFunction(FuncOp func) {
  if (func.getBlocks().size() != 1)
    emitError(func, "has more than one basic blocks.");
  os << "void " << func.getName() << "(\n";

  // Emit function signature.
  addIndent();

  // Handle input arguments.
  unsigned argIdx = 0;
  for (auto &arg : func.getArguments()) {
    indent();
    emitValue(arg);
    if (auto memType = arg.getType().dyn_cast<MemRefType>())
      for (auto &shape : memType.getShape())
        os << "[" << shape << "]";
    if (argIdx == func.getNumArguments() - 1 && func.getNumResults() == 0)
      os << "\n";
    else
      os << ",\n";
    argIdx += 1;
  }

  // Handle results.
  if (auto funcReturn = dyn_cast<ReturnOp>(func.front().getTerminator())) {
    unsigned resultIdx = 0;
    for (auto result : funcReturn.getOperands()) {
      indent();
      emitValue(result, /*isPtr=*/true);
      if (auto memType = result.getType().dyn_cast<MemRefType>())
        for (auto &shape : memType.getShape())
          os << "[" << shape << "]";
      if (resultIdx == func.getNumResults() - 1)
        os << "\n";
      else
        os << ",\n";
      resultIdx += 1;
    }
  } else {
    emitError(func, "doesn't have return operation as terminator.");
  }

  reduceIndent();
  os << ") {\n";

  // Emit function body.
  addIndent();

  // Traverse all operations and emit them.
  for (auto &op : func.front())
    emitOperation(&op);

  reduceIndent();
  os << "}\n";
}

void ModuleEmitter::emitModule(ModuleOp module) {
  os << R"XXX(
//===------------------------------------------------------------*- C++ -*-===//
//
// Automatically generated file for High-level Synthesis (HLS).
//
//===----------------------------------------------------------------------===//

#include <ap_axi_sdata.h>
#include <ap_fixed.h>
#include <ap_int.h>
#include <hls_math.h>
#include <hls_stream.h>
#include <math.h>
#include <stdint.h>

)XXX";

  for (auto &op : *module.getBody()) {
    if (auto func = dyn_cast<FuncOp>(op))
      emitFunction(func);
    else if (!isa<ModuleTerminatorOp>(op))
      emitError(&op, "is unsupported operation.");
  }
}

//===----------------------------------------------------------------------===//
// Entry of hlsld-translate
//===----------------------------------------------------------------------===//

static LogicalResult emitHLSCpp(ModuleOp module, llvm::raw_ostream &os) {
  HLSCppEmitterState state(os);
  ModuleEmitter(state).emitModule(module);
  return failure(state.encounteredError);
}

void hlsld::registerHLSCppEmitterTranslation() {
  static TranslateFromMLIRRegistration toHLSCpp("emit-hlscpp", emitHLSCpp);
}
