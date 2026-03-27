#include "import/output/_.out"

#if defined(OUTPUT_NULL)
#include "import/output/null.null"
#define OUT       null
#define OUT_STATE null_Null
#else
#include "import/output/stdout.std"
#define OUT       std
#define OUT_STATE std_Stdout
#endif
