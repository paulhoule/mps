/* policy.c: POLICY DECISIONS
 *
 * $Id$
 * Copyright (c) 2001-2015 Ravenbrook Limited.  See end of file for license.
 *
 * This module collects the decision-making code for the MPS, so that
 * policy can be maintained and adjusted.
 *
 * .sources: <design/strategy/>.
 */

#include "mpm.h"

SRCID(policy, "$Id$");


/* PolicyAlloc -- allocation policy
 *
 * This is the code responsible for making decisions about where to allocate
 * memory.
 */

Res PolicyAlloc(Tract *tractReturn, Arena arena, LocusPref pref,
                Size size, Pool pool)
{
  Res res;
  Tract tract;
  ZoneSet zones, moreZones, evenMoreZones;

  AVER(tractReturn != NULL);
  AVERT(Arena, arena);
  AVERT(LocusPref, pref);
  AVER(size > (Size)0);
  AVERT(Pool, pool);
  AVER(arena == PoolArena(pool));

  /* Don't attempt to allocate if doing so would definitely exceed the
   * commit limit. */
  if (arena->spareCommitted < size) {
    Size necessaryCommitIncrease = size - arena->spareCommitted;
    if (arena->committed + necessaryCommitIncrease > arena->commitLimit
        || arena->committed + necessaryCommitIncrease < arena->committed) {
      return ResCOMMIT_LIMIT;
    }
  }

  /* Plan A: allocate from the free Land in the requested zones */
  zones = ZoneSetDiff(pref->zones, pref->avoid);
  if (zones != ZoneSetEMPTY) {
    res = ArenaFreeLandAlloc(&tract, arena, zones, pref->high, size, pool);
    if (res == ResOK)
      goto found;
  }

  /* Plan B: add free zones that aren't blacklisted */
  /* TODO: Pools without ambiguous roots might not care about the blacklist. */
  /* TODO: zones are precious and (currently) never deallocated, so we
   * should consider extending the arena first if address space is plentiful.
   * See also job003384. */
  moreZones = ZoneSetUnion(pref->zones, ZoneSetDiff(arena->freeZones, pref->avoid));
  if (moreZones != zones) {
    res = ArenaFreeLandAlloc(&tract, arena, moreZones, pref->high, size, pool);
    if (res == ResOK)
      goto found;
  }

  /* Plan C: Extend the arena, then try A and B again. */
  if (moreZones != ZoneSetEMPTY) {
    res = arena->class->grow(arena, pref, size);
    if (res != ResOK)
      return res;
    if (zones != ZoneSetEMPTY) {
      res = ArenaFreeLandAlloc(&tract, arena, zones, pref->high, size, pool);
      if (res == ResOK)
        goto found;
    }
    if (moreZones != zones) {
      zones = ZoneSetUnion(zones, ZoneSetDiff(arena->freeZones, pref->avoid));
      res = ArenaFreeLandAlloc(&tract, arena, moreZones, pref->high,
                               size, pool);
      if (res == ResOK)
        goto found;
    }
  }

  /* Plan D: add every zone that isn't blacklisted.  This might mix GC'd
   * objects with those from other generations, causing the zone check
   * to give false positives and slowing down the collector. */
  /* TODO: log an event for this */
  evenMoreZones = ZoneSetDiff(ZoneSetUNIV, pref->avoid);
  if (evenMoreZones != moreZones) {
    res = ArenaFreeLandAlloc(&tract, arena, evenMoreZones, pref->high,
                             size, pool);
    if (res == ResOK)
      goto found;
  }

  /* Last resort: try anywhere.  This might put GC'd objects in zones where
   * common ambiguous bit patterns pin them down, causing the zone check
   * to give even more false positives permanently, and possibly retaining
   * garbage indefinitely. */
  res = ArenaFreeLandAlloc(&tract, arena, ZoneSetUNIV, pref->high, size, pool);
  if (res == ResOK)
    goto found;

  /* Uh oh. */
  return res;

found:
  *tractReturn = tract;
  return ResOK;
}


/* PolicyStartTrace -- consider starting a trace
 *
 * If a trace was started, update *traceReturn and return TRUE.
 * Otherwise, leave *traceReturn unchanged and return FALSE.
 */

Bool PolicyStartTrace(Trace *traceReturn, Arena arena)
{
  Res res;
  Trace trace;
  Size sFoundation, sCondemned, sSurvivors, sConsTrace;
  double tTracePerScan; /* tTrace/cScan */
  double dynamicDeferral;

  /* Compute dynamic criterion.  See strategy.lisp-machine. */
  AVER(arena->topGen.mortality >= 0.0);
  AVER(arena->topGen.mortality <= 1.0);
  sFoundation = (Size)0; /* condemning everything, only roots @@@@ */
  /* @@@@ sCondemned should be scannable only */
  sCondemned = ArenaCommitted(arena) - ArenaSpareCommitted(arena);
  sSurvivors = (Size)(sCondemned * (1 - arena->topGen.mortality));
  tTracePerScan = sFoundation + (sSurvivors * (1 + TraceCopyScanRATIO));
  AVER(TraceWorkFactor >= 0);
  AVER(sSurvivors + tTracePerScan * TraceWorkFactor <= (double)SizeMAX);
  sConsTrace = (Size)(sSurvivors + tTracePerScan * TraceWorkFactor);
  dynamicDeferral = (double)ArenaAvail(arena) - (double)sConsTrace;

  if (dynamicDeferral < 0.0) {
    /* Start full collection. */
    res = TraceStartCollectAll(&trace, arena, TraceStartWhyDYNAMICCRITERION);
    if (res != ResOK)
      goto failStart;
    *traceReturn = trace;
    return TRUE;
  } else {
    /* Find the chain most over its capacity. */
    Ring node, nextNode;
    double firstTime = 0.0;
    Chain firstChain = NULL;

    RING_FOR(node, &arena->chainRing, nextNode) {
      Chain chain = RING_ELT(Chain, chainRing, node);
      double time;

      AVERT(Chain, chain);
      time = ChainDeferral(chain);
      if (time < firstTime) {
        firstTime = time; firstChain = chain;
      }
    }

    /* If one was found, start collection on that chain. */
    if(firstTime < 0) {
      double mortality;

      res = TraceCreate(&trace, arena, TraceStartWhyCHAIN_GEN0CAP);
      AVER(res == ResOK);
      res = ChainCondemnAuto(&mortality, firstChain, trace);
      if (res != ResOK) /* should try some other trace, really @@@@ */
        goto failCondemn;
      trace->chain = firstChain;
      ChainStartGC(firstChain, trace);
      res = TraceStart(trace, mortality, trace->condemned * TraceWorkFactor);
      /* We don't expect normal GC traces to fail to start. */
      AVER(res == ResOK);
      *traceReturn = trace;
      return TRUE;
    }
  } /* (dynamicDeferral > 0.0) */
  return FALSE;

failCondemn:
  TraceDestroy(trace);
  /* This is an unlikely case, but clear the emergency flag so the next attempt
     starts normally. */
  ArenaSetEmergency(arena, FALSE);
failStart:
  return FALSE;
}


/* C. COPYRIGHT AND LICENSE
 *
 * Copyright (C) 2001-2015 Ravenbrook Limited <http://www.ravenbrook.com/>.
 * All rights reserved.  This is an open source license.  Contact
 * Ravenbrook for commercial licensing options.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Redistributions in any form must be accompanied by information on how
 * to obtain complete source code for this software and any accompanying
 * software that uses this software.  The source code must either be
 * included in the distribution or be available for no more than the cost
 * of distribution plus a nominal fee, and must be freely redistributable
 * under reasonable conditions.  For an executable file, complete source
 * code means the source code for all modules it contains. It does not
 * include source code for modules or files that typically accompany the
 * major components of the operating system on which the executable file
 * runs.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */