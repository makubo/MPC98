#ifndef MPC98_RPC_COMPAT_H
#define MPC98_RPC_COMPAT_H

// Older Platform SDKs do not provide MIDL RPC annotation macros.
#ifndef __RPC__in
#define __RPC__in
#endif
#ifndef __RPC__in_opt
#define __RPC__in_opt
#endif
#ifndef __RPC__inout
#define __RPC__inout
#endif
#ifndef __RPC__out
#define __RPC__out
#endif
#ifndef __RPC__deref_out
#define __RPC__deref_out
#endif
#ifndef __RPC__deref_out_opt
#define __RPC__deref_out_opt
#endif
#ifndef __RPC__deref_in_opt
#define __RPC__deref_in_opt
#endif
#ifndef __RPC__in_ecount_full
#define __RPC__in_ecount_full(size)
#endif
#ifndef __RPC__out_ecount_full
#define __RPC__out_ecount_full(size)
#endif

#endif
