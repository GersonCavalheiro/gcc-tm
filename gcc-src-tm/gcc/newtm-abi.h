/* Compiler-side STM v1 transport. See ../include/newtm-abi.h.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_COMPILER_ABI_H
#define GCC_NEWTM_COMPILER_ABI_H
#define NEWTM_ABI_COMPILER
#include "../include/newtm-abi.h"
#undef NEWTM_ABI_COMPILER
/* Context is a pointer to an opaque, incomplete record, matching the shared
   C declaration; SLOT_TYPE is a pointer to that context value. Allocate the
   owning slot with the context value type, not a void-pointer object accessed
   via an incompatible pointer-to-pointer type. Builtins erase pointer types
   only for transport. Bindings have native target field layout. */
extern tree newtm_abi_context_type ();
extern tree newtm_abi_slot_type ();
extern tree newtm_abi_binding_type ();
extern tree newtm_abi_clone_binding_type ();
/* Adopt remapped signature types only after checking the shared ABI layout. */
extern bool newtm_abi_load_types (tree, tree);
/* Caller supplies evaluated GIMPLE operands, explicit status destination and
   control flow (NULL status only for the noreturn FATAL service). This builder
   never consults expiring occurrence/descriptor
   tables and never reconstructs expressions from diagnostic recipes.
   Resolve incoming/result channels in clones; reject unknown/source-only/stale
   or reconstruct after remapping BEFORE calling this interface. */
extern gcall *newtm_abi_build_call (built_in_function, tree, unsigned, tree *);
/* Read-only ABI audit of the actual lowered/loaded CFG. Does not certify
   storage provenance, status flow or the complete transaction graph. */
extern bool newtm_abi_verify_cfg (function *);
#endif
