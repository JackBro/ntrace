#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <crtdbg.h>

#define ASSERT _ASSERTE
#ifndef VERIFY
#if defined(DBG) || defined( DBG )
#define VERIFY ASSERT
#else
#define VERIFY( x ) ( x )
#endif
#endif