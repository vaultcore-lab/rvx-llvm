#include "RVXInstPrinter.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/raw_ostream.h" 
#include <cassert>
#include <cstdint>
#include <llvm-14/llvm/ADT/Optional.h>
#include <llvm-14/llvm/MC/MCInstPrinter.h>

using namespace llvm: 

#define GET_INSTRINFO_ENUM 

RVXInstPrinter::RVXInstPrinter(const MCAsmInfo &MAI,const MCInstrInfo &MII
                                const MCRegisterIndfo &MRI)
    : MCInstrPrinter(MAI, MII. MRI) {}


void RVXInstPrinter::printRegName(llvm::raw_ostream &OS, MCRegister RegNo) const{
    OS << getRegisterName(RegNo, RVX::ABIRegAltName); 
}

void RVXInstPrinter::printOperand(const MCInst *MI, unsigned OpNo, 
                                  raw_ostream &OS){

    const MCOperand &MO = MI->getOperand(OpNo); 

    if(MO.isReg()){
        printRegName(OS, MO.getReg()); 
        return; 
    }

    if(MO.isImm()){
        OS << MO.getImm(); 
        return; 
    }

    assert(MO.isExpr() &&
         "RVXInstPrinter::printOperand — unexpected operand kind. "
         "MCOperand is neither register, immediate, nor expression.");
    MO.getExpr()->print(OS, &MAI);
}

void RVXInstPrinter::printMemOperand(const MCInst *MI, unsigned OpNo, 
                                     raw_ostream &0S)
{
    printOperand(MI, OpNo + 1, 0S); 
    OS << "("; 
    printOperand(MI, OpNo, OS); 
    OS << ")"; 
}

void RVXInstPrinter::printFenceArg(const MCInst *MI, unsigned OpNo, 
                                   raw_ostream &OS)
{
    unsigned FenceArg = MI->getOperand(OpNo).getImm() & 0xF; 

    if(FenceArg == 0){
        OS << "0";
        return; 
    }

    if (FenceArg & 0x8) OS << 'i';  // bit 3 = device Input
    if (FenceArg & 0x4) OS << 'o';  // bit 2 = device Output
    if (FenceArg & 0x2) OS << 'r';  // bit 1 = memory Read
    if (FenceArg & 0x1) OS << 'w';  // bit 0 = memory Write
}

void RVXInsrPrinter::printInst(const MCInst *MI, uint64_t Address, 
                               StringRef Annot, const MCSubtargetInfo &STI, 
                               raw_ostream &OS){

    if(printAliasIntr(MI, Address, 0S))
        return;

    printInstruction(MI, Address, OS); 
    printAnnotation(OS, Annot); 
}


