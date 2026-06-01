

//scalar process scheduling model 
//
def RVXSSchedMOdel : SchedModel {
    
    let IssuwWidth  1; 
    let MicroOpBufferSize = 0; 
    let LoadLatency = 2; 
    let Mispredicatabilty = 3; 
    let CompleteModel = 0; 
    let PostRAScheduler = 1; 
}

def RVXALUPipe : ProcResource<1> {
    let BufferSize =1; 
}

def RVXMemPipe : ProcResource<1>{
    let BufferSize = 1;
}

def RVXBrnachPipe : ProcResource<1>{
    let BufferSize = 1; 
}


def WriteALU    : SchedWrite: 
def WriteALUI   : SchedWrite;
def WriteUI     : SchedWrite;
def WriteShift  : SchedWrite; 
def WriteLD     : SchedWrite;
def WriteST     : SchedWrite; 
def WriteBr     : SchedWrite; 
def WriteJmp    : SchedWrite; 
def WriteNop    : SchedWrite; 


def ReadALU     : SchedRead;
def ReadALUI    : SchedRead;
def ReadLD      : SchedRead;
def ReadST      : SchedRead;
def ReadJmp     : SchedRead;
def ReadBr      : SchedRead;


def : WriteRes<WriteALU, [RVXALUPipe]>{
    let Latency = 1; 
    let ResourceCycles = [1]; 
}

def : WriteRes<WriteALUI, [RVXALUPipe]>{
    let Latency = 1; 
    let ResourceCycles = [1]; 
}

def : WriteRes<WriteUI, [RVXIntPipe]> {
    let Latency       = 1;
    let ResourceCycles = [1];
}

def : WriteRes<WriteShift, [RVXIntPipe]> {
    let Latency       = 1;
    let ResourceCycles = [1];
}

//cache hit 
def : WriteRes<WriteLD, [RVXMemPipe]> {
    let Latency       = 2;
    let ResourceCycles = [2];
}

def : WriteRes<WriteST, [RVXMemPipe]> {
    let Latency       = 1;
    let ResourceCycles = [2];
}

def : WriteRes<WriteBr, [RVXBranchPipe]> {
    let Latency       = 1;
    let ResourceCycles = [1];
}

def : WriteRes<WriteJmp, [RVXBranchPipe]> {
    let Latency       = 1;
    let ResourceCycles = [1];
}

def : WriteRes<WriteNop, []> {
    let Latency       = 0;
    let ResourceCycles = [];
}

//value to dependent instructions are only available at WB
//0 cycles before WB 
def : ReadAdvance<ReadALU,  0>;
def : ReadAdvance<ReadALUI, 0>;
def : ReadAdvance<ReadLD,   0>;
def : ReadAdvance<ReadST,   0>;
def : ReadAdvance<ReadJmp,  0>;
def : ReadAdvance<ReadBr,   0>;



