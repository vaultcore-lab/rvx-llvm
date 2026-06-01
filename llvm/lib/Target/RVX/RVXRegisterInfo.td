include "RVXInstrFormats.td"

let Namespace = "RVX" in {
    
    class RVXReg<bits<5> Enc, string n, list<string> alt = []>
        : Register<n> {
        
        let HWEncoding{4-0} = Enc; 
        let AltNames = alt; 
    }

    class RVXGPRREG<bits<5> Enc, string n, list<string> alt>
        : RVXReg<Enc, N, alt>; 
} 

let Namespace = "RVX" in {
    def ZERO : RVXGPRREG<0,  "x0",  ["zero"]>,  DwarfRegNum<[0]>;
    def RA   : RVXGPRREG<1,  "x1",  ["ra"]>,    DwarfRegNum<[1]>;
    def SP   : RVXGPRREG<2,  "x2",  ["sp"]>,    DwarfRegNum<[2]>;
    def GP   : RVXGPRREG<3,  "x3",  ["gp"]>,    DwarfRegNum<[3]>;
    def TP   : RVXGPRREG<4,  "x4",  ["tp"]>,    DwarfRegNum<[4]>;
    def T0   : RVXGPRREG<5,  "x5",  ["t0"]>,    DwarfRegNum<[5]>;
    def T1   : RVXGPRREG<6,  "x6",  ["t1"]>,    DwarfRegNum<[6]>;
    def T2   : RVXGPRREG<7,  "x7",  ["t2"]>,    DwarfRegNum<[7]>;
    def FP   : RVXGPRREG<8,  "x8",  ["s0","fp"]>, DwarfRegNum<[8]>;
    def S1   : RVXGPRREG<9,  "x9",  ["s1"]>,    DwarfRegNum<[9]>;
    def A0   : RVXGPRREG<10, "x10", ["a0"]>,    DwarfRegNum<[10]>;
    def A1   : RVXGPRREG<11, "x11", ["a1"]>,    DwarfRegNum<[11]>;
    def A2   : RVXGPRREG<12, "x12", ["a2"]>,    DwarfRegNum<[12]>;
    def A3   : RVXGPRREG<13, "x13", ["a3"]>,    DwarfRegNum<[13]>;
    def A4   : RVXGPRREG<14, "x14", ["a4"]>,    DwarfRegNum<[14]>;
    def A5   : RVXGPRREG<15, "x15", ["a5"]>,    DwarfRegNum<[15]>;
    def A6   : RVXGPRREG<16, "x16", ["a6"]>,    DwarfRegNum<[16]>;
    def A7   : RVXGPRREG<17, "x17", ["a7"]>,    DwarfRegNum<[17]>;
    def S2   : RVXGPRREG<18, "x18", ["s2"]>,    DwarfRegNum<[18]>;
    def S3   : RVXGPRREG<19, "x19", ["s3"]>,    DwarfRegNum<[19]>;
    def S4   : RVXGPRREG<20, "x20", ["s4"]>,    DwarfRegNum<[20]>;
    def S5   : RVXGPRREG<21, "x21", ["s5"]>,    DwarfRegNum<[21]>;
    def S6   : RVXGPRREG<22, "x22", ["s6"]>,    DwarfRegNum<[22]>;
    def S7   : RVXGPRREG<23, "x23", ["s7"]>,    DwarfRegNum<[23]>;
    def S8   : RVXGPRREG<24, "x24", ["s8"]>,    DwarfRegNum<[24]>;
    def S9   : RVXGPRREG<25, "x25", ["s9"]>,    DwarfRegNum<[25]>;
    def S10  : RVXGPRREG<26, "x26", ["s10"]>,   DwarfRegNum<[26]>;
    def S11  : RVXGPRREG<27, "x27", ["s11"]>,   DwarfRegNum<[27]>;
    def T3   : RVXGPRREG<28, "x28", ["t3"]>,    DwarfRegNum<[28]>;
    def T4   : RVXGPRREG<29, "x29", ["t4"]>,    DwarfRegNum<[29]>;
    def T5   : RVXGPRREG<30, "x30", ["t5"]>,    DwarfRegNum<[30]>;
    def T6   : RVXGPRREG<31, "x31", ["t6"]>,    DwarfRegNum<[31]>;
}

def XLenVT : ValuetTypeByMode< 
    [RV32, RV64, DefaultMode], 
    [i32, i64, i32]
>;

def GPR : RegisterClass<"RVX", [XLenVT], 32, (add
    ZERO,
    T0, T1, T2, T3, T4, T5, T6,                     
    FP, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, 
    RA, SP, GP, TP                              
  )> {

    let RegInfos = RegInfoByHwMode<
      [RV32,              RV64,              DefaultMode],
      [RegInfo<32,32,32>, RegInfo<64,64,64>, RegInfo<32,32,32>]
    >;
}v
