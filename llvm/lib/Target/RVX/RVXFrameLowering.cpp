#include "RVXFrameLowering.h"
#include "RVXInstrInfo.h"         
#include "RVXMachineFunctionInfo.h" 
#include "RVXRegisterInfo.h"        
#include "RVXSubtarget.h"          

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"  
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"  
#include "llvm/CodeGen/MachineRegisterInfo.h" 
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/Function.h"       
#include "llvm/MC/MCDwarf.h"        
#include "llvm/Support/MathExtras.h" 

using namespace llvm;


RVXFrameLowering::RVXFrameLowering(const RVXSubtarget &STI){
    : TargetFrameLowering(StackGrowsDown, 
                          Align(16), 
                          0), 
        STI(STI) {}

bool RVXFrameLowering::hasFP(const MachineFunction &MF) const{
    const MachineFrameInfo &MFI = MF.getFrameInfo(); 
    const TargetRegisterInfo *TRI = STI.getRegisterInfo(); 

    if (MF.getTarget().Options.DisableFramePointerElim(MF))
        return true;

    if (MFI.hasVarSizedObjects())
        return true;

    if (MFI.isFrameAddressTaken())
        return true;

    if (TRI->requiresRegisterScavenging(MF) && !MFI.hasVarSizedObjects()) {
    }

    return false;
}
void RVXFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                             BitVector &SavedRegs,
                                             RegScavenger *RS) const {
    TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

    if (MF.getFrameInfo().hasCalls()) {
        // Mark ra (x1) as needing a stack save slot.
        SavedRegs.set(RVX::X1); // x1 = ra
    }

    if (hasFP(MF)) {
        SavedRegs.set(RVX::X8); // x8 = s0/fp
    }
}

// ============================================================================
// emitPrologue
// ============================================================================
// Inserts the function prologue into the entry basic block MBB.
// At the point this is called, MBB already contains:
//   - Callee-save SPILL instructions (storeRegToStackSlot calls from PEI),
//     but using the UNALLOCATED sp value (above the frame).
//   - The first real user instructions after that.
//
// We insert the sp adjustment BEFORE the spills so that all stack accesses
// use the correct adjusted sp. Wait — actually for RISC-V the convention is:
//   SW ra,  (sp + frameSize - 8)   ← uses OLD sp (before adjustment)
//   SW s0,  (sp + frameSize - 16)  ← uses OLD sp
//   ADDI sp, sp, -frameSize        ← adjusts sp
//
// The spills use POSITIVE offsets from the old sp. After the sp adjustment,
// these same locations are at frameSize-8 and frameSize-16 from the new sp.
// PEI handles the offset adjustment automatically.
void RVXFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  // Fetch the objects we need throughout.
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const RVXInstrInfo *TII =
      static_cast<const RVXInstrInfo *>(STI.getInstrInfo());
  const RVXRegisterInfo *TRI =
      static_cast<const RVXRegisterInfo *>(STI.getRegisterInfo());
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  // ---- Determine total frame size ----------------------------------------
  // The frame size is computed by PEI from:
  //   - Fixed objects (locals with a specific offset, e.g. incoming stack args)
  //   - Regular objects (local variables, spill slots, callee-save slots)
  //   - Outgoing argument area (space for args passed on the stack to callees)
  // PEI calls MFI.setStackSize() before calling emitPrologue, so we can
  // read the final size directly.
  //
  // We MUST round up to the stack alignment (16 bytes) to satisfy the psABI.
  // alignTo(N, Align) rounds N up to the next multiple of Align.
  uint64_t StackSize = alignTo(MFI.getStackSize(), getStackAlign());
  MFI.setStackSize(StackSize); // store back the aligned size

  // If the frame is empty (no locals, no callee saves, no alloca), we have
  // nothing to do. This is common for tiny leaf functions.
  if (StackSize == 0 && !hasFP(MF))
    return;

  // ---- Insert the sp adjustment -------------------------------------------
  // We must subtract StackSize from sp. RISC-V ADDI has a 12-bit signed
  // immediate, so we handle two cases:

  if (isInt<12>(-static_cast<int64_t>(StackSize))) {
    // ---- CASE A: Small frame (fits in ADDI's 12-bit immediate) ------------
    //
    // ADDI sp, sp, -StackSize
    //
    // This is the common case for functions with frames up to 2047 bytes.
    // A single ADDI both allocates the frame and maintains the ABI guarantee
    // that sp is always 16-byte aligned (ensured by the alignTo above).
    //
    // The CFI directive .cfi_def_cfa_offset tells the DWARF unwinder that
    // the CFA (Canonical Frame Address = original sp) is now at sp + StackSize.
    // After this instruction, the unwinder can find the original sp by reading
    // the current sp and adding StackSize.

    BuildMI(MBB, MBBI, DL, TII->get(RVX::ADDI), RVX::X2 /*sp*/)
        .addReg(RVX::X2)           // source: sp
        .addImm(-static_cast<int64_t>(StackSize)); // immediate: -frameSize

    // Emit CFI: CFA is now at sp + StackSize (the unwinder must add StackSize
    // to the current sp to find the caller's original sp value).
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, StackSize));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

  } else {
    // ---- CASE B: Large frame (> 2047 bytes, doesn't fit in ADDI) ----------
    //
    // We need to materialise StackSize in a scratch register, then subtract.
    // Strategy:
    //   LUI  t0, %hi(StackSize)     → t0 = upper 20 bits
    //   ADDI t0, t0, %lo(StackSize) → t0 = full StackSize
    //   SUB  sp, sp, t0             → sp -= StackSize
    //
    // We use t0 (x5) as the scratch register. t0 is:
    //   - A CALLER-SAVED temporary, so it's not live on entry.
    //   - Available at function entry before any instructions run.
    //   - NOT in the callee-save list, so we don't need to save it.
    //
    // The %hi/%lo split with the +0x800 compensation (for ADDI sign extension)
    // is the same pattern as %hi/%lo in the code emitter:
    //   %hi(N) = (N + 0x800) >> 12   (upper 20 bits, rounded for sign ext)
    //   %lo(N) = N - (%hi(N) << 12)  (lower 12 bits, sign-adjusted)

    int64_t Hi = ((int64_t)StackSize + 0x800) >> 12;
    int64_t Lo = (int64_t)StackSize - (Hi << 12);

    // LUI t0, Hi — loads Hi into t0[31:12], zeros t0[11:0]
    BuildMI(MBB, MBBI, DL, TII->get(RVX::LUI), RVX::X5 /*t0*/)
        .addImm(Hi);

    // ADDI t0, t0, Lo — adds the sign-adjusted lower 12 bits
    // After this instruction: t0 == StackSize exactly
    BuildMI(MBB, MBBI, DL, TII->get(RVX::ADDI), RVX::X5 /*t0*/)
        .addReg(RVX::X5, RegState::Kill) // kill: we redefine t0 here
        .addImm(Lo);

    // SUB sp, sp, t0 — sp -= t0 (sp -= StackSize)
    // RISC-V SUB is: rd = rs1 - rs2
    BuildMI(MBB, MBBI, DL, TII->get(RVX::SUB), RVX::X2 /*sp*/)
        .addReg(RVX::X2)               // rs1 = sp
        .addReg(RVX::X5, RegState::Kill); // rs2 = t0 (killed — no longer needed)

    // CFI for large frame: same as small frame — CFA = sp + StackSize.
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, StackSize));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  // ---- Emit CFI for each callee-saved register ----------------------------
  // For each register that PEI decided to save (determined by determineCalleeSaves),
  // we emit a .cfi_offset directive telling the DWARF unwinder WHERE that
  // register was saved on the stack.
  //
  // The offset is relative to the CFA (original sp). Since we saved registers
  // at sp+frameSize-8, sp+frameSize-16, etc. (using the OLD sp), and CFA = sp+frameSize,
  // these locations are at CFA - 8, CFA - 16, etc.
  //
  // The MachineFrameInfo stores the object offsets that PEI assigned.
  // We iterate over the callee-save info to find each register's slot.
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  for (const CalleeSavedInfo &CS : CSI) {
    // Get the frame slot offset for this callee-saved register.
    // MFI.getObjectOffset() returns the offset from the frame base pointer
    // (which for RISC-V is sp after adjustment). We need the offset from CFA.
    int64_t Offset = MFI.getObjectOffset(CS.getFrameIdx());

    // DWARF register number: the unwinder uses DWARF register numbers (not
    // LLVM internal numbers). MCRegisterInfo::getDwarfRegNum() translates.
    Register Reg = CS.getReg();
    unsigned DwarfReg = TRI->getDwarfRegNum(Reg, true /*isEH*/);

    // .cfi_offset DwarfReg, Offset — "register DwarfReg is saved at CFA+Offset"
    // Since Offset is negative (below CFA), this effectively says
    // "register is at CFA - |Offset|".
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::createOffset(nullptr, DwarfReg, Offset));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  // ---- Set up the frame pointer (if hasFP) --------------------------------
  // If we decided to use a frame pointer, set fp = sp + StackSize.
  // After this, fp points to the ORIGINAL sp (the value sp had on function entry).
  // This is the CFA. All fp-relative frame accesses will work correctly.
  //
  // ADDI s0, sp, StackSize
  //   s0 = sp + StackSize = original sp = CFA
  //
  // If StackSize doesn't fit in 12 bits, we need a different approach:
  //   ADD s0, sp, t0  (but t0 was killed above — we'd need to re-materialise)
  //   For large frames with fp, emit: ADD s0, sp, t0 (where t0 still holds
  //   StackSize — but we killed it). A correct implementation would delay
  //   the kill or re-materialise. For now we handle only the small-frame case.

  if (hasFP(MF)) {
    if (isInt<12>(static_cast<int64_t>(StackSize))) {
      // Small frame: ADDI s0, sp, StackSize
      BuildMI(MBB, MBBI, DL, TII->get(RVX::ADDI), RVX::X8 /*s0/fp*/)
          .addReg(RVX::X2)                      // source: sp (after adjustment)
          .addImm(static_cast<int64_t>(StackSize)); // adds back frameSize → fp = old sp

      // CFI: now that fp is set up, the CFA can be expressed relative to fp.
      // .cfi_def_cfa s0, 0 — CFA = fp + 0 = fp (the frame pointer IS the CFA)
      // From this point on, the unwinder uses fp (not sp) as the frame anchor.
      unsigned DwarfFP = TRI->getDwarfRegNum(RVX::X8, true);
      unsigned CFIIndex = MF.addFrameInst(
          MCCFIInstruction::cfiDefCfa(nullptr, DwarfFP, 0));
      BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex);
    } else {
      // Large frame with fp: would need a scratch register here.
      // For a complete implementation: re-materialise StackSize in t0,
      // then ADD s0, sp, t0. Left as a TODO.
      report_fatal_error("RVXFrameLowering: large frame with frame pointer "
                         "not yet implemented. Frame size: " +
                         Twine(StackSize));
    }
  }
}

// ============================================================================
// emitEpilogue
// ============================================================================
// Inserts the function epilogue BEFORE the return instruction in MBB.
// This is the REVERSE of emitPrologue: we restore sp to its entry value.
//
// By the time emitEpilogue is called, PEI has already inserted the
// callee-save RESTORE instructions (loadRegFromStackSlot). These restores
// use the frame-adjusted sp (sp as it is in the function body). After our
// ADDI sp, sp, +StackSize, sp will be back to its original value.
void RVXFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const RVXInstrInfo *TII =
      static_cast<const RVXInstrInfo *>(STI.getInstrInfo());
  DebugLoc DL = MBBI->getDebugLoc();

  // ---- Compute frame size (same as in emitPrologue) ----------------------
  uint64_t StackSize = MFI.getStackSize();

  // If there's no frame, nothing to restore.
  if (StackSize == 0 && !hasFP(MF))
    return;

  // ---- Restore sp ---------------------------------------------------------
  // The epilogue must exactly REVERSE the prologue's sp adjustment so that
  // when we execute JALR x0, ra, 0 (the return), sp has its original value
  // and the caller sees a clean stack.
  //
  // If we used a frame pointer, we COULD restore sp from fp:
  //   ADDI sp, s0, -StackSize (or just ADDI sp, s0, 0 if fp = old sp)
  // But the simpler approach (and what most RISC-V backends do) is to
  // just add StackSize back to sp directly, since the frame layout is fixed.

  if (isInt<12>(static_cast<int64_t>(StackSize))) {
    // ---- CASE A: Small frame — single ADDI ---------------------------------
    //
    // ADDI sp, sp, +StackSize
    //
    // Insert BEFORE the return instruction (MBBI points to the return).
    // After this, sp = original sp at function entry = CFA.
    //
    // CFI note: we don't strictly need to emit CFI in the epilogue for
    // correctness (unwinders only need to unwind from live frames, not from
    // code that's about to return). But emitting .cfi_def_cfa_offset 0 helps
    // debuggers that inspect the epilogue.

    BuildMI(MBB, MBBI, DL, TII->get(RVX::ADDI), RVX::X2 /*sp*/)
        .addReg(RVX::X2)                      // source: sp
        .addImm(static_cast<int64_t>(StackSize)); // add back the frame size

  } else {
    // ---- CASE B: Large frame — LUI + ADDI + ADD ----------------------------
    //
    // Materialise StackSize in t0, then ADD sp, sp, t0.
    //
    // In the epilogue we use ADD (not SUB) because we're adding back:
    //   sp = sp + StackSize

    int64_t Hi = ((int64_t)StackSize + 0x800) >> 12;
    int64_t Lo = (int64_t)StackSize - (Hi << 12);

    // LUI t0, Hi
    BuildMI(MBB, MBBI, DL, TII->get(RVX::LUI), RVX::X5 /*t0*/)
        .addImm(Hi);

    // ADDI t0, t0, Lo — t0 = full StackSize
    BuildMI(MBB, MBBI, DL, TII->get(RVX::ADDI), RVX::X5 /*t0*/)
        .addReg(RVX::X5, RegState::Kill)
        .addImm(Lo);

    // ADD sp, sp, t0 — sp += StackSize (restores to original value)
    BuildMI(MBB, MBBI, DL, TII->get(RVX::ADD), RVX::X2 /*sp*/)
        .addReg(RVX::X2)
        .addReg(RVX::X5, RegState::Kill);
  }

  // ---- CFI for epilogue restores ------------------------------------------
  // Emit .cfi_restore for each callee-saved register to signal that they are
  // now back in their original locations (no longer saved on the stack).
  // This is used by debuggers inspecting the epilogue region.
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  const RVXRegisterInfo *TRI =
      static_cast<const RVXRegisterInfo *>(STI.getRegisterInfo());

  for (const CalleeSavedInfo &CS : CSI) {
    unsigned DwarfReg = TRI->getDwarfRegNum(CS.getReg(), true /*isEH*/);
    // .cfi_restore reg — "reg is back in its original location"
    unsigned CFIIndex =
        MF.addFrameInst(MCCFIInstruction::createRestore(nullptr, DwarfReg));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  // ---- CFI: CFA is back to sp + 0 ----------------------------------------
  // After the sp restore, the CFA = sp + 0 (sp IS the CFA again).
  // .cfi_def_cfa_offset 0 signals this to debuggers.
  unsigned CFIIndex =
      MF.addFrameInst(MCCFIInstruction::cfiDefCfaOffset(nullptr, 0));
  BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
      .addCFIIndex(CFIIndex);
}

// ============================================================================
// getFrameIndexReference
// ============================================================================
// Returns the base register and byte offset for accessing frame index FI
// from within the function body (after the prologue has run).
//
// Called by RegisterInfo::eliminateFrameIndex() to know what (base, offset)
// pair to substitute for a frame index pseudo-operand.
StackOffset
RVXFrameLowering::getFrameIndexReference(const MachineFunction &MF,
                                          int FI,
                                          Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // MFI.getObjectOffset(FI) returns the offset of the stack slot relative
  // to the frame base (the value of sp BEFORE the prologue adjustment).
  // This is a NEGATIVE number for most slots (they are below the entry sp).
  //
  // We need to translate this to an offset relative to either sp or fp
  // AS THEY EXIST IN THE FUNCTION BODY (after the prologue has run).

  if (hasFP(MF)) {
    // ---- Frame pointer mode -----------------------------------------------
    // fp (s0) was set to the ORIGINAL sp (before the prologue's subtraction).
    // So fp = CFA = the value sp had at function entry.
    //
    // MFI.getObjectOffset(FI) is relative to the CFA (= fp in this case).
    // Therefore: address of slot = fp + MFI.getObjectOffset(FI)
    //
    // The offset is typically negative (slots are below fp).
    // Example: if FI is the 8-byte slot for ra, saved at CFA-8, then
    //   MFI.getObjectOffset(FI) = -8
    //   address = fp + (-8) = fp - 8  ✓ (ra was saved just below fp)
    FrameReg = RVX::X8; // fp = s0
    return StackOffset::getFixed(MFI.getObjectOffset(FI));

  } else {
    // ---- Stack pointer mode -----------------------------------------------
    // sp in the function body = (entry sp - StackSize)
    //   i.e. sp was moved DOWN by StackSize bytes in the prologue.
    //
    // MFI.getObjectOffset(FI) is relative to CFA (= entry sp).
    // To get the offset from the CURRENT sp (= entry sp - StackSize):
    //   offset from sp = MFI.getObjectOffset(FI) + StackSize
    //
    // Example:
    //   StackSize = 32 bytes (prologue did ADDI sp, sp, -32)
    //   FI for ra was saved at CFA - 8 → MFI.getObjectOffset = -8
    //   Offset from current sp = -8 + 32 = 24
    //   Address = sp + 24  ✓ (sp+24 = entry_sp - 32 + 24 = entry_sp - 8 = CFA - 8)
    FrameReg = RVX::X2; // sp
    return StackOffset::getFixed(MFI.getObjectOffset(FI) +
                                 (int64_t)MFI.getStackSize());
  }
}
