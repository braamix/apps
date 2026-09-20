/*
                            __  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 2002-2003 Fred L. Drake, Jr. <fdrake@users.sourceforge.net>
   Copyright (c) 2002-2006 Karl Waclawek <karl@waclawek.net>
   Copyright (c) 2003      Greg Stein <gstein@users.sourceforge.net>
   Copyright (c) 2016-2026 Sebastian Pipping <sebastian@pipping.org>
   Copyright (c) 2018      Yury Gribov <tetra2005@gmail.com>
   Copyright (c) 2019      David Loffredo <loffredo@steptools.com>
   Copyright (c) 2023-2024 Sony Corporation / Snild Dolkow <snild@sony.com>
   Copyright (c) 2024      Taichi Haradaguchi <20001722@ymail.ne.jp>
   Licensed under the MIT license:

   Permission is  hereby granted,  free of charge,  to any  person obtaining
   a  copy  of  this  software   and  associated  documentation  files  (the
   "Software"),  to  deal in  the  Software  without restriction,  including
   without  limitation the  rights  to use,  copy,  modify, merge,  publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit
   persons  to whom  the Software  is  furnished to  do so,  subject to  the
   following conditions:

   The above copyright  notice and this permission notice  shall be included
   in all copies or substantial portions of the Software.

   THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
   EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
   NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.

   SPDX-License-Identifier: MIT
*/

#pragma once

#include "expat.h"
#include "expat_config.h"

/* NOTE BEGIN If you ever patch these defaults to greater values
              for non-attack XML payload in your environment,
              please file a bug report with libexpat.  Thank you!
*/
#define EXPAT_BILLION_LAUGHS_ATTACK_PROTECTION_MAXIMUM_AMPLIFICATION_DEFAULT 100.0f
#define EXPAT_BILLION_LAUGHS_ATTACK_PROTECTION_ACTIVATION_THRESHOLD_DEFAULT  8388608 // 8 MiB, 2^23
#define EXPAT_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION_DEFAULT                    100.0f
#define EXPAT_ALLOC_TRACKER_ACTIVATION_THRESHOLD_DEFAULT 67108864 // 64 MiB, 2^26
/* NOTE END */

#define EXPAT_MALLOC_ALIGNMENT sizeof(long long) // largest parser (sub)member
#define EXPAT_MALLOC_PADDING   ((EXPAT_MALLOC_ALIGNMENT) - sizeof(usize))

void _INTERNAL_trim_to_complete_utf8_characters(const char *from, const char **fromLimRef);

// Whether a parser defers reparsing a partial token by default. Upstream lets
// its own test suite write this; here it is a constant, and the module's
// SetReparseDeferralEnabled moves it per parser.
extern const XML_Bool g_reparseDeferralEnabledDefault;
