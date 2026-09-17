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

SDValue RVXTargetLowering::lowerSymbolAddress(SDValue Op, SelectionDAG &DAG, 
                                              SDValue TargetNode) const{

    SDLoc DL(Op); 
    MVT Ty = getXLenVT(); 
    cosnt GlobalValue *GV = nullptr; 

    bool IsLocal = getTargetMachine().shouldAssumeDSOLocal(
        *DAG.getMachineFunction().getFunction().getParent(), GV);

    if(getTargetMachine().gerRelocationMode() == Reloc::Static || IsLocal){
        //absolute addressing 
        //%hi(symbol)
        //%lo(symbol)

        SDValue Hi = DAG.getNode(RVXISD::HI20, DL, Ty, TargetNode); 

        //take hi2o result and adds to %lo bits 
        SDValue Lo = DAG.getNode(RVXISD::ADD_LO, DL, Ty, Hi, TargetNode); 

        return Lo; 
    }else{
        //PC-relative addressing
        SDValue Hi = DAG.getNode(RVXISD::PCREL_HI20, DL, Ty, TargetNode); 
        SDValue Lo = DAG.getNode(RVXISD::PCREL_LO, DL, Ty, Hi, TargetNode);
        return Lo; 
    }
}

SDValue RVXTargetLowering::LowerGlobalAddress(SDValue Op, SelectionDAG &DAG)const{

    SDLoc DL(Op); 
    MVT Ty = getXLenVT(); 
    const GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op.getNode()); 
    int64_t Offset = N->getOffset(); 
    const GlobalValue *GV = N->getGlobal();

    SDValue TargetNode = DAG.getTargetGlobalAddress(GV, DL, TY, 0); 
    SDValue Addr = lowerSymbolAddress(Op, DAG, TargetNode); 

    if(Offset != 0)
    Addr = DAG.getNode(ISD::ADD, DL, Ty, Addr, 
                       DAG.getConstant(Offset, DL, Ty));

    return Addr; 
}

SDValue RVXTargetLowering::LowerBlockAddress(SDValue Op,
                                              SelectionDAG &DAG) const {
    SDLoc DL(Op);
    MVT Ty = getXLenVT();
    const BlockAddressSDNode *N = cast<BlockAddressSDNode>(Op);

    SDValue TargetNode = DAG.getTargetBlockAddress(N->getBlockAddress(), Ty);
    return lowerSymbolAddress(Op, DAG, TargetNode);
}

SDValue RVXTargetLowering::LowerExternalSymbol(SDValue Op,
                                                SelectionDAG &DAG) const {
    SDLoc DL(Op);
    MVT Ty = getXLenVT();
    const ExternalSymbolSDNode *N = cast<ExternalSymbolSDNode>(Op);

    SDValue TargetNode = DAG.getTargetExternalSymbol(N->getSymbol(), Ty);
    return lowerSymbolAddress(Op, DAG, TargetNode);
}

SDValue RVXTargetLowering::LowerJumpTable(SDValue Op,
                                           SelectionDAG &DAG) const {
    SDLoc DL(Op);
    MVT Ty = getXLenVT();
    const JumpTableSDNode *N = cast<JumpTableSDNode>(Op);

    SDValue TargetNode = DAG.getTargetJumpTable(N->getIndex(), Ty);
    return lowerSymbolAddress(Op, DAG, TargetNode);
}

SDValue RVXTargetLowering::LowerConstantPool(SDValue Op,
                                              SelectionDAG &DAG) const {
    SDLoc DL(Op);
    MVT Ty = getXLenVT();
    const ConstantPoolSDNode *N = cast<ConstantPoolSDNode>(Op);

    SDValue TargetNode;
    if (N->isMachineConstantPoolEntry()){
        TargetNode = DAG.getTargetConstantPool(N->getMachineCPVal(), Ty,
                                               N->getAlign(), N->getOffset());
    }else{
        TargetNode = DAG.getTargetConstantPool(N->getConstVal(), Ty,
                                               N->getAlign(), N->getOffset());
    }
    return lowerSymbolAddress(Op, DAG, TargetNode);
}


SDValue RVXTargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
    MachineFunction &MF = DAG.getMachineFunction();
    RVXMachineFunctionInfo *FuncInfo = MF.getInfo<RVXMachineFunctionInfo>();
    SDLoc DL(Op);

    // 1. Extract operands from ISD::VASTART node
    SDValue Chain = Op.getOperand(0);
    SDValue VAListPtr = Op.getOperand(1); // Pointer to the va_list location
    const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();

    // Create the FrameIndex SDValue for the varargs save area
    int VarArgsFI = FuncInfo->getVarArgsFrameIndex();
    EVT PtrVT = getPointerTy(DAG.getDataLayout());
    SDValue VarArgsAddr = DAG.getFrameIndex(VarArgsFI, PtrVT);

    return DAG.getStore(Chain, DL, VarArgsAddr, VAListPtr, MachinePointerInfo(SV));
}

SDValue RVXTargetLowering::LowerSELECT(SDValue, Op, 
                                       SelectionDAG &DAG) const{

    SDValue CondV = Op.getOperand(0); 
    SDValue TrueV = Op.getOperand(1); 
    SDValue FalseV = Op.getOperand(2); 
    MVT Ty = Op.getSimpleValueType();

    if(CondV.getOpcode() == ISD::SETCC &&
       CondV.getOperand(0).getValueType() == getXLenVT()){

    }

}

SDValue RVXTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg, 
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL, 
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const{


    MachineFunction &MF = DAG.getMachineFunction(); 
    MachineFrameInfo &MFI = MF.getFrameInfo(); 
    MachineRegisterInfo &MRI = MF.getRegInfo(); 
    RVXMachineFunctionInfo *RVXFI = MF.getInfo<RVXMachineFunctionInfo>(); 

    //calling convention analyzer 
    SmallVector<CCValAssign, 16> ArgLocs; 
    CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
    CCInfo.AnalyzeFormalArguments(Ins, CC_RVX);

    for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i){
        CCValAssign &VA = ArgLocs[i]; 

        if(VA.isRegLoc()) {
            MVT RegVT = VA.getLocVT(); 

            const TargetRegisterClass *RC; 
            if(RegVT == MVT::i32 || RegVT == MVT::i64)
                RC = &RVX::GRPRegsRegClass; 
            else if (RegVT == MVT::f32)
                RC = &RVX::FPR32RegClass;
            else if (RegVT == MVT::f64)
                RC = &RVX::FPR64RegClass;
            else
                llvm_unreachable("Unexpected register type in formal argument");

            Register VReg = MRI.createVirtualRegister(RC); 
            MF.getRegInfo().addLiveIn(VA.getLocREG(), VReg); 

            SDValue ArgIn = DAG.getCopyFromReg(Chain, DL, VReg, RegVT); 

            DValue ArgValue;
            switch (VA.getLocInfo()) {
            case CCValAssign::Full:
                // Register holds exactly the argument type — no conversion needed.
                ArgValue = ArgIn;
                break;
            case CCValAssign::BCvt:
                // Bitcast: the register type and argument type have same bit width
                // but different EVT (e.g. f32 in an i32 register on soft-float).
                ArgValue = DAG.getNode(ISD::BITCAST, DL, VA.getValVT(), ArgIn);
                break;
            case CCValAssign::SExt:
                // Sign-extended — truncate from register type to argument type.
                ArgValue = DAG.getNode(ISD::AssertSext, DL, RegVT, ArgIn,
                                    DAG.getValueType(VA.getValVT()));
                ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);
                break;
            case CCValAssign::ZExt:
                // Zero-extended — truncate from register type to argument type.
                ArgValue = DAG.getNode(ISD::AssertZext, DL, RegVT, ArgIn,
                                    DAG.getValueType(VA.getValVT()));
                ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);
                break;
            case CCValAssign::AExt:
                // Any-extended — truncate without asserting sign/zero.
                ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgIn);
                break;
            default:
                llvm_unreachable("Unexpected CCValAssign LocInfo for register arg");
            }

            InVals.push_back(ArgValue);
        }else {

            //argument is on the stack 
            assert(VA.isMemeLoc() && "Expected mem loc for non-reg argument");
            
            int FI = MFI.CreateFixedObject(VA.getValVT().getSizeInBits() / 8,
                                      VA.getLocMemOffset(),
                                      /*Immutable=*/true);

            // Create a load node that reads the argument from the stack slot.
            SDValue FIPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
            SDValue ArgValue = DAG.getLoad(
                VA.getValVT(), DL, Chain, FIPtr,
                MachinePointerInfo::getFixedStack(MF, FI));

            InVals.push_back(ArgValue);
        }
    }

    if (IsVarArg) {
        // Find the first unused argument register.
        // CCInfo.getFirstUnallocated(ArgRegs) returns the index of the first
        // register in ArgRegs[] that wasn't assigned to a fixed argument.
        static const MCPhysReg ArgRegs[] = {
            RVX::X10, RVX::X11, RVX::X12, RVX::X13,
            RVX::X14, RVX::X15, RVX::X16, RVX::X17  // a0-a7
        };
        unsigned FirstVAReg = CCInfo.getFirstUnallocated(ArgRegs);

        if (FirstVAReg < array_lengthof(ArgRegs)) {
        // There are unused argument registers — spill them to the vararg area.
        // Allocate a fixed-object frame slot for each remaining register.
        // The slots are placed contiguously in the order a0, a1, ..., a7.
        int VarArgsFI = MFI.CreateFixedObject(
            (array_lengthof(ArgRegs) - FirstVAReg) * (STI.is64Bit() ? 8 : 4),
            CCInfo.getNextStackOffset(),
            /*Immutable=*/false);

        RVXFI->setVarArgsFrameIndex(VarArgsFI);

        // Spill each remaining argument register to its slot.
        SmallVector<SDValue, 8> MemOps;
        for (unsigned i = FirstVAReg; i < array_lengthof(ArgRegs); ++i) {
            Register VReg = MRI.createVirtualRegister(&RVX::GRPRegsRegClass);
            MRI.addLiveIn(ArgRegs[i], VReg);
            SDValue Val = DAG.getCopyFromReg(Chain, DL, VReg, getXLenVT());

            // Compute the offset of this register's slot within the vararg area.
            int Offset = (i - FirstVAReg) * (STI.is64Bit() ? 8 : 4);
            int FI = MFI.CreateFixedObject(STI.is64Bit() ? 8 : 4,
                                            CCInfo.getNextStackOffset() + Offset,
                                            /*Immutable=*/false);
            SDValue FIPtr =
                DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));

            MemOps.push_back(DAG.getStore(Chain, DL, Val, FIPtr,
                                        MachinePointerInfo::getFixedStack(MF, FI)));
        }

        // Chain all the spill stores together.
        if (!MemOps.empty())
            Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOps);
        }
    }
    return Chain;
}

SDValue RVXTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI, 
                                     SmallVectorImpl<SDValue> &InVals) const{

    SelectionDAG &DAG = CLI.DAG; 
    SDLoc &DL = CLI.DL; 
    SmallVectorImpl<ISD::OutputArg> Outs = CLI.Outs; 
    SmallVectorImpl<ISD::SDValue> &OutVals = CLI.OutVals; 
    SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins; 
    SDValue Chain = CLI.Chain; 
    SDValue Callee = CLI.Callee;  
    bool IsVargArg = CLI.isVarArg; 
    CallingConv::ID CallConv = CLI.CallConv; 
    MachineFunction &MF = DAG.getMachineFunction();

    SmallVector<CCValAssign, 16> ArgLoc; 
    CCState ArgCCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
    ArgCCInfo.AnalyzeCllOperand(Outs, CC_RVX);

    unsigned NumBytes = ArgCCInfo.getNextStackOffset(); 
    NumBytes = alignTo(NumBytes, STI.is64Bit ? 8 : 4); 

    Chain = DAG.getCALLSEQ_START(Chain, NumBytes, , DL);

    SmallVector<std::pair<Register, SDValue>, 8> RegsToPass; 
    SmallVector<SDValue, 8> MemOpChains; 


    for(unsigned i = 0; e = ArgLocs.size(); i != e; i++){
        CCValAssign &VA = ArgLocs[i]; 
        SDValue ArgVal = OutVals[i]; 

        switch(VA.getLocInfo()){
            case CCValAssign::Full: break; 
            case CCValAssign::SExt:
                ArgVal = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), ArgVal); 
                break; 
            case CCValAssign::ZExt:
                ArgVal = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), ArgVal);
                break;
            case CCValAssign::AExt:
                ArgVal = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), ArgVal);
                break;
            case CCValAssign::BCvt:
                ArgVal = DAG.getNode(ISD::BITCAST, DL, VA.getLocVT(), ArgVal);
                break;
            default:
                llvm_unreachable("Unexpected CCValAssign LocInfo for call argument");
        }

        if (VA.isRegLoc()) {
            // Argument goes in a register — record it for the CopyToReg chain.
            RegsToPass.push_back({VA.getLocReg(), ArgVal});
        } else {
            // Argument goes on the stack — emit a store to the outgoing arg area.
            assert(VA.isMemLoc());
            SDValue StackPtr = DAG.getCopyFromReg(Chain, DL, RVX::X2, // sp
                                             getPointerTy(DAG.getDataLayout()));
            SDValue PtrOff = DAG.getIntPtrConstant(VA.getLocMemOffset(), DL);
            SDValue StoreAddr = DAG.getNode(ISD::ADD, DL,
                                      getPointerTy(DAG.getDataLayout()),
                                      StackPtr, PtrOff);
            MemOpChains.push_back(
                DAG.getStore(Chain, DL, ArgVal, StoreAddr,
                       MachinePointerInfo::getStack(MF, VA.getLocMemOffset())));
        }
    }

    if (!MemOpChains.empty())
        Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);
    
    SDValue Glue; 
    for(auto &[Reg, Val] : RegsToPass){
        Chain = DAG.getCopyFromReg(Chain DL, Reg, Val, Glue); 
        Glue = chain.getValue(1); 
    }

    SmallVector<SDValue, 8> Ops;
    Ops.push_back(Chain);
    Ops.push_back(Callee);

    for (auto &[Reg, Val] : RegsToPass)
        Ops.push_back(DAG.getRegister(Reg, Val.getValueType()));

      // Add the glue to ensure the CopyToReg nodes happen before the CALL.
    if (Glue.getNode())
        Ops.push_back(Glue); 

    //Build the RVX::CALL node
    SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue); 
    Chain = DAG.getNode(RVXISD::CALL, DL, NodeTys, Ops); 
    Glue = Chain.getValue(1); 

    Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
    Glue = Chain.getValue(1); 

    SmallVector<CCValAssign, 4> RetLocs;
    CCState RetCCInfo(CallConv, IsVarArg, MF, RetLocs, *DAG.getContext());
    RetCCInfo.AnalyzeCallResult(Ins, RetCC_RVX);

    for (unsigned i = 0, e = RetLocs.size(); i != e; ++i) {
        CCValAssign &VA = RetLocs[i];
        // CopyFromReg extracts the return value from the physical return register.
        // Glue ensures this happens after the CALL completes.
        Chain = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(),
                               VA.getLocVT(), Glue).getValue(1);
        Glue = Chain.getValue(2);

        SDValue RetVal = Chain.getValue(0);

        // Truncate back to the declared return type if it was extended.
        switch (VA.getLocInfo()) {
            case CCValAssign::Full:  break;
            case CCValAssign::SExt:
                RetVal = DAG.getNode(ISD::AssertSext, DL, VA.getLocVT(), RetVal,
                                     DAG.getValueType(VA.getValVT()));
                RetVal = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), RetVal);
                break;
            case CCValAssign::ZExt:
                RetVal = DAG.getNode(ISD::AssertZext, DL, VA.getLocVT(), RetVal,
                                     DAG.getValueType(VA.getValVT()));
                RetVal = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), RetVal);
                break;
            case CCValAssign::AExt:
                RetVal = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), RetVal);
                break;
            default:
                llvm_unreachable("Unexpected CCValAssign LocInfo for return value");
        }

    InVals.push_back(RetVal); 
    }

    return Chain; 
}


