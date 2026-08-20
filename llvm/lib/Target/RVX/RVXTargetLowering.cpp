#include "RVXTargetLowering.h"
#include "RVX.h"                      // RVXISD opcodes
#include "RVXMachineFunctionInfo.h"   // RVX-specific per-MF state
#include "RVXRegisterInfo.h"          // register class definitions
#include "RVXSubtarget.h"             // feature bits

#include "llvm/CodeGen/CallingConvLower.h"  // CCState, CCValAssign
#include "llvm/CodeGen/MachineFrameInfo.h"  // createFixedObject()
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h" // ELF section handling
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"

#include "RVXGenCallingConv.inc"

using namespace llvm;

#define DEBUG_TYPE "rvx-lower"

RVXTargetLowering::RVXTargetLowering(const TargetMachine &TM,
                                       const RVXSubtarget &STI)
    : TargetLowering(TM), STI(STI) {

    MVT XLenVT = STI.getXLenVT();

    addRegisterClass(XLenVT, &RVX::GRPRegsRegClass);

    if (STI.is64Bit()) 
        addRegisterClass(MVT::i32, &RVX::GRPRegsRegClass);

    if (STI.hasStdExtF())
        addRegisterClass(MVT::f32, &RVX::FPR32RegClass);

    if (STI.hasStdExtD())
        addRegisterClass(MVT::f64, &RVX::FPR64RegClass);

    computeRegisterProperties(STI.getRegisterInfo());

    setExceptionPointerRegister(RVX::X10);  // a0
    setExceptionSelectorRegister(RVX::X11); // a1
    setMinFunctionAlignment(Align(4));
    setPrefFunctionAlignment(Align(16));

    // If M is enabled: DIV/REM are Legal (RISC-V DIV, DIVU, REM, REMU).
    // If M is disabled: must expand to libcalls or a software sequence.
    ISD::NodeType DivRemOps[] = {ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM};
    for (auto Op : DivRemOps) {
        if (STI.hasStdExtM()) {
            setOperationAction(Op, XLenVT, Legal);
        } else {
        // Expand: the generic legaliser will insert a libcall (__divsi3 etc.)
        // or an inline software division sequence depending on the target opts.
        setOperationAction(Op, XLenVT, Expand);
        }
    }

    if (STI.hasStdExtM()) {
        setOperationAction(ISD::MUL, XLenVT, Legal);
        setOperationAction(ISD::MULH, XLenVT, Legal);
        setOperationAction(ISD::MULHSU, XLenVT, Legal);
        setOperationAction(ISD::MULHU, XLenVT, Legal);
    } else {
        setOperationAction(ISD::MUL, XLenVT, Expand);
        setOperationAction(ISD::MULH, XLenVT, Expand);
        setOperationAction(ISD::MULHSU, XLenVT, Expand);
        setOperationAction(ISD::MULHU, XLenVT, Expand);
    }

    setOperationAction(ISD::ROTL, XLenVT, Expand);
    setOperationAction(ISD::ROTR, XLenVT, Expand);

    setOperationAction(ISD::CTLZ, XLenVT, Expand);
    setOperationAction(ISD::CTTZ, XLenVT, Expand);
    setOperationAction(ISD::CTPOP, XLenVT, Expand);
    setOperationAction(ISD::CTLZ_ZERO_UNDEF, XLenVT, Expand);
    setOperationAction(ISD::CTTZ_ZERO_UNDEF, XLenVT, Expand);

    setOperationAction(ISD::BSWAP, XLenVT, Expand);

    setOperationAction(ISD::BITREVERSE, XLenVT, Expand);

    setOperationAction(ISD::SMUL_LOHI, XLenVT, Expand);
    setOperationAction(ISD::UMUL_LOHI, XLenVT, Expand);

    setOperationAction(ISD::SDIVREM, XLenVT, Expand);
    setOperationAction(ISD::UDIVREM, XLenVT, Expand);

    setOperationAction(ISD::BR_CC, XLenVT, Custom);
    setOperationAction(ISD::BR_JT, MVT::Other, Expand); // jump tables: expand
    
    setOperationAction(ISD::SELECT_CC, XLenVT, Custom);
    setOperationAction(ISD::SELECT, XLenVT, Custom);

    setOperationAction(ISD::SETCC, XLenVT, Legal);

    setOperationAction(ISD::GlobalAddress, XLenVT, Custom);
    setOperationAction(ISD::GlobalTLSAddress, XLenVT, Custom);
    setOperationAction(ISD::ExternalSymbol, XLenVT, Custom);
    setOperationAction(ISD::BlockAddress, XLenVT, Custom);
    setOperationAction(ISD::JumpTable, XLenVT, Custom);
    setOperationAction(ISD::ConstantPool, XLenVT, Custom);

    setOperationAction(ISD::VASTART, MVT::Other, Custom);
    setOperationAction(ISD::VAARG, MVT::Other, Expand);
    setOperationAction(ISD::VACOPY, MVT::Other, Expand);
    setOperationAction(ISD::VAEND, MVT::Other, Expand);

    setOperationAction(ISD::DYNAMIC_STACKALLOC, XLenVT, Expand);


    if (STI.is64Bit()) {
        // 32-bit ADD and SUB on RV64 → ADDW and SUBW (sign-extend result to 64 bits)
        setOperationAction(ISD::ADD, MVT::i32, Custom); // → RVXISD::ADDW
        setOperationAction(ISD::SUB, MVT::i32, Custom); // → RVXISD::SUBW

        // 32-bit shifts on RV64 → SLLW, SRLW, SRAW (W-suffix shift instructions)
        setOperationAction(ISD::SHL, MVT::i32, Custom); // → RVXISD::SLLW
        setOperationAction(ISD::SRL, MVT::i32, Custom); // → RVXISD::SRLW
        setOperationAction(ISD::SRA, MVT::i32, Custom); // → RVXISD::SRAW

        if (STI.hasStdExtM())
            setOperationAction(ISD::MUL, MVT::i32, Custom);
    }

    if (STI.hasStdExtF()) {
        setOperationAction(ISD::FADD,  MVT::f32, Legal);
        setOperationAction(ISD::FSUB,  MVT::f32, Legal);
        setOperationAction(ISD::FMUL,  MVT::f32, Legal);
        setOperationAction(ISD::FDIV,  MVT::f32, Legal);
        setOperationAction(ISD::FSQRT, MVT::f32, Legal);
        setOperationAction(ISD::FABS,  MVT::f32, Legal);
        setOperationAction(ISD::FNEG,  MVT::f32, Legal);
        setOperationAction(ISD::FMA,   MVT::f32, Legal);
        setOperationAction(ISD::FMINNUM, MVT::f32, Legal);
        setOperationAction(ISD::FMAXNUM, MVT::f32, Legal);
    } else {
        setSoftFloatImpls(MVT::f32);
    }

    if (STI.hasStdExtD()) {
        setOperationAction(ISD::FADD,  MVT::f64, Legal);
        setOperationAction(ISD::FSUB,  MVT::f64, Legal);
        setOperationAction(ISD::FMUL,  MVT::f64, Legal);
        setOperationAction(ISD::FDIV,  MVT::f64, Legal);
        setOperationAction(ISD::FSQRT, MVT::f64, Legal);
        setOperationAction(ISD::FABS,  MVT::f64, Legal);
        setOperationAction(ISD::FNEG,  MVT::f64, Legal);
        setOperationAction(ISD::FMA,   MVT::f64, Legal);
        setOperationAction(ISD::FMINNUM, MVT::f64, Legal);
        setOperationAction(ISD::FMAXNUM, MVT::f64, Legal);
    }

    if (STI.hasStdExtF()) {
        setOperationAction(ISD::FP_TO_SINT, XLenVT, Legal);
        setOperationAction(ISD::FP_TO_UINT, XLenVT, Legal);
        setOperationAction(ISD::SINT_TO_FP, MVT::f32, Legal);
        setOperationAction(ISD::UINT_TO_FP, MVT::f32, Legal);
    }

     // Sign-extending loads: Legal for i8 and i16 to any legal integer type.
    setLoadExtAction(ISD::SEXTLOAD, XLenVT, MVT::i1,  Promote);
    setLoadExtAction(ISD::SEXTLOAD, XLenVT, MVT::i8,  Legal);  // LB
    setLoadExtAction(ISD::SEXTLOAD, XLenVT, MVT::i16, Legal);  // LH

    // Zero-extending loads: Legal for i8 and i16.
    setLoadExtAction(ISD::ZEXTLOAD, XLenVT, MVT::i1,  Promote);
    setLoadExtAction(ISD::ZEXTLOAD, XLenVT, MVT::i8,  Legal);  // LBU
    setLoadExtAction(ISD::ZEXTLOAD, XLenVT, MVT::i16, Legal);  // LHU

    // Any-extending loads: use zero-extension (cheapest).
    setLoadExtAction(ISD::EXTLOAD,  XLenVT, MVT::i1,  Promote);
    setLoadExtAction(ISD::EXTLOAD,  XLenVT, MVT::i8,  Legal);
    setLoadExtAction(ISD::EXTLOAD,  XLenVT, MVT::i16, Legal);

     // On RV64: 32-bit loads zero-extend or sign-extend to 64 bits.
     if (STI.is64Bit()) {
        setLoadExtAction(ISD::SEXTLOAD, MVT::i64, MVT::i32, Legal); // LW (sign)
        setLoadExtAction(ISD::ZEXTLOAD, MVT::i64, MVT::i32, Legal); // LWU (zero)
        setLoadExtAction(ISD::EXTLOAD,  MVT::i64, MVT::i32, Legal);
    }

    // Float loads: FLW (f32) and FLD (f64) are Legal with F/D extensions.
     if (STI.hasStdExtF()) {
        setLoadExtAction(ISD::EXTLOAD, MVT::f32, MVT::f16, Expand); // no f16 load
    }
    if (STI.hasStdExtD()) {
        setLoadExtAction(ISD::EXTLOAD, MVT::f64, MVT::f32, Expand); // no fp-extend load
    }

    setTruncStoreAction(XLenVT, MVT::i8,  Legal); // SB
    setTruncStoreAction(XLenVT, MVT::i16, Legal); // SH

    // Truncating stores to i1 must be promoted (no sub-byte store instruction).
    setTruncStoreAction(XLenVT, MVT::i1, Expand);

    // On RV64: truncating store to i32 is Legal (SW).
    if (STI.is64Bit())
        setTruncStoreAction(MVT::i64, MVT::i32, Legal); // SW

    if (STI.hasStdExtF()) {
        setCondCodeAction(ISD::SETONE, MVT::f32, Expand);
        setCondCodeAction(ISD::SETUEQ, MVT::f32, Expand);
        setCondCodeAction(ISD::SETUGT, MVT::f32, Expand);
        setCondCodeAction(ISD::SETUGE, MVT::f32, Expand);
        setCondCodeAction(ISD::SETULT, MVT::f32, Expand);
        setCondCodeAction(ISD::SETULE, MVT::f32, Expand);
    }
}

const char *RVXTargetLowering::getTargetNodeName(unsigned Opcode) const {
    switch ((RVXISD::NodeType)Opcode) {
        case RVXISD::FIRST_NUMBER:    break;
        case RVXISD::HI20:            return "RVXISD::HI20";
        case RVXISD::ADD_LO:          return "RVXISD::ADD_LO";
        case RVXISD::PCREL_HI20:      return "RVXISD::PCREL_HI20";
        case RVXISD::PCREL_LO:        return "RVXISD::PCREL_LO";
        case RVXISD::CALL:            return "RVXISD::CALL";
        case RVXISD::RET_FLAG:        return "RVXISD::RET_FLAG";
        case RVXISD::TAIL:            return "RVXISD::TAIL";
        case RVXISD::SELECT_CC:       return "RVXISD::SELECT_CC";
        case RVXISD::SLLW:            return "RVXISD::SLLW";
        case RVXISD::SRLW:            return "RVXISD::SRLW";
        case RVXISD::SRAW:            return "RVXISD::SRAW";
        case RVXISD::ADDW:            return "RVXISD::ADDW";
        case RVXISD::SUBW:            return "RVXISD::SUBW";
        case RVXISD::LR_W:            return "RVXISD::LR_W";
        case RVXISD::SC_W:            return "RVXISD::SC_W";
        case RVXISD::LR_D:            return "RVXISD::LR_D";
        case RVXISD::SC_D:            return "RVXISD::SC_D";
        case RVXISD::FMOV_W_GPR:      return "RVXISD::FMOV_W_GPR";
        case RVXISD::FMOV_GPR_W:      return "RVXISD::FMOV_GPR_W";
        case RVXISD::LAST_RVX_ISD_OPCODE: break;
        }
        return nullptr;
    }

MVT RVXTargetLowering::getXLenVT() const {
    return STI.getXLenVT();
}

SDValue RVXTargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const{

    switch (Op.getOpcode()){
        case ISD::GlobalAddress: 
            return LowerGlobalAddress(Op, DAG);
        case ISD::BlockAddress: 
            return LowerBlockAddress(Op, DAG);
        case ISD::ExternalSymbol: 
            return LowerExternalSymbol(Op, DAG);
        case ISD::JumpTable: 
            return LowerJumpTable(Op, DAG);
        case ISD::ConstantPool: 
            return LowerConstantPool(Op, DAG); 

        case ISD::VASTART:          
            return LowerVASTART(Op, DAG);
        case ISD::SELECT:
        case ISD::SELECT_CC:        
            return LowerSELECT(Op, DAG);
        case ISD::BR_CC:            
            return LowerBRCOND(Op, DAG);

         // ---- RV64 32-bit operations --------------------------------------------
        case ISD::ADD:
            if (STI.is64Bit() && Op.getValueType() == MVT::i32)
                return DAG.getNode(RVXISD::ADDW, SDLoc(Op), MVT::i32,
                                Op.getOperand(0), Op.getOperand(1));
            break;
        case ISD::SUB:
            if (STI.is64Bit() && Op.getValueType() == MVT::i32)
                return DAG.getNode(RVXISD::SUBW, SDLoc(Op), MVT::i32,
                                Op.getOperand(0), Op.getOperand(1));
            break;
        case ISD::SHL:
            if (STI.is64Bit() && Op.getValueType() == MVT::i32)
                return DAG.getNode(RVXISD::SLLW, SDLoc(Op), MVT::i32,
                                Op.getOperand(0), Op.getOperand(1));
            break;
        case ISD::SRL:
            if (STI.is64Bit() && Op.getValueType() == MVT::i32)
                return DAG.getNode(RVXISD::SRLW, SDLoc(Op), MVT::i32,
                                   Op.getOperand(0), Op.getOperand(1));
            break;
        case ISD::SRA:
            if (STI.is64Bit() && Op.getValueType() == MVT::i32)
                return DAG.getNode(RVXISD::SRAW, SDLoc(Op), MVT::i32,
                                Op.getOperand(0), Op.getOperand(1));
            break;

        default:
            llvm_unreachable("LowerOperation called on node that is not Custom");   
     }

    return SDValue(); 

}

