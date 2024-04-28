/* -------------------------------------------------------------------------
 * Configures prerequisite for this library
 * ------------------------------------------------------------------------- */

#ifndef IK_CONFIG_H
    #define IK_CONFIG_H

    /* ---------------------------------------------------------------------
     * Build settings
     * --------------------------------------------------------------------- */

#   define IKAPI ik
/* #undef IK_BENCHMARKS */
/* #undef IK_DOT_EXPORT */
    #define IK_HAVE_STDINT_H
/* #undef IK_MEMORY_DEBUGGING */
#   ifdef IK_MEMORY_DEBUGGING
/* #undef IK_MEMORY_BACKTRACE */
#   endif
#   define IK_PRECISION_FLOAT
/* #undef IK_PIC */
/* #undef IK_PROFILING */
/* #undef IK_PYTHON */
/* #undef IK_TESTS */

    /* ---------------------------------------------------------------------
     * Helpers
     * --------------------------------------------------------------------- */

    /* Helpful for making sure functions are being used */
#   define IK_WARN_UNUSED 

    /* Visibility macros */
#   define IK_HELPER_API_EXPORT 
#   define IK_HELPER_API_IMPORT 
#   define IK_HELPER_API_LOCAL  
#   if defined (IK_BUILDING)  /* defined only when the library is being compiled using a command line option, e.g. -DIK_BUILDING */
#       define IK_PUBLIC_API IK_HELPER_API_EXPORT
#   else
#       define IK_PUBLIC_API IK_HELPER_API_IMPORT
#   endif
#   define IK_PRIVATE_API IK_HELPER_API_LOCAL

    /* C linkage */
#   ifdef __cplusplus
#       define C_BEGIN extern "C" {
#       define C_END }
#   else
#       define C_BEGIN
#       define C_END
#   endif

    /* The "real" datatype to be used throughout the library */
typedef float ikreal_t;

    /* Define epsilon depending on the type of "real" */
#   include <float.h>
#   if defined(IK_PRECISION_LONG_DOUBLE)
#       define IK_EPSILON DBL_EPSILON
#   elif defined(IK_PRECISION_DOUBLE)
#       define IK_EPSILON DBL_EPSILON
#   elif defined(IK_PRECISION_FLOAT)
#       define IK_EPSILON FLT_EPSILON
#   else
#       error Unknown precision. Are you sure you defined IK_PRECISION and IK_PRECISION_CAPS_AND_NO_SPACES?
#   endif

    /* Dummy defines that are used to mark things to the interface generator script */
    #define IK_INTERFACE(i) struct ik_##i##_t

    /* --------------------------------------------------------------
     * Common include files
     * --------------------------------------------------------------*/

    #include "ik/retcodes.h"

    #ifdef IK_HAVE_STDINT_H
        #include <stdint.h>
    #else
        #include "ik/pstdint.h"
    #endif


#endif /* IK_CONFIG_H */
