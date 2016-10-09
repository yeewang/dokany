#ifndef _FUSE_DEBUG_H_
#define _FUSE_DEBUG_H_

#include "dokan.h"

#if 1 // def DEBUG

#if defined(__GNUC__)
#define FPRINTF(f, args...)                                                    \
  do {                                                                         \
    fprintf(f, args);                                                          \
    fflush(f);                                                                 \
  } while (0)
#else
#define FPRINTF fprintf
#endif

#else

#define FPRINTF(...)

#endif

impl_fuse_context *FuseContext(PDOKAN_FILE_INFO DokanFileInfo);

#endif /* _FUSE_DEBUG_H_ */

