#include "RVXRegisterInfo.h"
#include "RVXFrameLowering.h"
#include "RVXInstrInfo.h"   
#include "RVXMachineFunctionInfo.h" 
#include "RVXSubtarget.h"       
#include "Subtarget.h"

#include "llvm/CodeGen/MachineFrameInfo.h"   
#include "llvm/CodeGen/MachineFunction.h"    
#include "llvm/CodeGen/MachineInstrBuilder.h" 
#include "llvm/CodeGen/RegisterScavenging.h" 
#include "llvm/Support/ErrorHandling.h"
#include <cstdint>
#include <llvm-14/llvm/ADT/BitVector.h>
#include <llvm-14/llvm/IR/CallingConv.h>
#include <llvm-14/llvm/MC/MCRegister.h>

#define GET_REGINFO_TARGET_DESC
#include "RVXGenRegisterInfo.inc"

using namespace llvm;

RegisterInfo::RVXRegisterInfo(const RVXSubtarget &ST)
    : RVXGenRegisterInfo(RVX:X1){}

const MCPhysReg *
RVXRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const
{
    const RVXSubtarget &STI = MF->getSubtarget<RVXSubtarget>(); 

    switch(MF->getFunction().getCallingConv()){
        default:
        
            if(STI.hasStdExtD() || STI.hasStdExtF())
                return CSR_ILP32_LP64F_SaveList; 
        
            return CSR_ILP32_LP64_SaveList; 
        case CallingConv::GHC: 
            return CSR_NoRegs_SaveList;    
    }
}

const uint32_t * 
RVXRegisterInfo::getCallPreservedMask(const MachineFunction &MF, 
                                      CallingConv::ID CC) const {
    const RVXSubtarget &STI = MF.getSubtarget<RVXSubtarget>(); 

    switch(CC){
        default:
            if(STI.hasStdExtD() || STI.hasStdExtF())
                return CSR_ILP32F_LP64F_RegMask; 
            return CSR_ILP32_LP32_RegMask; 
        
        case CallingConv::GHC:
            return CSR_NoRegs_RegMask; 
    }
}

BitVector 
RVXRegisterInfo::getReservedRegs(const MachineFunction &MF) const{

    const RVXSubtarget &STI = MF.getSubtarget<RVXSubtarget>(); 
    const RVXFrameLowering *TFI = getFrameLowering(MF); 
    
    /*BitVector size of total physical registers */ 
    BitVector Reserved(getNumRegs());

    markSuperRegs(Reserved, RVX::X0); 
}
