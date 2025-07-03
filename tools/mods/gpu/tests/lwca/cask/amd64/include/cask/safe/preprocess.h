#ifndef SAFE_INCLUDE_GUARD_CASK_API_PREPROCESS_H
#define SAFE_INCLUDE_GUARD_CASK_API_PREPROCESS_H

#ifdef CASK_NAMESPACE
#define concat_tok(a, b) a ## b
#define mkcasknamespace(pre, ns) concat_tok(pre, ns)
#ifndef cask_safe
#define cask_safe mkcasknamespace(cask_safe_, CASK_NAMESPACE)
#endif
#endif

#endif // SAFE_INCLUDE_GUARD_CASK_API_PREPROCESS_H
