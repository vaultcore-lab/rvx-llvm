#if !defined RVXTARGETINFO_H_INCLUDED
#define RVXTARGETINFO_H_INCLUDED

namespace llvm{

class Target; 

Target &getTheRVX32Target(); 
Target &getTheRVX64Target(); 

}

#endif 
