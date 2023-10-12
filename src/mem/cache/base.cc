/*
 * Copyright (c) 2012-2013, 2018-2019 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Definition of BaseCache functions.
 */
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <tuple>
#include <iostream>
#include <fstream>  // Required for file handling

#include "mem/cache/base.hh"

#include "base/compiler.hh"
#include "base/logging.hh"
#include "debug/Cache.hh"
#include "debug/CacheComp.hh"
#include "debug/CachePort.hh"
#include "debug/CacheRepl.hh"
#include "debug/CacheVerbose.hh"
#include "debug/HWPrefetch.hh"
#include "mem/cache/compressors/base.hh"
#include "mem/cache/mshr.hh"
#include "mem/cache/prefetch/base.hh"
#include "mem/cache/queue_entry.hh"
#include "mem/cache/tags/compressed_tags.hh"
#include "mem/cache/tags/super_blk.hh"
#include "params/BaseCache.hh"
#include "params/WriteAllocator.hh"
#include "sim/cur_tick.hh"

namespace gem5
{

BaseCache::CacheResponsePort::CacheResponsePort(const std::string &_name,
                                          BaseCache *_cache,
                                          const std::string &_label)
    : QueuedResponsePort(_name, _cache, queue),
      queue(*_cache, *this, true, _label),
      blocked(false), mustSendRetry(false),
      sendRetryEvent([this]{ processSendRetry(); }, _name)
{
}

BaseCache::BaseCache(const BaseCacheParams &p, unsigned blk_size)
    : ClockedObject(p),
      cpuSidePort (p.name + ".cpu_side_port", this, "CpuSidePort"),
      memSidePort(p.name + ".mem_side_port", this, "MemSidePort"),
      mshrQueue("MSHRs", p.mshrs, 0, p.demand_mshr_reserve, p.name),
      writeBuffer("write buffer", p.write_buffers, p.mshrs, p.name),
      tags(p.tags),
      compressor(p.compressor),
      prefetcher(p.prefetcher),
      writeAllocator(p.write_allocator),
      writebackClean(p.writeback_clean),
      tempBlockWriteback(nullptr),
      writebackTempBlockAtomicEvent([this]{ writebackTempBlockAtomic(); },
                                    name(), false,
                                    EventBase::Delayed_Writeback_Pri),
      blkSize(blk_size),
      lookupLatency(p.tag_latency),
      dataLatency(p.data_latency),
      forwardLatency(p.tag_latency),
      fillLatency(p.data_latency),
      responseLatency(p.response_latency),
      sequentialAccess(p.sequential_access),
      numTarget(p.tgts_per_mshr),
      forwardSnoops(true),
      clusivity(p.clusivity),
      isReadOnly(p.is_read_only),
      replaceExpansions(p.replace_expansions),
      moveContractions(p.move_contractions),
      blocked(0),
      order(0),
      noTargetMSHR(nullptr),
      missCount(p.max_miss_count),
      addrRanges(p.addr_ranges.begin(), p.addr_ranges.end()),
      system(p.system),
      is_l1_cache_locking(p.is_l1_cache_locking),
      is_l1_cache_locking_full_context(p.is_l1_cache_locking_full_context),
      stats(*this)
{
    // the MSHR queue has no reserve entries as we check the MSHR
    // queue on every single allocation, whereas the write queue has
    // as many reserve entries as we have MSHRs, since every MSHR may
    // eventually require a writeback, and we do not check the write
    // buffer before committing to an MSHR

    // forward snoops is overridden in init() once we can query
    // whether the connected requestor is actually snooping or not
    if (is_l1_cache_locking_full_context)
        {

        
        std::ifstream inputFile;
        inputFile.open("full_context_locking.txt");

        std::string line;

    if (!inputFile) {
            std::cerr << "Error opening the file." << std::endl;
        }

while (std::getline(inputFile, line)) {
        int key, lastValue;
        std::vector<int> addresses;

        std::stringstream ss(line);
        ss >> key;

        char discard;
        ss >> discard;  // Discard the comma

        ss >> lastValue;

        char openBrace;
        ss >> openBrace;  // Read the opening brace

        std::string hexNum;
        std::cout<<"Addresses are: ";
        while (std::getline(ss, hexNum, ',')) {
            if (hexNum.find('}') != std::string::npos) {
                hexNum.erase(hexNum.find('}'));
            }
            if (!hexNum.empty()) {
                addresses.push_back(std::stoi(hexNum, nullptr, 16));
                std::cout<<"0x"<<std::hex<<std::stoi(hexNum, nullptr, 16)<<",";
            }
            
        }
        std::cout<<"\n";

     // Extract the last value after the curly brace and comma


        interval_locked_map[key] = std::make_tuple(addresses, lastValue);
        //std::cout<<key<<","<<lastValue<<std::endl;
    }

    inputFile.close();



        inputFile.close();
        printf("Done processing text file!\n");

    
        }

    tempBlock = new TempCacheBlk(blkSize);

    tags->tagsInit();
    if (prefetcher)
        prefetcher->setCache(this);

    fatal_if(compressor && !dynamic_cast<CompressedTags*>(tags),
        "The tags of compressed cache %s must derive from CompressedTags",
        name());
    warn_if(!compressor && dynamic_cast<CompressedTags*>(tags),
        "Compressed cache %s does not have a compression algorithm", name());
    if (compressor)
        compressor->setCache(this);

    if(is_l1_cache_locking)
        printf("IS L1 CACHE LOCKING!");
}

BaseCache::~BaseCache()
{
    delete tempBlock;
}

void
BaseCache::CacheResponsePort::setBlocked()
{
    assert(!blocked);
    DPRINTF(CachePort, "Port is blocking new requests\n");
    blocked = true;
    // if we already scheduled a retry in this cycle, but it has not yet
    // happened, cancel it
    if (sendRetryEvent.scheduled()) {
        owner.deschedule(sendRetryEvent);
        DPRINTF(CachePort, "Port descheduled retry\n");
        mustSendRetry = true;
    }
}

void
BaseCache::CacheResponsePort::clearBlocked()
{
    assert(blocked);
    DPRINTF(CachePort, "Port is accepting new requests\n");
    blocked = false;
    if (mustSendRetry) {
        // @TODO: need to find a better time (next cycle?)
        owner.schedule(sendRetryEvent, curTick() + 1);
    }
}

void
BaseCache::CacheResponsePort::processSendRetry()
{
    DPRINTF(CachePort, "Port is sending retry\n");

    // reset the flag and call retry
    mustSendRetry = false;
    sendRetryReq();
}

Addr
BaseCache::regenerateBlkAddr(CacheBlk* blk)
{
    if (blk != tempBlock) {
        return tags->regenerateBlkAddr(blk);
    } else {
        return tempBlock->getAddr();
    }
}

void
BaseCache::init()
{
    if (!cpuSidePort.isConnected() || !memSidePort.isConnected())
        fatal("Cache ports on %s are not connected\n", name());
    cpuSidePort.sendRangeChange();
    forwardSnoops = cpuSidePort.isSnooping();
}

Port &
BaseCache::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "mem_side") {
        return memSidePort;
    } else if (if_name == "cpu_side") {
        return cpuSidePort;
    }  else {
        return ClockedObject::getPort(if_name, idx);
    }
}

bool
BaseCache::inRange(Addr addr) const
{
    for (const auto& r : addrRanges) {
        if (r.contains(addr)) {
            return true;
       }
    }
    return false;
}

void
BaseCache::handleTimingReqHit(PacketPtr pkt, CacheBlk *blk, Tick request_time)
{

    // handle special cases for LockedRMW transactions
    if (pkt->isLockedRMW()) {
        Addr blk_addr = pkt->getBlockAddr(blkSize);

        if (pkt->isRead()) {
            // Read hit for LockedRMW.  Since it requires exclusive
            // permissions, there should be no outstanding access.
            assert(!mshrQueue.findMatch(blk_addr, pkt->isSecure()));
            // The keys to LockedRMW are that (1) we always have an MSHR
            // allocated during the RMW interval to catch snoops and
            // defer them until after the RMW completes, and (2) we
            // clear permissions on the block to turn any upstream
            // access other than the matching write into a miss, causing
            // it to append to the MSHR as well.

            // Because we hit in the cache, we have to fake an MSHR to
            // achieve part (1).  If the read had missed, this MSHR
            // would get allocated as part of normal miss processing.
            // Basically we need to get the MSHR in the same state as if
            // we had missed and just received the response.
            // Request *req2 = new Request(*(pkt->req));
            RequestPtr req2 = std::make_shared<Request>(*(pkt->req));
            PacketPtr pkt2 = new Packet(req2, pkt->cmd);
            MSHR *mshr = allocateMissBuffer(pkt2, curTick(), true);
            // Mark the MSHR "in service" (even though it's not) to prevent
            // the cache from sending out a request.
            mshrQueue.markInService(mshr, false);
            // Part (2): mark block inaccessible
            assert(blk);
            blk->clearCoherenceBits(CacheBlk::ReadableBit);
            blk->clearCoherenceBits(CacheBlk::WritableBit);
        } else {
            assert(pkt->isWrite());
            // All LockedRMW writes come here, as they cannot miss.
            // Need to undo the two things described above.  Block
            // permissions were already restored earlier in this
            // function, prior to the access() call.  Now we just need
            // to clear out the MSHR.

            // Read should have already allocated MSHR.
            MSHR *mshr = mshrQueue.findMatch(blk_addr, pkt->isSecure());
            assert(mshr);
            // Fake up a packet and "respond" to the still-pending
            // LockedRMWRead, to process any pending targets and clear
            // out the MSHR
            PacketPtr resp_pkt =
                new Packet(pkt->req, MemCmd::LockedRMWWriteResp);
            resp_pkt->senderState = mshr;
            recvTimingResp(resp_pkt);
        }
    }

    if (pkt->needsResponse()) {
        // These delays should have been consumed by now
        assert(pkt->headerDelay == 0);
        assert(pkt->payloadDelay == 0);

        pkt->makeTimingResponse();

        // In this case we are considering request_time that takes
        // into account the delay of the xbar, if any, and just
        // lat, neglecting responseLatency, modelling hit latency
        // just as the value of lat overriden by access(), which calls
        // the calculateAccessLatency() function.
        cpuSidePort.schedTimingResp(pkt, request_time);
    } else {
        DPRINTF(Cache, "%s satisfied %s, no response needed\n", __func__,
                pkt->print());

        // queue the packet for deletion, as the sending cache is
        // still relying on it; if the block is found in access(),
        // CleanEvict and Writeback messages will be deleted
        // here as well
        pendingDelete.reset(pkt);
    }
}

void
BaseCache::handleTimingReqMiss(PacketPtr pkt, MSHR *mshr, CacheBlk *blk,
                               Tick forward_time, Tick request_time)
{
    if (writeAllocator &&
        pkt && pkt->isWrite() && !pkt->req->isUncacheable()) {
        writeAllocator->updateMode(pkt->getAddr(), pkt->getSize(),
                                   pkt->getBlockAddr(blkSize));
    }

    if (mshr) {
        /// MSHR hit
        /// @note writebacks will be checked in getNextMSHR()
        /// for any conflicting requests to the same block

        //@todo remove hw_pf here

        // Coalesce unless it was a software prefetch (see above).
        if (pkt) {
            assert(!pkt->isWriteback());
            // CleanEvicts corresponding to blocks which have
            // outstanding requests in MSHRs are simply sunk here
            if (pkt->cmd == MemCmd::CleanEvict) {
                pendingDelete.reset(pkt);
            } else if (pkt->cmd == MemCmd::WriteClean) {
                // A WriteClean should never coalesce with any
                // outstanding cache maintenance requests.

                // We use forward_time here because there is an
                // uncached memory write, forwarded to WriteBuffer.
                allocateWriteBuffer(pkt, forward_time);
            } else {
                DPRINTF(Cache, "%s coalescing MSHR for %s\n", __func__,
                        pkt->print());

                assert(pkt->req->requestorId() < system->maxRequestors());
                stats.cmdStats(pkt).mshrHits[pkt->req->requestorId()]++;

                // We use forward_time here because it is the same
                // considering new targets. We have multiple
                // requests for the same address here. It
                // specifies the latency to allocate an internal
                // buffer and to schedule an event to the queued
                // port and also takes into account the additional
                // delay of the xbar.
                mshr->allocateTarget(pkt, forward_time, order++,
                                     allocOnFill(pkt->cmd));
                if (mshr->getNumTargets() >= numTarget) {
                    noTargetMSHR = mshr;
                    setBlocked(Blocked_NoTargets);
                    // need to be careful with this... if this mshr isn't
                    // ready yet (i.e. time > curTick()), we don't want to
                    // move it ahead of mshrs that are ready
                    // mshrQueue.moveToFront(mshr);
                }
            }
        }
    } else {
        // no MSHR
        assert(pkt->req->requestorId() < system->maxRequestors());
        stats.cmdStats(pkt).mshrMisses[pkt->req->requestorId()]++;
        if (prefetcher && pkt->isDemand())
            prefetcher->incrDemandMhsrMisses();

        if (pkt->isEviction() || pkt->cmd == MemCmd::WriteClean) {
            // We use forward_time here because there is an
            // writeback or writeclean, forwarded to WriteBuffer.
            allocateWriteBuffer(pkt, forward_time);
        } else {
            if (blk && blk->isValid()) {
                // If we have a write miss to a valid block, we
                // need to mark the block non-readable.  Otherwise
                // if we allow reads while there's an outstanding
                // write miss, the read could return stale data
                // out of the cache block... a more aggressive
                // system could detect the overlap (if any) and
                // forward data out of the MSHRs, but we don't do
                // that yet.  Note that we do need to leave the
                // block valid so that it stays in the cache, in
                // case we get an upgrade response (and hence no
                // new data) when the write miss completes.
                // As long as CPUs do proper store/load forwarding
                // internally, and have a sufficiently weak memory
                // model, this is probably unnecessary, but at some
                // point it must have seemed like we needed it...
                assert((pkt->needsWritable() &&
                    !blk->isSet(CacheBlk::WritableBit)) ||
                    pkt->req->isCacheMaintenance());
                blk->clearCoherenceBits(CacheBlk::ReadableBit);
            }
            // Here we are using forward_time, modelling the latency of
            // a miss (outbound) just as forwardLatency, neglecting the
            // lookupLatency component.
            allocateMissBuffer(pkt, forward_time);
        }
    }
}

void
BaseCache::recvTimingReq(PacketPtr pkt)
{
    // anything that is merely forwarded pays for the forward latency and
    // the delay provided by the crossbar
    Tick forward_time = clockEdge(forwardLatency) + pkt->headerDelay;

    if (pkt->cmd == MemCmd::LockedRMWWriteReq) {
        // For LockedRMW accesses, we mark the block inaccessible after the
        // read (see below), to make sure no one gets in before the write.
        // Now that the write is here, mark it accessible again, so the
        // write will succeed.  LockedRMWReadReq brings the block in in
        // exclusive mode, so we know it was previously writable.
        CacheBlk *blk = tags->findBlock(pkt->getAddr(), pkt->isSecure());
        assert(blk && blk->isValid());
        assert(!blk->isSet(CacheBlk::WritableBit) &&
               !blk->isSet(CacheBlk::ReadableBit));
        blk->setCoherenceBits(CacheBlk::ReadableBit);
        blk->setCoherenceBits(CacheBlk::WritableBit);
    }

    Cycles lat;
    CacheBlk *blk = nullptr;
    bool satisfied = false;
    {
        PacketList writebacks;
        // Note that lat is passed by reference here. The function
        // access() will set the lat value.
        satisfied = access(pkt, blk, lat, writebacks);

        // After the evicted blocks are selected, they must be forwarded
        // to the write buffer to ensure they logically precede anything
        // happening below
        doWritebacks(writebacks, clockEdge(lat + forwardLatency));
    }

    // Here we charge the headerDelay that takes into account the latencies
    // of the bus, if the packet comes from it.
    // The latency charged is just the value set by the access() function.
    // In case of a hit we are neglecting response latency.
    // In case of a miss we are neglecting forward latency.
    Tick request_time = clockEdge(lat);
    // Here we reset the timing of the packet.
    pkt->headerDelay = pkt->payloadDelay = 0;

    if (satisfied) {
        // notify before anything else as later handleTimingReqHit might turn
        // the packet in a response
        ppHit->notify(pkt);

        if (prefetcher && blk && blk->wasPrefetched()) {
            DPRINTF(Cache, "Hit on prefetch for addr %#x (%s)\n",
                    pkt->getAddr(), pkt->isSecure() ? "s" : "ns");
            blk->clearPrefetched();
        }

        handleTimingReqHit(pkt, blk, request_time);
    } else {
        handleTimingReqMiss(pkt, blk, forward_time, request_time);

        ppMiss->notify(pkt);
    }

    if (prefetcher) {
        // track time of availability of next prefetch, if any
        Tick next_pf_time = prefetcher->nextPrefetchReadyTime();
        if (next_pf_time != MaxTick) {
            schedMemSideSendEvent(next_pf_time);
        }
    }
}

void
BaseCache::handleUncacheableWriteResp(PacketPtr pkt)
{
    Tick completion_time = clockEdge(responseLatency) +
        pkt->headerDelay + pkt->payloadDelay;

    // Reset the bus additional time as it is now accounted for
    pkt->headerDelay = pkt->payloadDelay = 0;

    cpuSidePort.schedTimingResp(pkt, completion_time);
}

void
BaseCache::recvTimingResp(PacketPtr pkt)
{
    assert(pkt->isResponse());

    // all header delay should be paid for by the crossbar, unless
    // this is a prefetch response from above
    panic_if(pkt->headerDelay != 0 && pkt->cmd != MemCmd::HardPFResp,
             "%s saw a non-zero packet delay\n", name());

    const bool is_error = pkt->isError();

    if (is_error) {
        DPRINTF(Cache, "%s: Cache received %s with error\n", __func__,
                pkt->print());
    }

    DPRINTF(Cache, "%s: Handling response %s\n", __func__,
            pkt->print());

    // if this is a write, we should be looking at an uncacheable
    // write
    if (pkt->isWrite() && pkt->cmd != MemCmd::LockedRMWWriteResp) {
        assert(pkt->req->isUncacheable());
        handleUncacheableWriteResp(pkt);
        return;
    }

    // we have dealt with any (uncacheable) writes above, from here on
    // we know we are dealing with an MSHR due to a miss or a prefetch
    MSHR *mshr = dynamic_cast<MSHR*>(pkt->popSenderState());
    assert(mshr);

    if (mshr == noTargetMSHR) {
        // we always clear at least one target
        clearBlocked(Blocked_NoTargets);
        noTargetMSHR = nullptr;
    }

    // Initial target is used just for stats
    const QueueEntry::Target *initial_tgt = mshr->getTarget();
    const Tick miss_latency = curTick() - initial_tgt->recvTime;
    if (pkt->req->isUncacheable()) {
        assert(pkt->req->requestorId() < system->maxRequestors());
        stats.cmdStats(initial_tgt->pkt)
            .mshrUncacheableLatency[pkt->req->requestorId()] += miss_latency;
    } else {
        assert(pkt->req->requestorId() < system->maxRequestors());
        stats.cmdStats(initial_tgt->pkt)
            .mshrMissLatency[pkt->req->requestorId()] += miss_latency;
    }

    PacketList writebacks;

    bool is_fill = !mshr->isForward &&
        (pkt->isRead() || pkt->cmd == MemCmd::UpgradeResp ||
         mshr->wasWholeLineWrite);

    // make sure that if the mshr was due to a whole line write then
    // the response is an invalidation
    assert(!mshr->wasWholeLineWrite || pkt->isInvalidate());

    CacheBlk *blk = tags->findBlock(pkt->getAddr(), pkt->isSecure());

    if (is_fill && !is_error) {
        DPRINTF(Cache, "Block for addr %#llx being updated in Cache\n",
                pkt->getAddr());

        const bool allocate = (writeAllocator && mshr->wasWholeLineWrite) ?
            writeAllocator->allocate() : mshr->allocOnFill();
        blk = handleFill(pkt, blk, writebacks, allocate);
        assert(blk != nullptr);
        ppFill->notify(pkt);
    }

    // Don't want to promote the Locked RMW Read until
    // the locked write comes in
    if (!mshr->hasLockedRMWReadTarget()) {
        if (blk && blk->isValid() && pkt->isClean() && !pkt->isInvalidate()) {
            // The block was marked not readable while there was a pending
            // cache maintenance operation, restore its flag.
            blk->setCoherenceBits(CacheBlk::ReadableBit);

            // This was a cache clean operation (without invalidate)
            // and we have a copy of the block already. Since there
            // is no invalidation, we can promote targets that don't
            // require a writable copy
            mshr->promoteReadable();
        }

        if (blk && blk->isSet(CacheBlk::WritableBit) &&
            !pkt->req->isCacheInvalidate()) {
            // If at this point the referenced block is writable and the
            // response is not a cache invalidate, we promote targets that
            // were deferred as we couldn't guarrantee a writable copy
            mshr->promoteWritable();
        }
    }

    serviceMSHRTargets(mshr, pkt, blk);
    // We are stopping servicing targets early for the Locked RMW Read until
    // the write comes.
    if (!mshr->hasLockedRMWReadTarget()) {
        if (mshr->promoteDeferredTargets()) {
            // avoid later read getting stale data while write miss is
            // outstanding.. see comment in timingAccess()
            if (blk) {
                blk->clearCoherenceBits(CacheBlk::ReadableBit);
            }
            mshrQueue.markPending(mshr);
            schedMemSideSendEvent(clockEdge() + pkt->payloadDelay);
        } else {
            // while we deallocate an mshr from the queue we still have to
            // check the isFull condition before and after as we might
            // have been using the reserved entries already
            const bool was_full = mshrQueue.isFull();
            mshrQueue.deallocate(mshr);
            if (was_full && !mshrQueue.isFull()) {
                clearBlocked(Blocked_NoMSHRs);
            }

            // Request the bus for a prefetch if this deallocation freed enough
            // MSHRs for a prefetch to take place
            if (prefetcher && mshrQueue.canPrefetch() && !isBlocked()) {
                Tick next_pf_time = std::max(
                    prefetcher->nextPrefetchReadyTime(), clockEdge());
                if (next_pf_time != MaxTick)
                    schedMemSideSendEvent(next_pf_time);
            }
        }

        // if we used temp block, check to see if its valid and then clear it
        if (blk == tempBlock && tempBlock->isValid()) {
            evictBlock(blk, writebacks);
        }
    }

    const Tick forward_time = clockEdge(forwardLatency) + pkt->headerDelay;
    // copy writebacks to write buffer
    doWritebacks(writebacks, forward_time);

    DPRINTF(CacheVerbose, "%s: Leaving with %s\n", __func__, pkt->print());
    delete pkt;
}


Tick
BaseCache::recvAtomic(PacketPtr pkt)
{
    // should assert here that there are no outstanding MSHRs or
    // writebacks... that would mean that someone used an atomic
    // access in timing mode

    // We use lookupLatency here because it is used to specify the latency
    // to access.
    Cycles lat = lookupLatency;

    CacheBlk *blk = nullptr;
    PacketList writebacks;
    bool satisfied = access(pkt, blk, lat, writebacks);

    if (pkt->isClean() && blk && blk->isSet(CacheBlk::DirtyBit)) {
        // A cache clean opearation is looking for a dirty
        // block. If a dirty block is encountered a WriteClean
        // will update any copies to the path to the memory
        // until the point of reference.
        DPRINTF(CacheVerbose, "%s: packet %s found block: %s\n",
                __func__, pkt->print(), blk->print());
        PacketPtr wb_pkt = writecleanBlk(blk, pkt->req->getDest(), pkt->id);
        writebacks.push_back(wb_pkt);
        pkt->setSatisfied();
    }

    // handle writebacks resulting from the access here to ensure they
    // logically precede anything happening below
    doWritebacksAtomic(writebacks);
    assert(writebacks.empty());

    if (!satisfied) {
        lat += handleAtomicReqMiss(pkt, blk, writebacks);
    }

    // Note that we don't invoke the prefetcher at all in atomic mode.
    // It's not clear how to do it properly, particularly for
    // prefetchers that aggressively generate prefetch candidates and
    // rely on bandwidth contention to throttle them; these will tend
    // to pollute the cache in atomic mode since there is no bandwidth
    // contention.  If we ever do want to enable prefetching in atomic
    // mode, though, this is the place to do it... see timingAccess()
    // for an example (though we'd want to issue the prefetch(es)
    // immediately rather than calling requestMemSideBus() as we do
    // there).

    // do any writebacks resulting from the response handling
    doWritebacksAtomic(writebacks);

    // if we used temp block, check to see if its valid and if so
    // clear it out, but only do so after the call to recvAtomic is
    // finished so that any downstream observers (such as a snoop
    // filter), first see the fill, and only then see the eviction
    if (blk == tempBlock && tempBlock->isValid()) {
        // the atomic CPU calls recvAtomic for fetch and load/store
        // sequentuially, and we may already have a tempBlock
        // writeback from the fetch that we have not yet sent
        if (tempBlockWriteback) {
            // if that is the case, write the prevoius one back, and
            // do not schedule any new event
            writebackTempBlockAtomic();
        } else {
            // the writeback/clean eviction happens after the call to
            // recvAtomic has finished (but before any successive
            // calls), so that the response handling from the fill is
            // allowed to happen first
            schedule(writebackTempBlockAtomicEvent, curTick());
        }

        tempBlockWriteback = evictBlock(blk);
    }

    if (pkt->needsResponse()) {
        pkt->makeAtomicResponse();
    }

    return lat * clockPeriod();
}

void
BaseCache::functionalAccess(PacketPtr pkt, bool from_cpu_side)
{
    Addr blk_addr = pkt->getBlockAddr(blkSize);
    bool is_secure = pkt->isSecure();
    CacheBlk *blk = tags->findBlock(pkt->getAddr(), is_secure);
    MSHR *mshr = mshrQueue.findMatch(blk_addr, is_secure);

    pkt->pushLabel(name());

    CacheBlkPrintWrapper cbpw(blk);

    // Note that just because an L2/L3 has valid data doesn't mean an
    // L1 doesn't have a more up-to-date modified copy that still
    // needs to be found.  As a result we always update the request if
    // we have it, but only declare it satisfied if we are the owner.

    // see if we have data at all (owned or otherwise)
    bool have_data = blk && blk->isValid()
        && pkt->trySatisfyFunctional(&cbpw, blk_addr, is_secure, blkSize,
                                     blk->data);

    // data we have is dirty if marked as such or if we have an
    // in-service MSHR that is pending a modified line
    bool have_dirty =
        have_data && (blk->isSet(CacheBlk::DirtyBit) ||
                      (mshr && mshr->inService && mshr->isPendingModified()));

    bool done = have_dirty ||
        cpuSidePort.trySatisfyFunctional(pkt) ||
        mshrQueue.trySatisfyFunctional(pkt) ||
        writeBuffer.trySatisfyFunctional(pkt) ||
        memSidePort.trySatisfyFunctional(pkt);

    DPRINTF(CacheVerbose, "%s: %s %s%s%s\n", __func__,  pkt->print(),
            (blk && blk->isValid()) ? "valid " : "",
            have_data ? "data " : "", done ? "done " : "");

    // We're leaving the cache, so pop cache->name() label
    pkt->popLabel();

    if (done) {
        pkt->makeResponse();
    } else {
        // if it came as a request from the CPU side then make sure it
        // continues towards the memory side
        if (from_cpu_side) {
            memSidePort.sendFunctional(pkt);
        } else if (cpuSidePort.isSnooping()) {
            // if it came from the memory side, it must be a snoop request
            // and we should only forward it if we are forwarding snoops
            cpuSidePort.sendFunctionalSnoop(pkt);
        }
    }
}

void
BaseCache::updateBlockData(CacheBlk *blk, const PacketPtr cpkt,
    bool has_old_data)
{
    DataUpdate data_update(regenerateBlkAddr(blk), blk->isSecure());
    if (ppDataUpdate->hasListeners()) {
        if (has_old_data) {
            data_update.oldData = std::vector<uint64_t>(blk->data,
                blk->data + (blkSize / sizeof(uint64_t)));
        }
    }

    // Actually perform the data update
    if (cpkt) {
        cpkt->writeDataToBlock(blk->data, blkSize);
    }

    if (ppDataUpdate->hasListeners()) {
        if (cpkt) {
            data_update.newData = std::vector<uint64_t>(blk->data,
                blk->data + (blkSize / sizeof(uint64_t)));
        }
        ppDataUpdate->notify(data_update);
    }
}

void
BaseCache::cmpAndSwap(CacheBlk *blk, PacketPtr pkt)
{
    assert(pkt->isRequest());

    uint64_t overwrite_val;
    bool overwrite_mem;
    uint64_t condition_val64;
    uint32_t condition_val32;

    int offset = pkt->getOffset(blkSize);
    uint8_t *blk_data = blk->data + offset;

    assert(sizeof(uint64_t) >= pkt->getSize());

    // Get a copy of the old block's contents for the probe before the update
    DataUpdate data_update(regenerateBlkAddr(blk), blk->isSecure());
    if (ppDataUpdate->hasListeners()) {
        data_update.oldData = std::vector<uint64_t>(blk->data,
            blk->data + (blkSize / sizeof(uint64_t)));
    }

    overwrite_mem = true;
    // keep a copy of our possible write value, and copy what is at the
    // memory address into the packet
    pkt->writeData((uint8_t *)&overwrite_val);
    pkt->setData(blk_data);

    if (pkt->req->isCondSwap()) {
        if (pkt->getSize() == sizeof(uint64_t)) {
            condition_val64 = pkt->req->getExtraData();
            overwrite_mem = !std::memcmp(&condition_val64, blk_data,
                                         sizeof(uint64_t));
        } else if (pkt->getSize() == sizeof(uint32_t)) {
            condition_val32 = (uint32_t)pkt->req->getExtraData();
            overwrite_mem = !std::memcmp(&condition_val32, blk_data,
                                         sizeof(uint32_t));
        } else
            panic("Invalid size for conditional read/write\n");
    }

    if (overwrite_mem) {
        std::memcpy(blk_data, &overwrite_val, pkt->getSize());
        blk->setCoherenceBits(CacheBlk::DirtyBit);

        if (ppDataUpdate->hasListeners()) {
            data_update.newData = std::vector<uint64_t>(blk->data,
                blk->data + (blkSize / sizeof(uint64_t)));
            ppDataUpdate->notify(data_update);
        }
    }
}

QueueEntry*
BaseCache::getNextQueueEntry()
{
    // Check both MSHR queue and write buffer for potential requests,
    // note that null does not mean there is no request, it could
    // simply be that it is not ready
    MSHR *miss_mshr  = mshrQueue.getNext();
    WriteQueueEntry *wq_entry = writeBuffer.getNext();

    // If we got a write buffer request ready, first priority is a
    // full write buffer, otherwise we favour the miss requests
    if (wq_entry && (writeBuffer.isFull() || !miss_mshr)) {
        // need to search MSHR queue for conflicting earlier miss.
        MSHR *conflict_mshr = mshrQueue.findPending(wq_entry);

        if (conflict_mshr && conflict_mshr->order < wq_entry->order) {
            // Service misses in order until conflict is cleared.
            return conflict_mshr;

            // @todo Note that we ignore the ready time of the conflict here
        }

        // No conflicts; issue write
        return wq_entry;
    } else if (miss_mshr) {
        // need to check for conflicting earlier writeback
        WriteQueueEntry *conflict_mshr = writeBuffer.findPending(miss_mshr);
        if (conflict_mshr) {
            // not sure why we don't check order here... it was in the
            // original code but commented out.

            // The only way this happens is if we are
            // doing a write and we didn't have permissions
            // then subsequently saw a writeback (owned got evicted)
            // We need to make sure to perform the writeback first
            // To preserve the dirty data, then we can issue the write

            // should we return wq_entry here instead?  I.e. do we
            // have to flush writes in order?  I don't think so... not
            // for Alpha anyway.  Maybe for x86?
            return conflict_mshr;

            // @todo Note that we ignore the ready time of the conflict here
        }

        // No conflicts; issue read
        return miss_mshr;
    }

    // fall through... no pending requests.  Try a prefetch.
    assert(!miss_mshr && !wq_entry);
    if (prefetcher && mshrQueue.canPrefetch() && !isBlocked()) {
        // If we have a miss queue slot, we can try a prefetch
        PacketPtr pkt = prefetcher->getPacket();
        if (pkt) {
            Addr pf_addr = pkt->getBlockAddr(blkSize);
            if (tags->findBlock(pf_addr, pkt->isSecure())) {
                DPRINTF(HWPrefetch, "Prefetch %#x has hit in cache, "
                        "dropped.\n", pf_addr);
                prefetcher->pfHitInCache();
                // free the request and packet
                delete pkt;
            } else if (mshrQueue.findMatch(pf_addr, pkt->isSecure())) {
                DPRINTF(HWPrefetch, "Prefetch %#x has hit in a MSHR, "
                        "dropped.\n", pf_addr);
                prefetcher->pfHitInMSHR();
                // free the request and packet
                delete pkt;
            } else if (writeBuffer.findMatch(pf_addr, pkt->isSecure())) {
                DPRINTF(HWPrefetch, "Prefetch %#x has hit in the "
                        "Write Buffer, dropped.\n", pf_addr);
                prefetcher->pfHitInWB();
                // free the request and packet
                delete pkt;
            } else {
                // Update statistic on number of prefetches issued
                // (hwpf_mshr_misses)
                assert(pkt->req->requestorId() < system->maxRequestors());
                stats.cmdStats(pkt).mshrMisses[pkt->req->requestorId()]++;

                // allocate an MSHR and return it, note
                // that we send the packet straight away, so do not
                // schedule the send
                return allocateMissBuffer(pkt, curTick(), false);
            }
        }
    }

    return nullptr;
}

bool
BaseCache::handleEvictions(std::vector<CacheBlk*> &evict_blks,
    PacketList &writebacks)
{
    bool replacement = false;
    for (const auto& blk : evict_blks) {
        if (blk->isValid()) {
            replacement = true;

            const MSHR* mshr =
                mshrQueue.findMatch(regenerateBlkAddr(blk), blk->isSecure());
            if (mshr) {
                // Must be an outstanding upgrade or clean request on a block
                // we're about to replace
                assert((!blk->isSet(CacheBlk::WritableBit) &&
                    mshr->needsWritable()) || mshr->isCleaning());
                return false;
            }
        }
    }

    // The victim will be replaced by a new entry, so increase the replacement
    // counter if a valid block is being replaced
    if (replacement) {
        stats.replacements++;

        // Evict valid blocks associated to this victim block
        for (auto& blk : evict_blks) {
            if (blk->isValid()) {
                evictBlock(blk, writebacks);
            }
        }
    }

    return true;
}

bool
BaseCache::updateCompressionData(CacheBlk *&blk, const uint64_t* data,
                                 PacketList &writebacks)
{
    // tempBlock does not exist in the tags, so don't do anything for it.
    if (blk == tempBlock) {
        return true;
    }

    // The compressor is called to compress the updated data, so that its
    // metadata can be updated.
    Cycles compression_lat = Cycles(0);
    Cycles decompression_lat = Cycles(0);
    const auto comp_data =
        compressor->compress(data, compression_lat, decompression_lat);
    std::size_t compression_size = comp_data->getSizeBits();

    // Get previous compressed size
    CompressionBlk* compression_blk = static_cast<CompressionBlk*>(blk);
    [[maybe_unused]] const std::size_t prev_size =
        compression_blk->getSizeBits();

    // If compressed size didn't change enough to modify its co-allocatability
    // there is nothing to do. Otherwise we may be facing a data expansion
    // (block passing from more compressed to less compressed state), or a
    // data contraction (less to more).
    bool is_data_expansion = false;
    bool is_data_contraction = false;
    const CompressionBlk::OverwriteType overwrite_type =
        compression_blk->checkExpansionContraction(compression_size);
    std::string op_name = "";
    if (overwrite_type == CompressionBlk::DATA_EXPANSION) {
        op_name = "expansion";
        is_data_expansion = true;
    } else if ((overwrite_type == CompressionBlk::DATA_CONTRACTION) &&
        moveContractions) {
        op_name = "contraction";
        is_data_contraction = true;
    }

    // If block changed compression state, it was possibly co-allocated with
    // other blocks and cannot be co-allocated anymore, so one or more blocks
    // must be evicted to make room for the expanded/contracted block
    std::vector<CacheBlk*> evict_blks;
    if (is_data_expansion || is_data_contraction) {
        std::vector<CacheBlk*> evict_blks;
        bool victim_itself = false;
        CacheBlk *victim = nullptr;
        if (replaceExpansions || is_data_contraction) {
            victim = tags->findVictim(regenerateBlkAddr(blk),
                blk->isSecure(), compression_size, evict_blks);

            // It is valid to return nullptr if there is no victim
            if (!victim) {
                return false;
            }

            // If the victim block is itself the block won't need to be moved,
            // and the victim should not be evicted
            if (blk == victim) {
                victim_itself = true;
                auto it = std::find_if(evict_blks.begin(), evict_blks.end(),
                    [&blk](CacheBlk* evict_blk){ return evict_blk == blk; });
                evict_blks.erase(it);
            }

            // Print victim block's information
            DPRINTF(CacheRepl, "Data %s replacement victim: %s\n",
                op_name, victim->print());
        } else {
            // If we do not move the expanded block, we must make room for
            // the expansion to happen, so evict every co-allocated block
            const SuperBlk* superblock = static_cast<const SuperBlk*>(
                compression_blk->getSectorBlock());
            for (auto& sub_blk : superblock->blks) {
                if (sub_blk->isValid() && (blk != sub_blk)) {
                    evict_blks.push_back(sub_blk);
                }
            }
        }

        // Try to evict blocks; if it fails, give up on update
        if (!handleEvictions(evict_blks, writebacks)) {
            return false;
        }

        DPRINTF(CacheComp, "Data %s: [%s] from %d to %d bits\n",
                op_name, blk->print(), prev_size, compression_size);

        if (!victim_itself && (replaceExpansions || is_data_contraction)) {
            // Move the block's contents to the invalid block so that it now
            // co-allocates with the other existing superblock entry
            tags->moveBlock(blk, victim);
            blk = victim;
            compression_blk = static_cast<CompressionBlk*>(blk);
        }
    }

    // Update the number of data expansions/contractions
    if (is_data_expansion) {
        stats.dataExpansions++;
    } else if (is_data_contraction) {
        stats.dataContractions++;
    }

    compression_blk->setSizeBits(compression_size);
    compression_blk->setDecompressionLatency(decompression_lat);

    return true;
}

void
BaseCache::satisfyRequest(PacketPtr pkt, CacheBlk *blk, bool, bool)
{
    assert(pkt->isRequest());

    assert(blk && blk->isValid());
    // Occasionally this is not true... if we are a lower-level cache
    // satisfying a string of Read and ReadEx requests from
    // upper-level caches, a Read will mark the block as shared but we
    // can satisfy a following ReadEx anyway since we can rely on the
    // Read requestor(s) to have buffered the ReadEx snoop and to
    // invalidate their blocks after receiving them.
    // assert(!pkt->needsWritable() || blk->isSet(CacheBlk::WritableBit));
    assert(pkt->getOffset(blkSize) + pkt->getSize() <= blkSize);

    // Check RMW operations first since both isRead() and
    // isWrite() will be true for them
    if (pkt->cmd == MemCmd::SwapReq) {
        if (pkt->isAtomicOp()) {
            // Get a copy of the old block's contents for the probe before
            // the update
            DataUpdate data_update(regenerateBlkAddr(blk), blk->isSecure());
            if (ppDataUpdate->hasListeners()) {
                data_update.oldData = std::vector<uint64_t>(blk->data,
                    blk->data + (blkSize / sizeof(uint64_t)));
            }

            // extract data from cache and save it into the data field in
            // the packet as a return value from this atomic op
            int offset = tags->extractBlkOffset(pkt->getAddr());
            uint8_t *blk_data = blk->data + offset;
            pkt->setData(blk_data);

            // execute AMO operation
            (*(pkt->getAtomicOp()))(blk_data);

            // Inform of this block's data contents update
            if (ppDataUpdate->hasListeners()) {
                data_update.newData = std::vector<uint64_t>(blk->data,
                    blk->data + (blkSize / sizeof(uint64_t)));
                ppDataUpdate->notify(data_update);
            }

            // set block status to dirty
            blk->setCoherenceBits(CacheBlk::DirtyBit);
        } else {
            cmpAndSwap(blk, pkt);
        }
    } else if (pkt->isWrite()) {
        // we have the block in a writable state and can go ahead,
        // note that the line may be also be considered writable in
        // downstream caches along the path to memory, but always
        // Exclusive, and never Modified
        assert(blk->isSet(CacheBlk::WritableBit));
        // Write or WriteLine at the first cache with block in writable state
        if (blk->checkWrite(pkt)) {
            updateBlockData(blk, pkt, true);
        }
        // Always mark the line as dirty (and thus transition to the
        // Modified state) even if we are a failed StoreCond so we
        // supply data to any snoops that have appended themselves to
        // this cache before knowing the store will fail.
        blk->setCoherenceBits(CacheBlk::DirtyBit);
        DPRINTF(CacheVerbose, "%s for %s (write)\n", __func__, pkt->print());
    } else if (pkt->isRead()) {
        if (pkt->isLLSC()) {
            blk->trackLoadLocked(pkt);
        }

        // all read responses have a data payload
        assert(pkt->hasRespData());
        pkt->setDataFromBlock(blk->data, blkSize);
    } else if (pkt->isUpgrade()) {
        // sanity check
        assert(!pkt->hasSharers());

        if (blk->isSet(CacheBlk::DirtyBit)) {
            // we were in the Owned state, and a cache above us that
            // has the line in Shared state needs to be made aware
            // that the data it already has is in fact dirty
            pkt->setCacheResponding();
            blk->clearCoherenceBits(CacheBlk::DirtyBit);
        }
    } else if (pkt->isClean()) {
        blk->clearCoherenceBits(CacheBlk::DirtyBit);
    } else {
        assert(pkt->isInvalidate());
        invalidateBlock(blk);
        DPRINTF(CacheVerbose, "%s for %s (invalidation)\n", __func__,
                pkt->print());
    }
}

/////////////////////////////////////////////////////
//
// Access path: requests coming in from the CPU side
//
/////////////////////////////////////////////////////
Cycles
BaseCache::calculateTagOnlyLatency(const uint32_t delay,
                                   const Cycles lookup_lat) const
{
    // A tag-only access has to wait for the packet to arrive in order to
    // perform the tag lookup.
    return ticksToCycles(delay) + lookup_lat;
}

Cycles
BaseCache::calculateAccessLatency(const CacheBlk* blk, const uint32_t delay,
                                  const Cycles lookup_lat) const
{
    Cycles lat(0);

    if (blk != nullptr) {
        // As soon as the access arrives, for sequential accesses first access
        // tags, then the data entry. In the case of parallel accesses the
        // latency is dictated by the slowest of tag and data latencies.
        if (sequentialAccess) {
            lat = ticksToCycles(delay) + lookup_lat + dataLatency;
        } else {
            lat = ticksToCycles(delay) + std::max(lookup_lat, dataLatency);
        }

        // Check if the block to be accessed is available. If not, apply the
        // access latency on top of when the block is ready to be accessed.
        const Tick tick = curTick() + delay;
        const Tick when_ready = blk->getWhenReady();
        if (when_ready > tick &&
            ticksToCycles(when_ready - tick) > lat) {
            lat += ticksToCycles(when_ready - tick);
        }
    } else {
        // In case of a miss, we neglect the data access in a parallel
        // configuration (i.e., the data access will be stopped as soon as
        // we find out it is a miss), and use the tag-only latency.
        lat = calculateTagOnlyLatency(delay, lookup_lat);
    }

    return lat;
}

bool
BaseCache::access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                  PacketList &writebacks)
{
    // sanity check
    assert(pkt->isRequest());

    gem5_assert(!(isReadOnly && pkt->isWrite()),
                "Should never see a write in a read-only cache %s\n",
                name());

    // Access block in the tags
    Cycles tag_latency(0);
    blk = tags->accessBlock(pkt, tag_latency);

/*lockingDurationsTable[0xfffff8d8]=36;
lockingDurationsTable[0xfffff938]=100;
lockingDurationsTable[0xfffffd6c]=94;
lockingDurationsTable[0xfffff8b0]=39;
lockingDurationsTable[0x0007ea28]=38;
lockingDurationsTable[0xfffff878]=4;
lockingDurationsTable[0xfffffb00]=5;
lockingDurationsTable[0xfffffb40]=5;
lockingDurationsTable[0xfffffac0]=6;
lockingDurationsTable[0xfffff9c0]=5;
lockingDurationsTable[0xfffffa40]=5;
lockingDurationsTable[0xfffffa80]=6;
lockingDurationsTable[0xfffff9a0]=5;
lockingDurationsTable[0x000503d0]=2;
lockingDurationsTable[0x0004ffa0]=2;
lockingDurationsTable[0x00050800]=2;*/

/*lockingDurationsTable[0xfffff8d8]=36;
lockingDurationsTable[0xfffff938]=100;
lockingDurationsTable[0xfffffd6c]=94;
lockingDurationsTable[0xfffff8b0]=39;
lockingDurationsTable[0x0007ea28]=38;
lockingDurationsTable[0xfffff878]=4;
lockingDurationsTable[0xfffff9c0]=5;
lockingDurationsTable[0xfffff9a0]=5;*/







/*lockingDurationsTable[0x0006e230]=1010;
lockingDurationsTable[0x00070cac]=1096;
lockingDurationsTable[0x0006e258]=839;
lockingDurationsTable[0xfffff590]=1064;
lockingDurationsTable[0xfffff568]=938;
lockingDurationsTable[0x0006e308]=577;
lockingDurationsTable[0x000697d0]=456;
lockingDurationsTable[0x0006e2f0]=297;*/

/* Window Size 45000*/
/*lockingDurationsTable[0xfffffae8]=1532;
lockingDurationsTable[0xfffffb10]=877;
lockingDurationsTable[0x00073340]=332;
lockingDurationsTable[0x000733b0]=325;
lockingDurationsTable[0x000733d0]=322;
lockingDurationsTable[0x000734b0]=316;
lockingDurationsTable[0x00073430]=315;
lockingDurationsTable[0x00073470]=311;*/
/* Window Size 45000*/

/* Window Size 5000*/
/*lockingDurationsTable[0xfffffae8]=228;
lockingDurationsTable[0xfffffb10]=118;
lockingDurationsTable[0x00073340]=54;
lockingDurationsTable[0x000733b0]=53;
lockingDurationsTable[0x000733d0]=55;
lockingDurationsTable[0x000734b0]=53;
lockingDurationsTable[0x00073430]=54;
lockingDurationsTable[0x00073470]=48;*/
/* Window Size 5000*/

/*Window Size 100*/
/*lockingDurationsTable[0xfffffae8]=45;
lockingDurationsTable[0xfffffb10]=9;
lockingDurationsTable[0x00073340]=9;
lockingDurationsTable[0x000733b0]=5;
lockingDurationsTable[0x000733d0]=7;
lockingDurationsTable[0x000734b0]=5;
lockingDurationsTable[0x00073430]=5;
lockingDurationsTable[0x00073470]=4;*/
/*Window Size 100*/

/*Window Size 500*/
/*lockingDurationsTable[0xfffffae8]=60;
lockingDurationsTable[0xfffffb10]=18;
lockingDurationsTable[0x00073340]=22;
lockingDurationsTable[0x000733b0]=11;
lockingDurationsTable[0x000733d0]=11;
lockingDurationsTable[0x000734b0]=10;
lockingDurationsTable[0x00073430]=10;
lockingDurationsTable[0x00073470]=6;*/

/*fft Window size 100*/
/*lockingDurationsTable[0xfffffc9c]=28;
lockingDurationsTable[0xfffffd6c]=19;
lockingDurationsTable[0xfffffd0c]=5;
lockingDurationsTable[0xfffffcdc]=3;
lockingDurationsTable[0x0004d278]=2;
lockingDurationsTable[0x00070f84]=3;
lockingDurationsTable[0x00070fc4]=3;
lockingDurationsTable[0x00071004]=3;*/
/*fft Window size 100*/

/*Window Size 500*/
    /*lockingDurationsTable[0x6e230]=21234;
    lockingDurationsTable[0x70cac]=19523;
    lockingDurationsTable[0x6e258]=19414;
    lockingDurationsTable[0xfffff590]=19391;
    lockingDurationsTable[0xfffff568]=17327;
    lockingDurationsTable[0x6e308]=13502;
    lockingDurationsTable[0x697d0]=10696;
    lockingDurationsTable[0x6e2f0]=5588;*/

    /* #######################All functions including printfs and memcopy ####################*/
    /*lockingDurationsTable[0x6e230]=21234;
    lockingDurationsTable[0x70cac]=19523;
    lockingDurationsTable[0x6e258]=19414;
    lockingDurationsTable[0xfffff590]=19391;
    lockingDurationsTable[0xfffff568]=17327;
    lockingDurationsTable[0xfffff4a8]=17072;
    lockingDurationsTable[0xfffff478]=16226;
    lockingDurationsTable[0x6e308]=13502;
    lockingDurationsTable[0xfffff558]=10998;
    lockingDurationsTable[0x697d0]=10696;
    lockingDurationsTable[0xfffff9df]=8100;
    lockingDurationsTable[0xfffff598]=6803;
    lockingDurationsTable[0xfffff530]=6258;
    lockingDurationsTable[0xfffffa00]=5713;
    lockingDurationsTable[0x6e2f0]=5588;
    lockingDurationsTable[0xfffff6e0]=5484;*/
    /* #######################All functions including printfs and memcopy ####################*/

/*lockingDurationsTable[0x0006e230]=8278;
lockingDurationsTable[0x00070cac]=7648;
lockingDurationsTable[0x0006e258]=7442;
lockingDurationsTable[0xfffff590]=9524;
lockingDurationsTable[0xfffff568]=8459;
lockingDurationsTable[0xfffff4a8]=8427;
lockingDurationsTable[0xfffff478]=8143;
lockingDurationsTable[0x0006e308]=5073;
lockingDurationsTable[0xfffff558]=5234;
lockingDurationsTable[0x000697d0]=4071;
lockingDurationsTable[0xfffff9df]=4073;
lockingDurationsTable[0xfffff598]=2754;
lockingDurationsTable[0xfffff530]=2863;
lockingDurationsTable[0xfffffa00]=2822;
lockingDurationsTable[0x0006e2f0]=2178;
lockingDurationsTable[0xfffff6e0]=3158;*/


/*lockingDurationsTable[0x0006e230]=8278;
lockingDurationsTable[0x00070cac]=7648;
lockingDurationsTable[0x0006e258]=7442;
lockingDurationsTable[0xfffff590]=9524;
lockingDurationsTable[0xfffff568]=8459;
lockingDurationsTable[0xfffff4a8]=8427;
lockingDurationsTable[0xfffff4a0]=8427;
lockingDurationsTable[0xfffff490]=8285;*/

/*Minver Dataset*/
/*lockingDurationsTable[0xfffffd44]=218;
lockingDurationsTable[0xfffffd4c]=235;
lockingDurationsTable[0xfffffd38]=118;
lockingDurationsTable[0xfffff53c]=84;
lockingDurationsTable[0xfffffcf8]=24;
lockingDurationsTable[0xfffffcf0]=23;
lockingDurationsTable[0x0006d018]=25;
lockingDurationsTable[0x0006d020]=23;
lockingDurationsTable[0x0006d040]=21;
lockingDurationsTable[0xfffffd88]=17;
lockingDurationsTable[0xfffffd80]=17;
lockingDurationsTable[0x0006ec90]=3;
lockingDurationsTable[0xffffecb8]=11;
lockingDurationsTable[0xfffffc58]=6;
lockingDurationsTable[0x0006fdc0]=0;
lockingDurationsTable[0xfffffbd0]=6;*/

    
    // list benchmark 1 iter 
    
    /*lockingDurationsTable[0x73340]=712;
    lockingDurationsTable[0x734f0]=681;
    lockingDurationsTable[0x733b0]=671;
    lockingDurationsTable[0x73510]=670;
    lockingDurationsTable[0x733d0]=667;
    lockingDurationsTable[0x73480]=664;
    lockingDurationsTable[0x73420]=661;
    lockingDurationsTable[0x73470]=654;*/
    
    // list benchmark 5 iter 
    /*lockingDurationsTable[0x73340]=3376;
    lockingDurationsTable[0x734f0]=3338;
    lockingDurationsTable[0x733b0]=3296;
    lockingDurationsTable[0x73510]=3281;
    lockingDurationsTable[0x733d0]=3267;
    lockingDurationsTable[0x73480]=3245;
    lockingDurationsTable[0x73430]=3240;
    lockingDurationsTable[0x73470]=3213;*/


/*lockingDurationsTable[0xfffffae8]=3291;
lockingDurationsTable[0xfffffb10]=1889;
lockingDurationsTable[0x73340]=682;
lockingDurationsTable[0x733b0]=668;
lockingDurationsTable[0x733d0]=664;
lockingDurationsTable[0x73480]=661;
lockingDurationsTable[0x73420]=658;
lockingDurationsTable[0x73460]=651;*/

    /*lockingDurationsTable[0xfffffae8]=3288;
    lockingDurationsTable[0xfffffb10]=1888;
    lockingDurationsTable[0x73340]=667;
    lockingDurationsTable[0x733b0]=657;
    lockingDurationsTable[0x733d0]=651;
    lockingDurationsTable[0x734b0]=647;
    lockingDurationsTable[0x73430]=646;
    lockingDurationsTable[0x73470]=640;*/
    //printf("Address is: %d",pkt->req->getVaddr());
    /*lockingDurationsTable[0xfffffae8]=32883;
    lockingDurationsTable[0xfffffb10]=18881;
    lockingDurationsTable[0x7367e]=1081;
    lockingDurationsTable[0x73680]=10810;
    lockingDurationsTable[0x735dc]=9610;
    lockingDurationsTable[0x73600]=9610;
    lockingDurationsTable[0x73740]=3600;
    lockingDurationsTable[0x73780]=2800;*/

/*lockingDurationsTable[0xfffffae8]=411;
lockingDurationsTable[0xfffffb10]=228;
lockingDurationsTable[0xfffffb18]=108;
lockingDurationsTable[0xfffffaf4]=80;*/

/*lockingDurationsTable[0xfffffd18]=42;
lockingDurationsTable[0xfffffcd8]=38;
lockingDurationsTable[0xfffffbe8]=45;
lockingDurationsTable[0xfffffc18]=34;
lockingDurationsTable[0xfffffb98]=45;
lockingDurationsTable[0xfffffc60]=33;
lockingDurationsTable[0xfffffb48]=44;
lockingDurationsTable[0xfffffcb0]=33;*/
/*
lockingDurationsTable[0xfffffc1c]=92;
lockingDurationsTable[0xfffffc60]=37;
lockingDurationsTable[0xfffffd28]=55;
lockingDurationsTable[0xfffffc88]=18;
lockingDurationsTable[0xfffffd6c]=149;
lockingDurationsTable[0xfffffcc8]=36;*/

/*lockingDurationsTable[0xfffffc1c]=148;
lockingDurationsTable[0xfffffc60]=79;
lockingDurationsTable[0xfffffd28]=72;
lockingDurationsTable[0xfffffc88]=34;
lockingDurationsTable[0xfffffd6c]=325;
lockingDurationsTable[0xfffffcc8]=76;
lockingDurationsTable[0x000705b0]=2;
lockingDurationsTable[0x000705e8]=3;*/


/*Dataset for EPIC 2kb 2 ways 1line per set*/
/*lockingDurationsTable[0xfffffbf0]=25;
lockingDurationsTable[0xfffffc08]=25;
lockingDurationsTable[0xfffffb84]=62;
lockingDurationsTable[0xfffffb50]=24;
lockingDurationsTable[0xfffffd6c]=115;
lockingDurationsTable[0xfffffcc8]=6;
lockingDurationsTable[0xfffffcb0]=5;
lockingDurationsTable[0x0004de60]=23;
lockingDurationsTable[0xfffffc78]=55;*/
/*Dataset for EPIC 2kb 2 ways 1line per set*/

/*Anagram data*/
/*lockingDurationsTable[0xfffffcc8]=95;
lockingDurationsTable[0xfffffcb8]=67;
lockingDurationsTable[0xfffffd2c]=126;
lockingDurationsTable[0xfffffd40]=53;
lockingDurationsTable[0xfffffc50]=100;
lockingDurationsTable[0x00077b88]=25;
lockingDurationsTable[0xfffffc38]=45;
lockingDurationsTable[0x000779d8]=4;*/

/*Locking for SHA 256B 4 ways*/
/*lockingDurationsTable[0xffffdcec]=120;
lockingDurationsTable[0xffffdd1c]=59;
lockingDurationsTable[0xfffffd6c]=64;*/

/*lockingDurationsTable[0x52000]=3;

lockingDurationsTable[0x7f000]=50;

lockingDurationsTable[0x9a000]=50;

lockingDurationsTable[0x74000]=50;

lockingDurationsTable[0x96000]=50;

lockingDurationsTable[0x84000]=50;

lockingDurationsTable[0x95000]=50;

lockingDurationsTable[0x8e000]=50;

lockingDurationsTable[0x62000]=6;

lockingDurationsTable[0x99000]=50;

lockingDurationsTable[0x9b000]=50;

lockingDurationsTable[0x81000]=50;

lockingDurationsTable[0x94000]=50;

lockingDurationsTable[0x0]=8;

lockingDurationsTable[0x71000]=50;*/



/*Disparity 4 ways 50 window size*/

/*lockingDurationsTable[0x96000]=50;

lockingDurationsTable[0x95000]=50;

lockingDurationsTable[0x73000]=33;

lockingDurationsTable[0x74000]=50;

lockingDurationsTable[0x81000]=50;

lockingDurationsTable[0x84000]=50;

lockingDurationsTable[0x99000]=50;

lockingDurationsTable[0x0]=8;

lockingDurationsTable[0x71000]=50;

lockingDurationsTable[0x9b000]=50;

lockingDurationsTable[0x70000]=4;

lockingDurationsTable[0x76000]=1;

lockingDurationsTable[0x8e000]=50;

lockingDurationsTable[0x6e000]=13;

lockingDurationsTable[0x93000]=50;

lockingDurationsTable[0x6f000]=13;

lockingDurationsTable[0x8d000]=50;

lockingDurationsTable[0x9a000]=50;

lockingDurationsTable[0x8b000]=50;

lockingDurationsTable[0x94000]=50;*/


/*Tracking window size 50*/
/*lockingDurationsTable[0x78000]=50;

lockingDurationsTable[0x89000]=50;

lockingDurationsTable[0x8f000]=22;

lockingDurationsTable[0x74000]=32;

lockingDurationsTable[0x77000]=1;

lockingDurationsTable[0x91000]=12;

lockingDurationsTable[0x54000]=2;

lockingDurationsTable[0x83000]=50;

lockingDurationsTable[0x96000]=12;

lockingDurationsTable[0x53000]=3;

lockingDurationsTable[0x4f000]=2;

lockingDurationsTable[0x6d000]=17;

lockingDurationsTable[0x72000]=50;

lockingDurationsTable[0x5f000]=12;

lockingDurationsTable[0x90000]=22;

lockingDurationsTable[0x0]=8;*/


/**MSER Window size =50 sim 4kB 4 ways*/
/*lockingDurationsTable[0x7b000]=12;

lockingDurationsTable[0x4e000]=2;

lockingDurationsTable[0xc2000]=2;

lockingDurationsTable[0xa7000]=50;

lockingDurationsTable[0x73000]=33;

lockingDurationsTable[0x5e000]=2;

lockingDurationsTable[0xb3000]=50;

lockingDurationsTable[0x6f000]=12;

lockingDurationsTable[0x71000]=50;

lockingDurationsTable[0x74000]=50;

lockingDurationsTable[0x86000]=30;

lockingDurationsTable[0x8b000]=18;

lockingDurationsTable[0x76000]=1;

lockingDurationsTable[0x94000]=50;

lockingDurationsTable[0xb9000]=24;

lockingDurationsTable[0x52000]=3;

lockingDurationsTable[0x8e000]=12;

lockingDurationsTable[0xb7000]=25;

lockingDurationsTable[0x0]=8;*/


/**MSER Window size =50 cif 4kB 4 ways*/
/*lockingDurationsTable[0x390000]=10;

lockingDurationsTable[0x3dc000]=10;

lockingDurationsTable[0x373000]=50;

lockingDurationsTable[0x3ef000]=30;

lockingDurationsTable[0x3a6000]=10;

lockingDurationsTable[0x52000]=3;

lockingDurationsTable[0x77000]=4;

lockingDurationsTable[0x3ea000]=10;

lockingDurationsTable[0x394000]=10;

lockingDurationsTable[0x409000]=29;

lockingDurationsTable[0x3a2000]=10;

lockingDurationsTable[0x387000]=10;

lockingDurationsTable[0x38e000]=10;

lockingDurationsTable[0x172000]=14;

lockingDurationsTable[0x3cf000]=10;

lockingDurationsTable[0x499000]=30;

lockingDurationsTable[0x3e7000]=10;

lockingDurationsTable[0x459000]=23;

lockingDurationsTable[0x188000]=12;

lockingDurationsTable[0x62000]=7;

lockingDurationsTable[0x3cb000]=10;

lockingDurationsTable[0x3de000]=10;

lockingDurationsTable[0x4f000]=1;

lockingDurationsTable[0x14a000]=10;

lockingDurationsTable[0x15f000]=14;

lockingDurationsTable[0x1bc000]=14;

lockingDurationsTable[0x3e0000]=10;

lockingDurationsTable[0x3a8000]=10;

lockingDurationsTable[0x354000]=10;

lockingDurationsTable[0x73000]=33;

lockingDurationsTable[0x38a000]=50;

lockingDurationsTable[0x38c000]=10;

lockingDurationsTable[0x3c7000]=10;

lockingDurationsTable[0x35b000]=10;

lockingDurationsTable[0x3d3000]=10;

lockingDurationsTable[0x37a000]=10;

lockingDurationsTable[0x1e3000]=16;

lockingDurationsTable[0x363000]=10;

lockingDurationsTable[0x3c3000]=50;

lockingDurationsTable[0x76000]=50;

lockingDurationsTable[0x366000]=50;

lockingDurationsTable[0x3d2000]=10;

lockingDurationsTable[0x378000]=10;

lockingDurationsTable[0x352000]=10;

lockingDurationsTable[0x3e4000]=10;

lockingDurationsTable[0x382000]=10;

lockingDurationsTable[0x16b000]=16;

lockingDurationsTable[0x3d6000]=10;

lockingDurationsTable[0x3ee000]=10;

lockingDurationsTable[0x3d0000]=10;

lockingDurationsTable[0x356000]=10;

lockingDurationsTable[0x3a4000]=10;

lockingDurationsTable[0x393000]=50;

lockingDurationsTable[0x74000]=50;

lockingDurationsTable[0x3c6000]=10;

lockingDurationsTable[0x392000]=10;

lockingDurationsTable[0x3d7000]=10;

lockingDurationsTable[0x0]=8;

lockingDurationsTable[0x42d000]=30;

lockingDurationsTable[0x374000]=10;

lockingDurationsTable[0x370000]=10;

lockingDurationsTable[0x1c8000]=14;

lockingDurationsTable[0x376000]=10;

lockingDurationsTable[0x3d4000]=50;

lockingDurationsTable[0x36e000]=10;

lockingDurationsTable[0x3c2000]=10;

lockingDurationsTable[0x380000]=10;

lockingDurationsTable[0x3b0000]=10;

lockingDurationsTable[0x3a0000]=10;

lockingDurationsTable[0x13c000]=12;

lockingDurationsTable[0x71000]=50;

lockingDurationsTable[0x36c000]=10;

lockingDurationsTable[0x372000]=10;

lockingDurationsTable[0x3ae000]=10;

lockingDurationsTable[0x3e2000]=10;

lockingDurationsTable[0x3eb000]=10;

lockingDurationsTable[0x364000]=50;

lockingDurationsTable[0x3f1000]=26;

lockingDurationsTable[0x353000]=50;

lockingDurationsTable[0x37c000]=10;

lockingDurationsTable[0x3e6000]=10;

lockingDurationsTable[0x3da000]=10;

lockingDurationsTable[0x37e000]=10;

lockingDurationsTable[0x3ac000]=10;

lockingDurationsTable[0x1c2000]=8;

lockingDurationsTable[0x3aa000]=10;

lockingDurationsTable[0x3c4000]=10;

lockingDurationsTable[0x35f000]=10;

lockingDurationsTable[0x3df000]=50;

lockingDurationsTable[0x1cb000]=8;

lockingDurationsTable[0x3ca000]=10;

lockingDurationsTable[0x383000]=10;

lockingDurationsTable[0x5e000]=2;*/



/** disparity 8kB 2 ways Average durations**/
/*lockingDurationsTable[0x91000]=16;

lockingDurationsTable[0x98000]=22;

lockingDurationsTable[0x7f000]=23;

lockingDurationsTable[0x85000]=12;

lockingDurationsTable[0x53000]=1;

lockingDurationsTable[0x81000]=22;

lockingDurationsTable[0x52000]=3;

lockingDurationsTable[0x8d000]=25;

lockingDurationsTable[0x90000]=16;

lockingDurationsTable[0x6c000]=4;

lockingDurationsTable[0x5e000]=1;

lockingDurationsTable[0x9b000]=22;

lockingDurationsTable[0x71000]=33;

lockingDurationsTable[0x95000]=28;

lockingDurationsTable[0x92000]=17;

lockingDurationsTable[0x99000]=22;

lockingDurationsTable[0x94000]=28;

lockingDurationsTable[0x0]=7;

lockingDurationsTable[0x80000]=22;

lockingDurationsTable[0x9a000]=22;

lockingDurationsTable[0x8e000]=25;

lockingDurationsTable[0x96000]=28;

lockingDurationsTable[0x83000]=14;

lockingDurationsTable[0x64000]=1;

lockingDurationsTable[0x51000]=12;

lockingDurationsTable[0x70000]=1;*/

/* Tracking 8kB 2 ways*/
/*lockingDurationsTable[0x4e000]=1;

lockingDurationsTable[0x93000]=6;

lockingDurationsTable[0x70000]=6;

lockingDurationsTable[0x55000]=1;

lockingDurationsTable[0x96000]=10;

lockingDurationsTable[0x5f000]=3;

lockingDurationsTable[0x73000]=3;

lockingDurationsTable[0x50000]=1;

lockingDurationsTable[0x8d000]=8;

lockingDurationsTable[0x71000]=1;

lockingDurationsTable[0x65000]=1;

lockingDurationsTable[0x8b000]=15;

lockingDurationsTable[0x8f000]=11;

lockingDurationsTable[0x72000]=32;*/


/*MSER window size 50 8kB 2 ways*/
/*lockingDurationsTable[0x86000]=13;

lockingDurationsTable[0x0]=7;

lockingDurationsTable[0x4f000]=1;

lockingDurationsTable[0x54000]=1;

lockingDurationsTable[0x62000]=3;

lockingDurationsTable[0x65000]=2;

lockingDurationsTable[0x6d000]=9;

lockingDurationsTable[0x51000]=11;

lockingDurationsTable[0x70000]=1;

lockingDurationsTable[0x52000]=3;

lockingDurationsTable[0x50000]=1;

lockingDurationsTable[0x5e000]=2;

lockingDurationsTable[0x71000]=32;*/


/*lockingDurationsTable[0x75aa0]=3794;
lockingDurationsTable[0x75990]=17;
lockingDurationsTable[0x75994]=1;
lockingDurationsTable[0x759d8]=790;
lockingDurationsTable[0x75978]=1;
lockingDurationsTable[0x75970]=1;
lockingDurationsTable[0x86600]=32;
lockingDurationsTable[0x7e5e0]=790;
lockingDurationsTable[0x8a610]=1;
lockingDurationsTable[0xb0720]=3733;
lockingDurationsTable[0xb0724]=3733;
lockingDurationsTable[0x78444]=772;
lockingDurationsTable[0x78678]=9255;
lockingDurationsTable[0x78684]=1;
lockingDurationsTable[0x754e0]=194;
lockingDurationsTable[0x752d0]=288;*/

/*lockingDurationsTable[0x75aa0]=3794;
lockingDurationsTable[0x75990]=17;
lockingDurationsTable[0x75994]=1;
lockingDurationsTable[0x75998]=1;
lockingDurationsTable[0x759d8]=790;
lockingDurationsTable[0x75978]=1;
lockingDurationsTable[0x75970]=1;
lockingDurationsTable[0x75968]=1;
lockingDurationsTable[0x86600]=32;
lockingDurationsTable[0x7e5e0]=790;
lockingDurationsTable[0x825f0]=1;
lockingDurationsTable[0x8a610]=1;
lockingDurationsTable[0xb0720]=3733;
lockingDurationsTable[0xb0724]=3733;
lockingDurationsTable[0x78444]=772;
lockingDurationsTable[0x78678]=9255;
lockingDurationsTable[0xb1730]=1;
lockingDurationsTable[0x92630]=1;
lockingDurationsTable[0x78448]=772;
lockingDurationsTable[0x78684]=1;
lockingDurationsTable[0x6bcb0]=772;
lockingDurationsTable[0x754e0]=194;
lockingDurationsTable[0x752d0]=288;
lockingDurationsTable[0x756ec]=32;*/


/*lockingDurationsTable[0x71a08]=9926;
lockingDurationsTable[0x71560]=9926;
lockingDurationsTable[0x71a10]=9926;
lockingDurationsTable[0x755d0]=3081;
lockingDurationsTable[0x71568]=9926;
lockingDurationsTable[0x712f8]=2388;
lockingDurationsTable[0x712f0]=2388;
lockingDurationsTable[0x713f0]=3981;
lockingDurationsTable[0x6e7c8]=3981;
lockingDurationsTable[0x713c0]=2388;
lockingDurationsTable[0x71580]=9926;
lockingDurationsTable[0x72730]=2388;
lockingDurationsTable[0x10aecd0]=10535;
lockingDurationsTable[0x7dbf8]=31586;
lockingDurationsTable[0x71378]=2388;
lockingDurationsTable[0x75360]=796;
lockingDurationsTable[0x719b8]=796;
lockingDurationsTable[0x719b0]=796;
lockingDurationsTable[0x71400]=2388;
lockingDurationsTable[0x712b8]=2388;
lockingDurationsTable[0x71581]=9928;
lockingDurationsTable[0x71b60]=4777;
lockingDurationsTable[0x71a88]=796;
lockingDurationsTable[0x71b08]=796;
lockingDurationsTable[0x71af8]=796;
lockingDurationsTable[0x71b00]=796;
lockingDurationsTable[0x71ac0]=796;
lockingDurationsTable[0x75444]=796;
lockingDurationsTable[0x719d8]=796;
lockingDurationsTable[0x719e8]=796;
lockingDurationsTable[0x72720]=2388;
lockingDurationsTable[0x6e980]=2388;
lockingDurationsTable[0x71b68]=1;
lockingDurationsTable[0x10aecc0]=3081;
lockingDurationsTable[0x53d80]=1592;
lockingDurationsTable[0x71aa8]=796;
lockingDurationsTable[0x75448]=796;
lockingDurationsTable[0x6e948]=2388;
lockingDurationsTable[0x4d97c]=2388;
lockingDurationsTable[0x6e9c0]=2388;
lockingDurationsTable[0x5df58]=2388;
lockingDurationsTable[0x5df48]=2388;
lockingDurationsTable[0x71a40]=1;
lockingDurationsTable[0x53da2]=3981;
lockingDurationsTable[0x71380]=796;
lockingDurationsTable[0x713a0]=796;
lockingDurationsTable[0x6e9d8]=796;
lockingDurationsTable[0x4cfc1]=1;
lockingDurationsTable[0x4cfba]=1;
lockingDurationsTable[0x4cfbf]=1;
lockingDurationsTable[0x70328]=4964;
lockingDurationsTable[0x70410]=4964;
lockingDurationsTable[0x703c8]=4964;
lockingDurationsTable[0x70408]=4964;
lockingDurationsTable[0x71a68]=2;
lockingDurationsTable[0x75420]=796;
lockingDurationsTable[0x6e760]=796;
lockingDurationsTable[0x71338]=796;
lockingDurationsTable[0x71328]=796;
lockingDurationsTable[0x6e768]=796;
lockingDurationsTable[0x644f8]=796;
lockingDurationsTable[0x70340]=660;
lockingDurationsTable[0x4cfc3]=1;
lockingDurationsTable[0x10aead0]=1820;
lockingDurationsTable[0x10ae968]=369;
lockingDurationsTable[0x10ae788]=1;
lockingDurationsTable[0x10ae784]=1;
lockingDurationsTable[0x10ad020]=64;
lockingDurationsTable[0x10ad030]=1;
lockingDurationsTable[0x703a0]=4964;
lockingDurationsTable[0x10aeef0]=6;
lockingDurationsTable[0x10aeef8]=1;
lockingDurationsTable[0x10aef10]=31;
lockingDurationsTable[0x10aef18]=22;
lockingDurationsTable[0x10ad068]=1;
lockingDurationsTable[0x10aee78]=168;
lockingDurationsTable[0x10ad6d0]=293;
lockingDurationsTable[0x10aee70]=26;
lockingDurationsTable[0x10ad080]=1;
lockingDurationsTable[0x10ae6f8]=1;
lockingDurationsTable[0x10ad830]=6881;
lockingDurationsTable[0x10aee88]=1;
lockingDurationsTable[0x10ad6f8]=1;
lockingDurationsTable[0x10ad120]=53;
lockingDurationsTable[0x10aef50]=24;
lockingDurationsTable[0x10ad838]=4122;
lockingDurationsTable[0x10ad840]=8;
lockingDurationsTable[0x10ad288]=599;
lockingDurationsTable[0x10aee38]=557;
lockingDurationsTable[0x10aeea8]=3692;
lockingDurationsTable[0x10aee00]=1;
lockingDurationsTable[0x10ad720]=1;
lockingDurationsTable[0x10aed00]=5;
lockingDurationsTable[0x10ad888]=1;
lockingDurationsTable[0x10ad890]=1;
lockingDurationsTable[0x10aed70]=805;
lockingDurationsTable[0x10aed78]=805;
lockingDurationsTable[0x10aed80]=884;
lockingDurationsTable[0x10aed88]=884;
lockingDurationsTable[0x10aeab8]=837;
lockingDurationsTable[0x10aea08]=369;
lockingDurationsTable[0x10b0f60]=6;
lockingDurationsTable[0x10ae4c8]=237;
lockingDurationsTable[0x10a7140]=274;
lockingDurationsTable[0x10b0aa0]=31;
lockingDurationsTable[0x10afbc0]=1;
lockingDurationsTable[0x10a7060]=4314;
lockingDurationsTable[0x10b0260]=1;
lockingDurationsTable[0x10a7700]=22;
lockingDurationsTable[0x10a7b80]=82;
lockingDurationsTable[0x10afcc0]=87;
lockingDurationsTable[0x10af1a0]=1;
lockingDurationsTable[0x10b02a0]=94;
lockingDurationsTable[0x10a77a0]=633;
lockingDurationsTable[0x10a70a0]=15;
lockingDurationsTable[0x10a7160]=5;
lockingDurationsTable[0x10a7480]=1;
lockingDurationsTable[0x10af900]=1;
lockingDurationsTable[0x10b0b60]=1;
lockingDurationsTable[0x10a75e0]=557;
lockingDurationsTable[0x10b0dc0]=6881;
lockingDurationsTable[0x10a7e40]=24;
lockingDurationsTable[0x10b04a0]=1;
lockingDurationsTable[0x10b1230]=1;
lockingDurationsTable[0x10af260]=15;
lockingDurationsTable[0x10b0de0]=4122;
lockingDurationsTable[0x10b03c0]=1;
lockingDurationsTable[0x10b0800]=1;
lockingDurationsTable[0x10a7c50]=1;
lockingDurationsTable[0x10b0840]=293;
lockingDurationsTable[0x10b05c0]=1;
lockingDurationsTable[0x10af190]=1;
lockingDurationsTable[0x10a7260]=26;
lockingDurationsTable[0x10a7920]=805;
lockingDurationsTable[0x10af950]=1;
lockingDurationsTable[0x10a7d70]=1;
lockingDurationsTable[0x10a7c20]=221;
lockingDurationsTable[0x10a70f0]=1;
lockingDurationsTable[0x10b0670]=1;
lockingDurationsTable[0x10a7df0]=1;
lockingDurationsTable[0x10ae988]=369;
lockingDurationsTable[0x10aeac0]=599;
lockingDurationsTable[0x10aea68]=369;
lockingDurationsTable[0x10aea78]=369;
lockingDurationsTable[0x10aeb18]=599;
lockingDurationsTable[0x10aeb74]=837;
lockingDurationsTable[0x10aec50]=8;
lockingDurationsTable[0x10aec58]=8;
lockingDurationsTable[0x10aec88]=8;
lockingDurationsTable[0x10aec80]=8;
lockingDurationsTable[0x10aeb10]=377;
lockingDurationsTable[0x6ebf0]=8;
lockingDurationsTable[0x10aebf8]=8;
lockingDurationsTable[0x10aebb8]=8;
lockingDurationsTable[0x10aebb0]=8;
lockingDurationsTable[0x10aec38]=8;
lockingDurationsTable[0x10aec30]=8;
lockingDurationsTable[0x78028]=8;
lockingDurationsTable[0x31d530]=1;
lockingDurationsTable[0x3254a8]=1;
lockingDurationsTable[0x5e6320]=1;
lockingDurationsTable[0x70390]=8;
lockingDurationsTable[0x78020]=8;
lockingDurationsTable[0x8b780]=1;
lockingDurationsTable[0x146650]=29639;
lockingDurationsTable[0x6e7d0]=23;
lockingDurationsTable[0x44c1d8]=1;
lockingDurationsTable[0x75dde]=1;
lockingDurationsTable[0x67fc0]=1;
lockingDurationsTable[0x78078]=31586;
lockingDurationsTable[0x785c0]=481;
lockingDurationsTable[0xb9dd40]=1;
lockingDurationsTable[0x7a4698]=1;
lockingDurationsTable[0xb9dd30]=1;
lockingDurationsTable[0xb9dd38]=1;
lockingDurationsTable[0x7a4690]=1;
lockingDurationsTable[0x6ed34]=1;
lockingDurationsTable[0x6d698]=2;
lockingDurationsTable[0x473e28]=796;
lockingDurationsTable[0x42e120]=796;
lockingDurationsTable[0x71cf8]=796;
lockingDurationsTable[0x76598]=1;
lockingDurationsTable[0x78618]=16;
lockingDurationsTable[0x78610]=8;
lockingDurationsTable[0x78090]=8;
lockingDurationsTable[0x7f1e8]=4964;
lockingDurationsTable[0x740d8]=1;
lockingDurationsTable[0x78098]=8;
lockingDurationsTable[0x6d6bc]=1;
lockingDurationsTable[0x701d0]=257;
lockingDurationsTable[0x7f220]=4964;
lockingDurationsTable[0x70258]=1;
lockingDurationsTable[0x74238]=7601;
lockingDurationsTable[0x67fd8]=1;
lockingDurationsTable[0x6d648]=1;*/


/*lockingDurationsTable[0x71a08]=9926;
lockingDurationsTable[0x71560]=9926;
lockingDurationsTable[0x71a10]=9926;
lockingDurationsTable[0x755d0]=3081;
lockingDurationsTable[0x71568]=9926;
lockingDurationsTable[0x712f8]=2388;
lockingDurationsTable[0x712f0]=2388;
lockingDurationsTable[0x713f0]=3981;
lockingDurationsTable[0x6e7c8]=3981;
lockingDurationsTable[0x713c0]=2388;
lockingDurationsTable[0x71580]=9926;
lockingDurationsTable[0x72730]=2388;
lockingDurationsTable[0x10aecd0]=10535;
lockingDurationsTable[0x71378]=2388;
lockingDurationsTable[0x75360]=796;
lockingDurationsTable[0x719b8]=796;
lockingDurationsTable[0x719b0]=796;
lockingDurationsTable[0x71400]=2388;
lockingDurationsTable[0x712b8]=2388;
lockingDurationsTable[0x71581]=9928;
lockingDurationsTable[0x71a88]=796;
lockingDurationsTable[0x71b08]=796;
lockingDurationsTable[0x71b00]=796;
lockingDurationsTable[0x75444]=796;
lockingDurationsTable[0x719d8]=796;
lockingDurationsTable[0x719e8]=796;
lockingDurationsTable[0x72720]=2388;
lockingDurationsTable[0x10aecc0]=3081;
lockingDurationsTable[0x75448]=796;
lockingDurationsTable[0x6e948]=2388;
lockingDurationsTable[0x4d97c]=2388;
lockingDurationsTable[0x5df58]=2388;
lockingDurationsTable[0x5df48]=2388;
lockingDurationsTable[0x71a40]=1;
lockingDurationsTable[0x71380]=796;
lockingDurationsTable[0x713a0]=796;
lockingDurationsTable[0x4cfc1]=1;
lockingDurationsTable[0x4cfba]=1;
lockingDurationsTable[0x4cfbf]=1;
lockingDurationsTable[0x70410]=4964;
lockingDurationsTable[0x71a68]=2;
lockingDurationsTable[0x10ad020]=64;
lockingDurationsTable[0x10ad030]=1;
lockingDurationsTable[0x10aeef0]=6;
lockingDurationsTable[0x10aeef8]=1;
lockingDurationsTable[0x10ad068]=1;
lockingDurationsTable[0x10aee78]=168;
lockingDurationsTable[0x10aee70]=26;
lockingDurationsTable[0x10ad080]=1;
lockingDurationsTable[0x10aee88]=1;
lockingDurationsTable[0x10ad120]=53;
lockingDurationsTable[0x10ad840]=8;
lockingDurationsTable[0x10aee38]=557;
lockingDurationsTable[0x10aeea8]=3692;
lockingDurationsTable[0x10aee00]=1;
lockingDurationsTable[0x10aed00]=5;
lockingDurationsTable[0x10ad888]=1;
lockingDurationsTable[0x10a7480]=1;
lockingDurationsTable[0x10af900]=1;
lockingDurationsTable[0x10a75e0]=557;
lockingDurationsTable[0x10b04a0]=1;
lockingDurationsTable[0x10a70f0]=1;
lockingDurationsTable[0x31d530]=1;
lockingDurationsTable[0x740d8]=1;*/

/*lockingDurationsTable[0x71a08]=9926;
lockingDurationsTable[0x71560]=9926;
lockingDurationsTable[0x71a10]=9926;
lockingDurationsTable[0x755d0]=3081;
lockingDurationsTable[0x71568]=9926;
lockingDurationsTable[0x712f8]=2388;
lockingDurationsTable[0x712f0]=2388;
lockingDurationsTable[0x713f0]=3981;
lockingDurationsTable[0x6e7c8]=3981;
lockingDurationsTable[0x713c0]=2388;
lockingDurationsTable[0x71580]=9926;
lockingDurationsTable[0x72730]=2388;
lockingDurationsTable[0x10aecd0]=10535;
lockingDurationsTable[0x71378]=2388;
lockingDurationsTable[0x75360]=796;
lockingDurationsTable[0x719b8]=796;
lockingDurationsTable[0x719b0]=796;
lockingDurationsTable[0x71400]=2388;
lockingDurationsTable[0x712b8]=2388;
lockingDurationsTable[0x71581]=9928;
lockingDurationsTable[0x71a88]=796;
lockingDurationsTable[0x71b08]=796;
lockingDurationsTable[0x71b00]=796;
lockingDurationsTable[0x75444]=796;
lockingDurationsTable[0x719d8]=796;
lockingDurationsTable[0x719e8]=796;
lockingDurationsTable[0x72720]=2388;
lockingDurationsTable[0x10aecc0]=3081;
lockingDurationsTable[0x75448]=796;
lockingDurationsTable[0x6e948]=2388;
lockingDurationsTable[0x4d97c]=2388;
lockingDurationsTable[0x5df58]=2388;
lockingDurationsTable[0x5df48]=2388;
lockingDurationsTable[0x71380]=796;
lockingDurationsTable[0x713a0]=796;
lockingDurationsTable[0x70410]=4964;
lockingDurationsTable[0x10ad020]=64;
lockingDurationsTable[0x10aee78]=168;
lockingDurationsTable[0x10aee70]=26;
lockingDurationsTable[0x10ad120]=53;
lockingDurationsTable[0x10ad840]=8;
lockingDurationsTable[0x10aee38]=557;
lockingDurationsTable[0x10aeea8]=3692;
lockingDurationsTable[0x10a75e0]=557;*/

lockingDurationsTable[0x6140]=3561;
lockingDurationsTable[0xde00]=1;
lockingDurationsTable[0x5800]=4246;
lockingDurationsTable[0x12700]=439;
lockingDurationsTable[0x14c00]=26;
lockingDurationsTable[0x4b00]=59;
lockingDurationsTable[0xdb80]=3150;
lockingDurationsTable[0xd7c0]=1;
lockingDurationsTable[0xde40]=1;
lockingDurationsTable[0x20200]=3560;
lockingDurationsTable[0x6180]=3013;
lockingDurationsTable[0x12680]=440;
lockingDurationsTable[0x5540]=2465;
lockingDurationsTable[0x5580]=2876;
lockingDurationsTable[0x15d80]=1;
lockingDurationsTable[0x5700]=1780;
lockingDurationsTable[0xd800]=1;
lockingDurationsTable[0xdb40]=3972;
lockingDurationsTable[0x55c0]=3561;
lockingDurationsTable[0x5740]=1232;
lockingDurationsTable[0x17880]=109;
lockingDurationsTable[0x126c0]=547;
lockingDurationsTable[0x56c0]=2191;
lockingDurationsTable[0xddc0]=1;
lockingDurationsTable[0x57c0]=3424;
lockingDurationsTable[0xde80]=1;
/*512B 4ways*/
//120653600000
//120665474000


//118513878000
    /******************** Locking logic ********************************************/ 

        if (is_l1_cache_locking_full_context && !is_l1_cache_locking)
        {
            int mask = ~(0x3F);
            if(blk)
            {
                //cache_accesses+=1;
                //int set_number = blk->getSet();
                //Addr phys_addr_blk = int(pkt->getAddr());
               // if (interval_locked_map.find(cache_accesses) != interval_locked_map.end()) {
                    //const auto& data = interval_locked_map[cache_accesses];
                    //const auto& innerVector = std::get<0>(data);
                 //   Addr phys_addr_blk = int(pkt->getAddr());

                /*           if (std::find(innerVector.begin(), innerVector.end(), phys_addr_blk) != innerVector.end()) {
                    printf("dadadad");
                }*/
                //auto it = interval_locked_map.find(cache_accesses);

                if(cache_accesses == bottom_cache_accesses_interval)
                {
                    //printf("END OF INTERVAL FREEING ALL ADDRESSES!\n");
                    for (auto const& block : locked_blocks)
                    {
                      //      printf("FREEING: 0x%x!\n",block.first);
                            locked_blocks[block.first]->freeLock();
                            locked_ways_per_set[locked_blocks[block.first]->getSet()] = 0;
                            //locked_blocks.erase(block.first);
                    }
                    top_cache_accesses_interval = 0;
                    bottom_cache_accesses_interval = 0;
                }
                
                if (interval_locked_map.find(cache_accesses) != interval_locked_map.end())
                {
                    const auto& data = interval_locked_map[cache_accesses];
                    bottom_cache_accesses_interval = std::get<1>(data);
                    top_cache_accesses_interval = cache_accesses;
                    

                }

                if (cache_accesses < bottom_cache_accesses_interval)
                {
                    
                    const auto& data = interval_locked_map[top_cache_accesses_interval];
                    const auto& innerVector = std::get<0>(data);
                    const auto& end_address = std::get<1>(data);

                    Addr phys_addr_blk = int(pkt->getAddr()&mask);
                     
                    if (std::find(innerVector.begin(), innerVector.end(), phys_addr_blk) != innerVector.end()) 
                     {
                        //printf("FOUND!!!\n");
                        if(!blk->getLock())
                        {   
                            int set_number = blk->getSet();
                            if ((locked_ways_per_set.find(set_number) == locked_ways_per_set.end()) || locked_ways_per_set[set_number]<2)
                            {
                                if (locked_ways_per_set.find(set_number) == locked_ways_per_set.end())
                                    locked_ways_per_set[set_number] = 1;
                                else
                                    locked_ways_per_set[set_number] += 1;
                                //printf("SIZE OF MAP IS: %x\n",locked_blocks.size());
                                //printf("Locked addresses per set: %d\n",locked_ways_per_set[set_number]);
                                blk->setLock();

                                locked_blocks[phys_addr_blk] = blk;
                            }
                            
                        }
                     }

                }
                //printf("Bottom Accesses is: %d\n",bottom_cache_accesses_interval);
                //printf("Cache Accesses is: %d\n",cache_accesses);
                

            cache_accesses+=1;
            }

        
        }
        /*if (is_l1_cache_locking_full_context && !is_l1_cache_locking)
        {

            printf("Memory access at PC: 0x%x\n",pkt->req->getPC());
            printf("Cache Accesses: %d\n",cache_accesses);
            if(blk)
            {
                //cache_accesses+=1;
                int set_number = blk->getSet();
                Addr phys_addr_blk = int(pkt->getAddr());

                if (lockingDurationsTable.find(phys_addr_blk) != lockingDurationsTable.end())
                {
                    if(!blk->getLock())
                    {
                        blk->setLock();
                        locked_blocks[phys_addr_blk] = blk;
                        printf("SIZE OF MAP IS: %x\n",locked_blocks.size());
                    }
                }
            }
        }*/

        if(is_l1_cache_locking)
        {
            if(blk)
            {   
                int set_number = blk->getSet();
                unsigned int mask =  ~(0x3F);
                //unsigned int mask =  -1;
                Addr virt_addr_blk = int(pkt->getAddr());
                //printf("Address is: 0x%x\n",virt_addr_blk);
                for (auto const& block : lockingDurationCounters)
                {
                    // If block is not locked skip it
                    if(lockingDurationCounters[block.first] == 0)
                    {
                        continue;
                    }

                    else
                    {
                        // Decrement locking counter 
                        //printf("DECREMENTING LOCK! FOR 0x%x LOCK COUNTER IS AT %d\n",block.first,lockingDurationCounters[block.first]);   
                        if(!is_l1_cache_locking_full_context)
                        {
                            if (locked_blocks[block.first]->getSet() == set_number)
                            {
                                lockingDurationCounters[block.first] = lockingDurationCounters[block.first] - 1;
                                //printf("Decrementing Lock for 0x%x from %d to %d\n",block.first,lockingDurationCounters[block.first]+1,lockingDurationCounters[block.first]);
                            }
                            

                            if(lockingDurationCounters[block.first] == 0)
                            {
                                // Free the lock
                                printf("FREEING LOCK FOR ADDRESS 0x%x!\n",block.first);
                                locked_blocks[block.first]->freeLock();
                                //locked_blocks.erase(block.first);

                                locked_ways_per_set[blk->getSet()] -= 1;
                                printf("Number of ways locked per set is %d!\n",locked_ways_per_set[blk->getSet()]);
                            }

                            // If new incmoming page has a greater age old locked blocks can be evicted
                            /*if (lockingDurationsTable[virt_addr_blk&mask]>lockingDurationCounters[block.first] && (locked_blocks[block.first]->getSet() == blk->getSet())&&((locked_ways_per_set[blk->getSet()] == 0)||locked_ways_per_set.find(blk->getSet())== locked_ways_per_set.end()))
                            {
                                locked_blocks[block.first]->freeLock();
                                locked_ways_per_set[blk->getSet()] -= 1;
                                //blk->setLock();

                                // Add block to the locked list
                                //locked_blocks[virt_addr_blk] = blk;
                        
                                // Update counters table to start counting the duration of locking
                                //lockingDurationCounters[virt_addr_blk] = lockingDurationsTable[virt_addr_blk & mask];

                            }*/
                        }

                    }


                }
            }
            

            if(blk)
            {
             
                //Addr virt_addr_blk = int(pkt->req->getVaddr());
                unsigned int mask =  ~(0x3F);
                //Addr virt_addr_blk = int(pkt->getAddr()&mask);
                //unsigned int mask = -1;
                Addr virt_addr_blk = int(pkt->getAddr());

                //if(locked_ways_per_set.find(blk->getSet()) != locked_ways_per_set.end())
                //    printf("Number of locked ways: %d\n",locked_ways_per_set[blk->getSet()]);

                //printf("Address is: 0x%x\n",virt_addr_blk);
                //printf("Set is: %x\n",blk->getSet());

                // Checking if block exists in dictionary and locking duration is greater than 0
                if (lockingDurationsTable.find(virt_addr_blk & mask) != lockingDurationsTable.end() && lockingDurationsTable[virt_addr_blk & mask] >= 0)
                {
                    if(!blk->getLock())
                    {
                        
                        //printf("ACQUIRING LOCK FOR ADDRESS 0x%x!\n",pkt->req->getVaddr());
                        // Lock the block

                        bool locked_set_flag = 0;
                        int set_number = blk->getSet();
                        /*for (const auto& page:locked_blocks)
                        {
                            if (locked_blocks[page.first]->getSet() == set_number)
                            {
                                //printf("MATCHEDDDD!!!!!!!!!!!\n");
                                locked_set_flag = 1;
                                break;
                            }
                            locked_set_flag = 0;
                        }*/


                        if ((locked_ways_per_set.find(set_number) == locked_ways_per_set.end()) || locked_ways_per_set[set_number]<3)
                        {
                            //printf("LIST EMPTY WE CAN LOCK!\n");
                            //printf("SIZE OF MAP IS: %x\n",locked_blocks.size());

                            if (locked_ways_per_set.find(set_number) == locked_ways_per_set.end())
                                locked_ways_per_set[set_number] = 1;
                            else
                                locked_ways_per_set[set_number] += 1;
                            
                            if(locked_ways_per_set[set_number] ==2 || locked_ways_per_set[set_number] ==3)
                            {
                                printf("SET IS ALMOST FULL!!!!!\n");

                            }

                            
                            blk->setLock();

                            // Add block to the locked list
                            locked_blocks[virt_addr_blk] = blk;
                        
                            // Update counters table to start counting the duration of locking
                            printf("ACQUIRING LOCK FOR ADDRESS 0x%x!\n",pkt->getAddr());
                            //printf("THIS IS HOW THE ADDRESS APPEARS TO BE: 0x%x\n",virt_addr_blk & mask);
                            lockingDurationCounters[virt_addr_blk] = lockingDurationsTable[virt_addr_blk & mask];
                        }    
                            


                    }    
                    /*else
                    {
                        if(lockingDurationCounters[virt_addr_blk] > 0)
                        {   
                            printf("DECREMENTING DURATION!\n");
                            // Decrement the counter
                            lockingDurationCounters[virt_addr_blk] = lockingDurationCounters[virt_addr_blk] - 1;
                        }
                        else
                        {
                            printf("FREEING LOCK!\n");
                            // If counter reaches zero unlock the cache line 
                            blk->freeLock();
                        }
                        
                    }*/      
                }
            }
        }

        /******************** Locking logic ********************************************/ 




    DPRINTF(Cache, "%s for %s %s\n", __func__, pkt->print(),
            blk ? "hit " + blk->print() : "miss");

    if (pkt->req->isCacheMaintenance()) {
        // A cache maintenance operation is always forwarded to the
        // memory below even if the block is found in dirty state.

        // We defer any changes to the state of the block until we
        // create and mark as in service the mshr for the downstream
        // packet.

        // Calculate access latency on top of when the packet arrives. This
        // takes into account the bus delay.
        lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

        return false;
    }

    if (pkt->isEviction()) {
        // We check for presence of block in above caches before issuing
        // Writeback or CleanEvict to write buffer. Therefore the only
        // possible cases can be of a CleanEvict packet coming from above
        // encountering a Writeback generated in this cache peer cache and
        // waiting in the write buffer. Cases of upper level peer caches
        // generating CleanEvict and Writeback or simply CleanEvict and
        // CleanEvict almost simultaneously will be caught by snoops sent out
        // by crossbar.
        WriteQueueEntry *wb_entry = writeBuffer.findMatch(pkt->getAddr(),
                                                          pkt->isSecure());
        if (wb_entry) {
            assert(wb_entry->getNumTargets() == 1);
            PacketPtr wbPkt = wb_entry->getTarget()->pkt;
            assert(wbPkt->isWriteback());

            if (pkt->isCleanEviction()) {
                // The CleanEvict and WritebackClean snoops into other
                // peer caches of the same level while traversing the
                // crossbar. If a copy of the block is found, the
                // packet is deleted in the crossbar. Hence, none of
                // the other upper level caches connected to this
                // cache have the block, so we can clear the
                // BLOCK_CACHED flag in the Writeback if set and
                // discard the CleanEvict by returning true.
                wbPkt->clearBlockCached();

                // A clean evict does not need to access the data array
                lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

                return true;
            } else {
                assert(pkt->cmd == MemCmd::WritebackDirty);
                // Dirty writeback from above trumps our clean
                // writeback... discard here
                // Note: markInService will remove entry from writeback buffer.
                markInService(wb_entry);
                delete wbPkt;
            }
        }
    }

    // The critical latency part of a write depends only on the tag access
    if (pkt->isWrite()) {
        lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);
    }

    // Writeback handling is special case.  We can write the block into
    // the cache without having a writeable copy (or any copy at all).
    if (pkt->isWriteback()) {
        assert(blkSize == pkt->getSize());

        // we could get a clean writeback while we are having
        // outstanding accesses to a block, do the simple thing for
        // now and drop the clean writeback so that we do not upset
        // any ordering/decisions about ownership already taken
        if (pkt->cmd == MemCmd::WritebackClean &&
            mshrQueue.findMatch(pkt->getAddr(), pkt->isSecure())) {
            DPRINTF(Cache, "Clean writeback %#llx to block with MSHR, "
                    "dropping\n", pkt->getAddr());

            // A writeback searches for the block, then writes the data.
            // As the writeback is being dropped, the data is not touched,
            // and we just had to wait for the time to find a match in the
            // MSHR. As of now assume a mshr queue search takes as long as
            // a tag lookup for simplicity.
            return true;
        }

        const bool has_old_data = blk && blk->isValid();
        if (!blk) {
            // need to do a replacement
            blk = allocateBlock(pkt, writebacks);
            if (!blk) {
                // no replaceable block available: give up, fwd to next level.
                //cache_accesses+=1;
                incMissCount(pkt);
                return false;
            }

            blk->setCoherenceBits(CacheBlk::ReadableBit);
        } else if (compressor) {
            // This is an overwrite to an existing block, therefore we need
            // to check for data expansion (i.e., block was compressed with
            // a smaller size, and now it doesn't fit the entry anymore).
            // If that is the case we might need to evict blocks.
            if (!updateCompressionData(blk, pkt->getConstPtr<uint64_t>(),
                writebacks)) {
                invalidateBlock(blk);
                return false;
            }
        }

        // only mark the block dirty if we got a writeback command,
        // and leave it as is for a clean writeback
        if (pkt->cmd == MemCmd::WritebackDirty) {
            // TODO: the coherent cache can assert that the dirty bit is set
            blk->setCoherenceBits(CacheBlk::DirtyBit);
        }
        // if the packet does not have sharers, it is passing
        // writable, and we got the writeback in Modified or Exclusive
        // state, if not we are in the Owned or Shared state
        if (!pkt->hasSharers()) {
            blk->setCoherenceBits(CacheBlk::WritableBit);
        }
        // nothing else to do; writeback doesn't expect response
        assert(!pkt->needsResponse());

        updateBlockData(blk, pkt, has_old_data);
        DPRINTF(Cache, "%s new state is %s\n", __func__, blk->print());
        
        //cache_accesses+=1;
        incHitCount(pkt);

        // When the packet metadata arrives, the tag lookup will be done while
        // the payload is arriving. Then the block will be ready to access as
        // soon as the fill is done
        blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
            std::max(cyclesToTicks(tag_latency), (uint64_t)pkt->payloadDelay));

        return true;
    } else if (pkt->cmd == MemCmd::CleanEvict) {
        // A CleanEvict does not need to access the data array
        lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

        if (blk) {
            // Found the block in the tags, need to stop CleanEvict from
            // propagating further down the hierarchy. Returning true will
            // treat the CleanEvict like a satisfied write request and delete
            // it.
            return true;
        }
        // We didn't find the block here, propagate the CleanEvict further
        // down the memory hierarchy. Returning false will treat the CleanEvict
        // like a Writeback which could not find a replaceable block so has to
        // go to next level.
        return false;
    } else if (pkt->cmd == MemCmd::WriteClean) {
        // WriteClean handling is a special case. We can allocate a
        // block directly if it doesn't exist and we can update the
        // block immediately. The WriteClean transfers the ownership
        // of the block as well.
        assert(blkSize == pkt->getSize());

        const bool has_old_data = blk && blk->isValid();
        if (!blk) {
            if (pkt->writeThrough()) {
                // if this is a write through packet, we don't try to
                // allocate if the block is not present
                return false;
            } else {
                // a writeback that misses needs to allocate a new block
                blk = allocateBlock(pkt, writebacks);
                if (!blk) {
                    // no replaceable block available: give up, fwd to
                    // next level.
                    //cache_accesses+=1;
                    incMissCount(pkt);
                    return false;
                }

                blk->setCoherenceBits(CacheBlk::ReadableBit);
            }
        } else if (compressor) {
            // This is an overwrite to an existing block, therefore we need
            // to check for data expansion (i.e., block was compressed with
            // a smaller size, and now it doesn't fit the entry anymore).
            // If that is the case we might need to evict blocks.
            if (!updateCompressionData(blk, pkt->getConstPtr<uint64_t>(),
                writebacks)) {
                invalidateBlock(blk);
                return false;
            }
        }

        // at this point either this is a writeback or a write-through
        // write clean operation and the block is already in this
        // cache, we need to update the data and the block flags
        assert(blk);
        // TODO: the coherent cache can assert that the dirty bit is set
        if (!pkt->writeThrough()) {
            blk->setCoherenceBits(CacheBlk::DirtyBit);
        }
        // nothing else to do; writeback doesn't expect response
        assert(!pkt->needsResponse());

        updateBlockData(blk, pkt, has_old_data);
        DPRINTF(Cache, "%s new state is %s\n", __func__, blk->print());
        
        //cache_accesses+=1;
        incHitCount(pkt);

        // When the packet metadata arrives, the tag lookup will be done while
        // the payload is arriving. Then the block will be ready to access as
        // soon as the fill is done
        blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
            std::max(cyclesToTicks(tag_latency), (uint64_t)pkt->payloadDelay));

        // If this a write-through packet it will be sent to cache below
        return !pkt->writeThrough();
    } else if (blk && (pkt->needsWritable() ?
            blk->isSet(CacheBlk::WritableBit) :
            blk->isSet(CacheBlk::ReadableBit))) {
        // OK to satisfy access
        //
        //cache_accesses+=1;
        incHitCount(pkt);

        // Calculate access latency based on the need to access the data array
        if (pkt->isRead()) {
            lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);

            // When a block is compressed, it must first be decompressed
            // before being read. This adds to the access latency.
            if (compressor) {
                lat += compressor->getDecompressionLatency(blk);
            }
        } else {
            lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);
        }

        



        satisfyRequest(pkt, blk);
        maintainClusivity(pkt->fromCache(), blk);

        return true;
    }

    // Can't satisfy access normally... either no block (blk == nullptr)
    // or have block but need writable
    //cache_accesses+=1;
    incMissCount(pkt);

    lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);

    if (!blk && pkt->isLLSC() && pkt->isWrite()) {
        // complete miss on store conditional... just give up now
        pkt->req->setExtraData(0);
        return true;
    }

    return false;
}

void
BaseCache::maintainClusivity(bool from_cache, CacheBlk *blk)
{
    if (from_cache && blk && blk->isValid() &&
        !blk->isSet(CacheBlk::DirtyBit) && clusivity == enums::mostly_excl) {
        // if we have responded to a cache, and our block is still
        // valid, but not dirty, and this cache is mostly exclusive
        // with respect to the cache above, drop the block
        invalidateBlock(blk);
    }
}

CacheBlk*
BaseCache::handleFill(PacketPtr pkt, CacheBlk *blk, PacketList &writebacks,
                      bool allocate)
{
    assert(pkt->isResponse());
    Addr addr = pkt->getAddr();
    bool is_secure = pkt->isSecure();
    const bool has_old_data = blk && blk->isValid();
    const std::string old_state = (debug::Cache && blk) ? blk->print() : "";

    // When handling a fill, we should have no writes to this line.
    assert(addr == pkt->getBlockAddr(blkSize));
    assert(!writeBuffer.findMatch(addr, is_secure));

    if (!blk) {
        // better have read new data...
        assert(pkt->hasData() || pkt->cmd == MemCmd::InvalidateResp);

        // need to do a replacement if allocating, otherwise we stick
        // with the temporary storage
        blk = allocate ? allocateBlock(pkt, writebacks) : nullptr;

        if (!blk) {
            // No replaceable block or a mostly exclusive
            // cache... just use temporary storage to complete the
            // current request and then get rid of it
            blk = tempBlock;
            tempBlock->insert(addr, is_secure);
            DPRINTF(Cache, "using temp block for %#llx (%s)\n", addr,
                    is_secure ? "s" : "ns");
        }
    } else {
        // existing block... probably an upgrade
        // don't clear block status... if block is already dirty we
        // don't want to lose that
    }

    // Block is guaranteed to be valid at this point
    assert(blk->isValid());
    assert(blk->isSecure() == is_secure);
    assert(regenerateBlkAddr(blk) == addr);

    blk->setCoherenceBits(CacheBlk::ReadableBit);

    // sanity check for whole-line writes, which should always be
    // marked as writable as part of the fill, and then later marked
    // dirty as part of satisfyRequest
    if (pkt->cmd == MemCmd::InvalidateResp) {
        assert(!pkt->hasSharers());
    }

    // here we deal with setting the appropriate state of the line,
    // and we start by looking at the hasSharers flag, and ignore the
    // cacheResponding flag (normally signalling dirty data) if the
    // packet has sharers, thus the line is never allocated as Owned
    // (dirty but not writable), and always ends up being either
    // Shared, Exclusive or Modified, see Packet::setCacheResponding
    // for more details
    if (!pkt->hasSharers()) {
        // we could get a writable line from memory (rather than a
        // cache) even in a read-only cache, note that we set this bit
        // even for a read-only cache, possibly revisit this decision
        blk->setCoherenceBits(CacheBlk::WritableBit);

        // check if we got this via cache-to-cache transfer (i.e., from a
        // cache that had the block in Modified or Owned state)
        if (pkt->cacheResponding()) {
            // we got the block in Modified state, and invalidated the
            // owners copy
            blk->setCoherenceBits(CacheBlk::DirtyBit);

            gem5_assert(!isReadOnly, "Should never see dirty snoop response "
                        "in read-only cache %s\n", name());

        }
    }

    DPRINTF(Cache, "Block addr %#llx (%s) moving from %s to %s\n",
            addr, is_secure ? "s" : "ns", old_state, blk->print());

    // if we got new data, copy it in (checking for a read response
    // and a response that has data is the same in the end)
    if (pkt->isRead()) {
        // sanity checks
        assert(pkt->hasData());
        assert(pkt->getSize() == blkSize);

        updateBlockData(blk, pkt, has_old_data);
    }
    // The block will be ready when the payload arrives and the fill is done
    blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
                      pkt->payloadDelay);

    return blk;
}

CacheBlk*
BaseCache::allocateBlock(const PacketPtr pkt, PacketList &writebacks)
{
    // Get address
    const Addr addr = pkt->getAddr();

    // Get secure bit
    const bool is_secure = pkt->isSecure();

    // Block size and compression related access latency. Only relevant if
    // using a compressor, otherwise there is no extra delay, and the block
    // is fully sized
    std::size_t blk_size_bits = blkSize*8;
    Cycles compression_lat = Cycles(0);
    Cycles decompression_lat = Cycles(0);

    // If a compressor is being used, it is called to compress data before
    // insertion. Although in Gem5 the data is stored uncompressed, even if a
    // compressor is used, the compression/decompression methods are called to
    // calculate the amount of extra cycles needed to read or write compressed
    // blocks.
    if (compressor && pkt->hasData()) {
        const auto comp_data = compressor->compress(
            pkt->getConstPtr<uint64_t>(), compression_lat, decompression_lat);
        blk_size_bits = comp_data->getSizeBits();
    }

    // Find replacement victim
    std::vector<CacheBlk*> evict_blks;
    CacheBlk *victim = tags->findVictim(addr, is_secure, blk_size_bits,
                                        evict_blks);

    // It is valid to return nullptr if there is no victim
    if (!victim)
        return nullptr;

    // Print victim block's information
    DPRINTF(CacheRepl, "Replacement victim: %s\n", victim->print());

    // Try to evict blocks; if it fails, give up on allocation
    if (!handleEvictions(evict_blks, writebacks)) {
        return nullptr;
    }

    // Insert new block at victimized entry
    tags->insertBlock(pkt, victim);

    // If using a compressor, set compression data. This must be done after
    // insertion, as the compression bit may be set.
    if (compressor) {
        compressor->setSizeBits(victim, blk_size_bits);
        compressor->setDecompressionLatency(victim, decompression_lat);
    }

    return victim;
}

void
BaseCache::invalidateBlock(CacheBlk *blk)
{
    // If block is still marked as prefetched, then it hasn't been used
    if (blk->wasPrefetched()) {
        prefetcher->prefetchUnused();
    }

    // Notify that the data contents for this address are no longer present
    updateBlockData(blk, nullptr, blk->isValid());

    // If handling a block present in the Tags, let it do its invalidation
    // process, which will update stats and invalidate the block itself
    if (blk != tempBlock) {
        tags->invalidate(blk);
    } else {
        tempBlock->invalidate();
    }
}

void
BaseCache::evictBlock(CacheBlk *blk, PacketList &writebacks)
{
    PacketPtr pkt = evictBlock(blk);
    if (pkt) {
        writebacks.push_back(pkt);
    }
}

PacketPtr
BaseCache::writebackBlk(CacheBlk *blk)
{
    gem5_assert(!isReadOnly || writebackClean,
                "Writeback from read-only cache");
    assert(blk && blk->isValid() &&
        (blk->isSet(CacheBlk::DirtyBit) || writebackClean));

    stats.writebacks[Request::wbRequestorId]++;

    RequestPtr req = std::make_shared<Request>(
        regenerateBlkAddr(blk), blkSize, 0, Request::wbRequestorId);

    if (blk->isSecure())
        req->setFlags(Request::SECURE);

    req->taskId(blk->getTaskId());

    PacketPtr pkt =
        new Packet(req, blk->isSet(CacheBlk::DirtyBit) ?
                   MemCmd::WritebackDirty : MemCmd::WritebackClean);

    DPRINTF(Cache, "Create Writeback %s writable: %d, dirty: %d\n",
        pkt->print(), blk->isSet(CacheBlk::WritableBit),
        blk->isSet(CacheBlk::DirtyBit));

    if (blk->isSet(CacheBlk::WritableBit)) {
        // not asserting shared means we pass the block in modified
        // state, mark our own block non-writeable
        blk->clearCoherenceBits(CacheBlk::WritableBit);
    } else {
        // we are in the Owned state, tell the receiver
        pkt->setHasSharers();
    }

    // make sure the block is not marked dirty
    blk->clearCoherenceBits(CacheBlk::DirtyBit);

    pkt->allocate();
    pkt->setDataFromBlock(blk->data, blkSize);

    // When a block is compressed, it must first be decompressed before being
    // sent for writeback.
    if (compressor) {
        pkt->payloadDelay = compressor->getDecompressionLatency(blk);
    }

    return pkt;
}

PacketPtr
BaseCache::writecleanBlk(CacheBlk *blk, Request::Flags dest, PacketId id)
{
    RequestPtr req = std::make_shared<Request>(
        regenerateBlkAddr(blk), blkSize, 0, Request::wbRequestorId);

    if (blk->isSecure()) {
        req->setFlags(Request::SECURE);
    }
    req->taskId(blk->getTaskId());

    PacketPtr pkt = new Packet(req, MemCmd::WriteClean, blkSize, id);

    if (dest) {
        req->setFlags(dest);
        pkt->setWriteThrough();
    }

    DPRINTF(Cache, "Create %s writable: %d, dirty: %d\n", pkt->print(),
            blk->isSet(CacheBlk::WritableBit), blk->isSet(CacheBlk::DirtyBit));

    if (blk->isSet(CacheBlk::WritableBit)) {
        // not asserting shared means we pass the block in modified
        // state, mark our own block non-writeable
        blk->clearCoherenceBits(CacheBlk::WritableBit);
    } else {
        // we are in the Owned state, tell the receiver
        pkt->setHasSharers();
    }

    // make sure the block is not marked dirty
    blk->clearCoherenceBits(CacheBlk::DirtyBit);

    pkt->allocate();
    pkt->setDataFromBlock(blk->data, blkSize);

    // When a block is compressed, it must first be decompressed before being
    // sent for writeback.
    if (compressor) {
        pkt->payloadDelay = compressor->getDecompressionLatency(blk);
    }

    return pkt;
}


void
BaseCache::memWriteback()
{
    tags->forEachBlk([this](CacheBlk &blk) { writebackVisitor(blk); });
}

void
BaseCache::memInvalidate()
{
    tags->forEachBlk([this](CacheBlk &blk) { invalidateVisitor(blk); });
}

bool
BaseCache::isDirty() const
{
    return tags->anyBlk([](CacheBlk &blk) {
        return blk.isSet(CacheBlk::DirtyBit); });
}

bool
BaseCache::coalesce() const
{
    return writeAllocator && writeAllocator->coalesce();
}

void
BaseCache::writebackVisitor(CacheBlk &blk)
{
    if (blk.isSet(CacheBlk::DirtyBit)) {
        assert(blk.isValid());

        RequestPtr request = std::make_shared<Request>(
            regenerateBlkAddr(&blk), blkSize, 0, Request::funcRequestorId);

        request->taskId(blk.getTaskId());
        if (blk.isSecure()) {
            request->setFlags(Request::SECURE);
        }

        Packet packet(request, MemCmd::WriteReq);
        packet.dataStatic(blk.data);

        memSidePort.sendFunctional(&packet);

        blk.clearCoherenceBits(CacheBlk::DirtyBit);
    }
}

void
BaseCache::invalidateVisitor(CacheBlk &blk)
{
    if (blk.isSet(CacheBlk::DirtyBit))
        warn_once("Invalidating dirty cache lines. " \
                  "Expect things to break.\n");

    if (blk.isValid()) {
        assert(!blk.isSet(CacheBlk::DirtyBit));
        invalidateBlock(&blk);
    }
}

Tick
BaseCache::nextQueueReadyTime() const
{
    Tick nextReady = std::min(mshrQueue.nextReadyTime(),
                              writeBuffer.nextReadyTime());

    // Don't signal prefetch ready time if no MSHRs available
    // Will signal once enoguh MSHRs are deallocated
    if (prefetcher && mshrQueue.canPrefetch() && !isBlocked()) {
        nextReady = std::min(nextReady,
                             prefetcher->nextPrefetchReadyTime());
    }

    return nextReady;
}


bool
BaseCache::sendMSHRQueuePacket(MSHR* mshr)
{
    assert(mshr);

    // use request from 1st target
    PacketPtr tgt_pkt = mshr->getTarget()->pkt;

    DPRINTF(Cache, "%s: MSHR %s\n", __func__, tgt_pkt->print());

    // if the cache is in write coalescing mode or (additionally) in
    // no allocation mode, and we have a write packet with an MSHR
    // that is not a whole-line write (due to incompatible flags etc),
    // then reset the write mode
    if (writeAllocator && writeAllocator->coalesce() && tgt_pkt->isWrite()) {
        if (!mshr->isWholeLineWrite()) {
            // if we are currently write coalescing, hold on the
            // MSHR as many cycles extra as we need to completely
            // write a cache line
            if (writeAllocator->delay(mshr->blkAddr)) {
                Tick delay = blkSize / tgt_pkt->getSize() * clockPeriod();
                DPRINTF(CacheVerbose, "Delaying pkt %s %llu ticks to allow "
                        "for write coalescing\n", tgt_pkt->print(), delay);
                mshrQueue.delay(mshr, delay);
                return false;
            } else {
                writeAllocator->reset();
            }
        } else {
            writeAllocator->resetDelay(mshr->blkAddr);
        }
    }

    CacheBlk *blk = tags->findBlock(mshr->blkAddr, mshr->isSecure);

    // either a prefetch that is not present upstream, or a normal
    // MSHR request, proceed to get the packet to send downstream
    PacketPtr pkt = createMissPacket(tgt_pkt, blk, mshr->needsWritable(),
                                     mshr->isWholeLineWrite());

    mshr->isForward = (pkt == nullptr);

    if (mshr->isForward) {
        // not a cache block request, but a response is expected
        // make copy of current packet to forward, keep current
        // copy for response handling
        pkt = new Packet(tgt_pkt, false, true);
        assert(!pkt->isWrite());
    }

    // play it safe and append (rather than set) the sender state,
    // as forwarded packets may already have existing state
    pkt->pushSenderState(mshr);

    if (pkt->isClean() && blk && blk->isSet(CacheBlk::DirtyBit)) {
        // A cache clean opearation is looking for a dirty block. Mark
        // the packet so that the destination xbar can determine that
        // there will be a follow-up write packet as well.
        pkt->setSatisfied();
    }

    if (!memSidePort.sendTimingReq(pkt)) {
        // we are awaiting a retry, but we
        // delete the packet and will be creating a new packet
        // when we get the opportunity
        delete pkt;

        // note that we have now masked any requestBus and
        // schedSendEvent (we will wait for a retry before
        // doing anything), and this is so even if we do not
        // care about this packet and might override it before
        // it gets retried
        return true;
    } else {
        // As part of the call to sendTimingReq the packet is
        // forwarded to all neighbouring caches (and any caches
        // above them) as a snoop. Thus at this point we know if
        // any of the neighbouring caches are responding, and if
        // so, we know it is dirty, and we can determine if it is
        // being passed as Modified, making our MSHR the ordering
        // point
        bool pending_modified_resp = !pkt->hasSharers() &&
            pkt->cacheResponding();
        markInService(mshr, pending_modified_resp);

        if (pkt->isClean() && blk && blk->isSet(CacheBlk::DirtyBit)) {
            // A cache clean opearation is looking for a dirty
            // block. If a dirty block is encountered a WriteClean
            // will update any copies to the path to the memory
            // until the point of reference.
            DPRINTF(CacheVerbose, "%s: packet %s found block: %s\n",
                    __func__, pkt->print(), blk->print());
            PacketPtr wb_pkt = writecleanBlk(blk, pkt->req->getDest(),
                                             pkt->id);
            PacketList writebacks;
            writebacks.push_back(wb_pkt);
            doWritebacks(writebacks, 0);
        }

        return false;
    }
}

bool
BaseCache::sendWriteQueuePacket(WriteQueueEntry* wq_entry)
{
    assert(wq_entry);

    // always a single target for write queue entries
    PacketPtr tgt_pkt = wq_entry->getTarget()->pkt;

    DPRINTF(Cache, "%s: write %s\n", __func__, tgt_pkt->print());

    // forward as is, both for evictions and uncacheable writes
    if (!memSidePort.sendTimingReq(tgt_pkt)) {
        // note that we have now masked any requestBus and
        // schedSendEvent (we will wait for a retry before
        // doing anything), and this is so even if we do not
        // care about this packet and might override it before
        // it gets retried
        return true;
    } else {
        markInService(wq_entry);
        return false;
    }
}

void
BaseCache::serialize(CheckpointOut &cp) const
{
    bool dirty(isDirty());

    if (dirty) {
        warn("*** The cache still contains dirty data. ***\n");
        warn("    Make sure to drain the system using the correct flags.\n");
        warn("    This checkpoint will not restore correctly " \
             "and dirty data in the cache will be lost!\n");
    }

    // Since we don't checkpoint the data in the cache, any dirty data
    // will be lost when restoring from a checkpoint of a system that
    // wasn't drained properly. Flag the checkpoint as invalid if the
    // cache contains dirty data.
    bool bad_checkpoint(dirty);
    SERIALIZE_SCALAR(bad_checkpoint);
}

void
BaseCache::unserialize(CheckpointIn &cp)
{
    bool bad_checkpoint;
    UNSERIALIZE_SCALAR(bad_checkpoint);
    if (bad_checkpoint) {
        fatal("Restoring from checkpoints with dirty caches is not "
              "supported in the classic memory system. Please remove any "
              "caches or drain them properly before taking checkpoints.\n");
    }
}


BaseCache::CacheCmdStats::CacheCmdStats(BaseCache &c,
                                        const std::string &name)
    : statistics::Group(&c, name.c_str()), cache(c),
      ADD_STAT(hits, statistics::units::Count::get(),
               ("number of " + name + " hits").c_str()),
      ADD_STAT(misses, statistics::units::Count::get(),
               ("number of " + name + " misses").c_str()),
      ADD_STAT(hitLatency, statistics::units::Tick::get(),
               ("number of " + name + " hit ticks").c_str()),
      ADD_STAT(missLatency, statistics::units::Tick::get(),
               ("number of " + name + " miss ticks").c_str()),
      ADD_STAT(accesses, statistics::units::Count::get(),
               ("number of " + name + " accesses(hits+misses)").c_str()),
      ADD_STAT(missRate, statistics::units::Ratio::get(),
               ("miss rate for " + name + " accesses").c_str()),
      ADD_STAT(avgMissLatency, statistics::units::Rate<
                    statistics::units::Tick, statistics::units::Count>::get(),
               ("average " + name + " miss latency").c_str()),
      ADD_STAT(mshrHits, statistics::units::Count::get(),
               ("number of " + name + " MSHR hits").c_str()),
      ADD_STAT(mshrMisses, statistics::units::Count::get(),
               ("number of " + name + " MSHR misses").c_str()),
      ADD_STAT(mshrUncacheable, statistics::units::Count::get(),
               ("number of " + name + " MSHR uncacheable").c_str()),
      ADD_STAT(mshrMissLatency, statistics::units::Tick::get(),
               ("number of " + name + " MSHR miss ticks").c_str()),
      ADD_STAT(mshrUncacheableLatency, statistics::units::Tick::get(),
               ("number of " + name + " MSHR uncacheable ticks").c_str()),
      ADD_STAT(mshrMissRate, statistics::units::Ratio::get(),
               ("mshr miss rate for " + name + " accesses").c_str()),
      ADD_STAT(avgMshrMissLatency, statistics::units::Rate<
                    statistics::units::Tick, statistics::units::Count>::get(),
               ("average " + name + " mshr miss latency").c_str()),
      ADD_STAT(avgMshrUncacheableLatency, statistics::units::Rate<
                    statistics::units::Tick, statistics::units::Count>::get(),
               ("average " + name + " mshr uncacheable latency").c_str())
{
}

void
BaseCache::CacheCmdStats::regStatsFromParent()
{
    using namespace statistics;

    statistics::Group::regStats();
    System *system = cache.system;
    const auto max_requestors = system->maxRequestors();

    hits
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        hits.subname(i, system->getRequestorName(i));
    }

    // Miss statistics
    misses
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        misses.subname(i, system->getRequestorName(i));
    }

    // Hit latency statistics
    hitLatency
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        hitLatency.subname(i, system->getRequestorName(i));
    }

    // Miss latency statistics
    missLatency
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        missLatency.subname(i, system->getRequestorName(i));
    }

    // access formulas
    accesses.flags(total | nozero | nonan);
    accesses = hits + misses;
    for (int i = 0; i < max_requestors; i++) {
        accesses.subname(i, system->getRequestorName(i));
    }

    // miss rate formulas
    missRate.flags(total | nozero | nonan);
    missRate = misses / accesses;
    for (int i = 0; i < max_requestors; i++) {
        missRate.subname(i, system->getRequestorName(i));
    }

    // miss latency formulas
    avgMissLatency.flags(total | nozero | nonan);
    avgMissLatency = missLatency / misses;
    for (int i = 0; i < max_requestors; i++) {
        avgMissLatency.subname(i, system->getRequestorName(i));
    }

    // MSHR statistics
    // MSHR hit statistics
    mshrHits
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        mshrHits.subname(i, system->getRequestorName(i));
    }

    // MSHR miss statistics
    mshrMisses
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        mshrMisses.subname(i, system->getRequestorName(i));
    }

    // MSHR miss latency statistics
    mshrMissLatency
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        mshrMissLatency.subname(i, system->getRequestorName(i));
    }

    // MSHR uncacheable statistics
    mshrUncacheable
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        mshrUncacheable.subname(i, system->getRequestorName(i));
    }

    // MSHR miss latency statistics
    mshrUncacheableLatency
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        mshrUncacheableLatency.subname(i, system->getRequestorName(i));
    }

    // MSHR miss rate formulas
    mshrMissRate.flags(total | nozero | nonan);
    mshrMissRate = mshrMisses / accesses;

    for (int i = 0; i < max_requestors; i++) {
        mshrMissRate.subname(i, system->getRequestorName(i));
    }

    // mshrMiss latency formulas
    avgMshrMissLatency.flags(total | nozero | nonan);
    avgMshrMissLatency = mshrMissLatency / mshrMisses;
    for (int i = 0; i < max_requestors; i++) {
        avgMshrMissLatency.subname(i, system->getRequestorName(i));
    }

    // mshrUncacheable latency formulas
    avgMshrUncacheableLatency.flags(total | nozero | nonan);
    avgMshrUncacheableLatency = mshrUncacheableLatency / mshrUncacheable;
    for (int i = 0; i < max_requestors; i++) {
        avgMshrUncacheableLatency.subname(i, system->getRequestorName(i));
    }
}

BaseCache::CacheStats::CacheStats(BaseCache &c)
    : statistics::Group(&c), cache(c),

    ADD_STAT(demandHits, statistics::units::Count::get(),
             "number of demand (read+write) hits"),
    ADD_STAT(overallHits, statistics::units::Count::get(),
             "number of overall hits"),
    ADD_STAT(demandHitLatency, statistics::units::Tick::get(),
             "number of demand (read+write) hit ticks"),
    ADD_STAT(overallHitLatency, statistics::units::Tick::get(),
            "number of overall hit ticks"),
    ADD_STAT(demandMisses, statistics::units::Count::get(),
             "number of demand (read+write) misses"),
    ADD_STAT(overallMisses, statistics::units::Count::get(),
             "number of overall misses"),
    ADD_STAT(demandMissLatency, statistics::units::Tick::get(),
             "number of demand (read+write) miss ticks"),
    ADD_STAT(overallMissLatency, statistics::units::Tick::get(),
             "number of overall miss ticks"),
    ADD_STAT(demandAccesses, statistics::units::Count::get(),
             "number of demand (read+write) accesses"),
    ADD_STAT(overallAccesses, statistics::units::Count::get(),
             "number of overall (read+write) accesses"),
    ADD_STAT(demandMissRate, statistics::units::Ratio::get(),
             "miss rate for demand accesses"),
    ADD_STAT(overallMissRate, statistics::units::Ratio::get(),
             "miss rate for overall accesses"),
    ADD_STAT(demandAvgMissLatency, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "average overall miss latency in ticks"),
    ADD_STAT(overallAvgMissLatency, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "average overall miss latency"),
    ADD_STAT(blockedCycles, statistics::units::Cycle::get(),
            "number of cycles access was blocked"),
    ADD_STAT(blockedCauses, statistics::units::Count::get(),
            "number of times access was blocked"),
    ADD_STAT(avgBlocked, statistics::units::Rate<
                statistics::units::Cycle, statistics::units::Count>::get(),
             "average number of cycles each access was blocked"),
    ADD_STAT(writebacks, statistics::units::Count::get(),
             "number of writebacks"),
    ADD_STAT(demandMshrHits, statistics::units::Count::get(),
             "number of demand (read+write) MSHR hits"),
    ADD_STAT(overallMshrHits, statistics::units::Count::get(),
             "number of overall MSHR hits"),
    ADD_STAT(demandMshrMisses, statistics::units::Count::get(),
             "number of demand (read+write) MSHR misses"),
    ADD_STAT(overallMshrMisses, statistics::units::Count::get(),
            "number of overall MSHR misses"),
    ADD_STAT(overallMshrUncacheable, statistics::units::Count::get(),
             "number of overall MSHR uncacheable misses"),
    ADD_STAT(demandMshrMissLatency, statistics::units::Tick::get(),
             "number of demand (read+write) MSHR miss ticks"),
    ADD_STAT(overallMshrMissLatency, statistics::units::Tick::get(),
             "number of overall MSHR miss ticks"),
    ADD_STAT(overallMshrUncacheableLatency, statistics::units::Tick::get(),
             "number of overall MSHR uncacheable ticks"),
    ADD_STAT(demandMshrMissRate, statistics::units::Ratio::get(),
             "mshr miss ratio for demand accesses"),
    ADD_STAT(overallMshrMissRate, statistics::units::Ratio::get(),
             "mshr miss ratio for overall accesses"),
    ADD_STAT(demandAvgMshrMissLatency, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "average overall mshr miss latency"),
    ADD_STAT(overallAvgMshrMissLatency, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "average overall mshr miss latency"),
    ADD_STAT(overallAvgMshrUncacheableLatency, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "average overall mshr uncacheable latency"),
    ADD_STAT(replacements, statistics::units::Count::get(),
             "number of replacements"),
    ADD_STAT(dataExpansions, statistics::units::Count::get(),
             "number of data expansions"),
    ADD_STAT(dataContractions, statistics::units::Count::get(),
             "number of data contractions"),
    cmd(MemCmd::NUM_MEM_CMDS)
{
    for (int idx = 0; idx < MemCmd::NUM_MEM_CMDS; ++idx)
        cmd[idx].reset(new CacheCmdStats(c, MemCmd(idx).toString()));
}

void
BaseCache::CacheStats::regStats()
{
    using namespace statistics;

    statistics::Group::regStats();

    System *system = cache.system;
    const auto max_requestors = system->maxRequestors();

    for (auto &cs : cmd)
        cs->regStatsFromParent();

// These macros make it easier to sum the right subset of commands and
// to change the subset of commands that are considered "demand" vs
// "non-demand"
#define SUM_DEMAND(s)                                                   \
    (cmd[MemCmd::ReadReq]->s + cmd[MemCmd::WriteReq]->s +               \
     cmd[MemCmd::WriteLineReq]->s + cmd[MemCmd::ReadExReq]->s +         \
     cmd[MemCmd::ReadCleanReq]->s + cmd[MemCmd::ReadSharedReq]->s)

// should writebacks be included here?  prior code was inconsistent...
#define SUM_NON_DEMAND(s)                                       \
    (cmd[MemCmd::SoftPFReq]->s + cmd[MemCmd::HardPFReq]->s +    \
     cmd[MemCmd::SoftPFExReq]->s)

    demandHits.flags(total | nozero | nonan);
    demandHits = SUM_DEMAND(hits);
    for (int i = 0; i < max_requestors; i++) {
        demandHits.subname(i, system->getRequestorName(i));
    }

    overallHits.flags(total | nozero | nonan);
    overallHits = demandHits + SUM_NON_DEMAND(hits);
    for (int i = 0; i < max_requestors; i++) {
        overallHits.subname(i, system->getRequestorName(i));
    }

    demandMisses.flags(total | nozero | nonan);
    demandMisses = SUM_DEMAND(misses);
    for (int i = 0; i < max_requestors; i++) {
        demandMisses.subname(i, system->getRequestorName(i));
    }

    overallMisses.flags(total | nozero | nonan);
    overallMisses = demandMisses + SUM_NON_DEMAND(misses);
    for (int i = 0; i < max_requestors; i++) {
        overallMisses.subname(i, system->getRequestorName(i));
    }

    demandMissLatency.flags(total | nozero | nonan);
    demandMissLatency = SUM_DEMAND(missLatency);
    for (int i = 0; i < max_requestors; i++) {
        demandMissLatency.subname(i, system->getRequestorName(i));
    }

    overallMissLatency.flags(total | nozero | nonan);
    overallMissLatency = demandMissLatency + SUM_NON_DEMAND(missLatency);
    for (int i = 0; i < max_requestors; i++) {
        overallMissLatency.subname(i, system->getRequestorName(i));
    }

    demandHitLatency.flags(total | nozero | nonan);
    demandHitLatency = SUM_DEMAND(hitLatency);
    for (int i = 0; i < max_requestors; i++) {
        demandHitLatency.subname(i, system->getRequestorName(i));
    }
    overallHitLatency.flags(total | nozero | nonan);
    overallHitLatency = demandHitLatency + SUM_NON_DEMAND(hitLatency);
    for (int i = 0; i < max_requestors; i++) {
        overallHitLatency.subname(i, system->getRequestorName(i));
    }

    demandAccesses.flags(total | nozero | nonan);
    demandAccesses = demandHits + demandMisses;
    for (int i = 0; i < max_requestors; i++) {
        demandAccesses.subname(i, system->getRequestorName(i));
    }

    overallAccesses.flags(total | nozero | nonan);
    overallAccesses = overallHits + overallMisses;
    for (int i = 0; i < max_requestors; i++) {
        overallAccesses.subname(i, system->getRequestorName(i));
    }

    demandMissRate.flags(total | nozero | nonan);
    demandMissRate = demandMisses / demandAccesses;
    for (int i = 0; i < max_requestors; i++) {
        demandMissRate.subname(i, system->getRequestorName(i));
    }

    overallMissRate.flags(total | nozero | nonan);
    overallMissRate = overallMisses / overallAccesses;
    for (int i = 0; i < max_requestors; i++) {
        overallMissRate.subname(i, system->getRequestorName(i));
    }

    demandAvgMissLatency.flags(total | nozero | nonan);
    demandAvgMissLatency = demandMissLatency / demandMisses;
    for (int i = 0; i < max_requestors; i++) {
        demandAvgMissLatency.subname(i, system->getRequestorName(i));
    }

    overallAvgMissLatency.flags(total | nozero | nonan);
    overallAvgMissLatency = overallMissLatency / overallMisses;
    for (int i = 0; i < max_requestors; i++) {
        overallAvgMissLatency.subname(i, system->getRequestorName(i));
    }

    blockedCycles.init(NUM_BLOCKED_CAUSES);
    blockedCycles
        .subname(Blocked_NoMSHRs, "no_mshrs")
        .subname(Blocked_NoTargets, "no_targets")
        ;


    blockedCauses.init(NUM_BLOCKED_CAUSES);
    blockedCauses
        .subname(Blocked_NoMSHRs, "no_mshrs")
        .subname(Blocked_NoTargets, "no_targets")
        ;

    avgBlocked
        .subname(Blocked_NoMSHRs, "no_mshrs")
        .subname(Blocked_NoTargets, "no_targets")
        ;
    avgBlocked = blockedCycles / blockedCauses;

    writebacks
        .init(max_requestors)
        .flags(total | nozero | nonan)
        ;
    for (int i = 0; i < max_requestors; i++) {
        writebacks.subname(i, system->getRequestorName(i));
    }

    demandMshrHits.flags(total | nozero | nonan);
    demandMshrHits = SUM_DEMAND(mshrHits);
    for (int i = 0; i < max_requestors; i++) {
        demandMshrHits.subname(i, system->getRequestorName(i));
    }

    overallMshrHits.flags(total | nozero | nonan);
    overallMshrHits = demandMshrHits + SUM_NON_DEMAND(mshrHits);
    for (int i = 0; i < max_requestors; i++) {
        overallMshrHits.subname(i, system->getRequestorName(i));
    }

    demandMshrMisses.flags(total | nozero | nonan);
    demandMshrMisses = SUM_DEMAND(mshrMisses);
    for (int i = 0; i < max_requestors; i++) {
        demandMshrMisses.subname(i, system->getRequestorName(i));
    }

    overallMshrMisses.flags(total | nozero | nonan);
    overallMshrMisses = demandMshrMisses + SUM_NON_DEMAND(mshrMisses);
    for (int i = 0; i < max_requestors; i++) {
        overallMshrMisses.subname(i, system->getRequestorName(i));
    }

    demandMshrMissLatency.flags(total | nozero | nonan);
    demandMshrMissLatency = SUM_DEMAND(mshrMissLatency);
    for (int i = 0; i < max_requestors; i++) {
        demandMshrMissLatency.subname(i, system->getRequestorName(i));
    }

    overallMshrMissLatency.flags(total | nozero | nonan);
    overallMshrMissLatency =
        demandMshrMissLatency + SUM_NON_DEMAND(mshrMissLatency);
    for (int i = 0; i < max_requestors; i++) {
        overallMshrMissLatency.subname(i, system->getRequestorName(i));
    }

    overallMshrUncacheable.flags(total | nozero | nonan);
    overallMshrUncacheable =
        SUM_DEMAND(mshrUncacheable) + SUM_NON_DEMAND(mshrUncacheable);
    for (int i = 0; i < max_requestors; i++) {
        overallMshrUncacheable.subname(i, system->getRequestorName(i));
    }


    overallMshrUncacheableLatency.flags(total | nozero | nonan);
    overallMshrUncacheableLatency =
        SUM_DEMAND(mshrUncacheableLatency) +
        SUM_NON_DEMAND(mshrUncacheableLatency);
    for (int i = 0; i < max_requestors; i++) {
        overallMshrUncacheableLatency.subname(i, system->getRequestorName(i));
    }

    demandMshrMissRate.flags(total | nozero | nonan);
    demandMshrMissRate = demandMshrMisses / demandAccesses;
    for (int i = 0; i < max_requestors; i++) {
        demandMshrMissRate.subname(i, system->getRequestorName(i));
    }

    overallMshrMissRate.flags(total | nozero | nonan);
    overallMshrMissRate = overallMshrMisses / overallAccesses;
    for (int i = 0; i < max_requestors; i++) {
        overallMshrMissRate.subname(i, system->getRequestorName(i));
    }

    demandAvgMshrMissLatency.flags(total | nozero | nonan);
    demandAvgMshrMissLatency = demandMshrMissLatency / demandMshrMisses;
    for (int i = 0; i < max_requestors; i++) {
        demandAvgMshrMissLatency.subname(i, system->getRequestorName(i));
    }

    overallAvgMshrMissLatency.flags(total | nozero | nonan);
    overallAvgMshrMissLatency = overallMshrMissLatency / overallMshrMisses;
    for (int i = 0; i < max_requestors; i++) {
        overallAvgMshrMissLatency.subname(i, system->getRequestorName(i));
    }

    overallAvgMshrUncacheableLatency.flags(total | nozero | nonan);
    overallAvgMshrUncacheableLatency =
        overallMshrUncacheableLatency / overallMshrUncacheable;
    for (int i = 0; i < max_requestors; i++) {
        overallAvgMshrUncacheableLatency.subname(i,
            system->getRequestorName(i));
    }

    dataExpansions.flags(nozero | nonan);
    dataContractions.flags(nozero | nonan);
}

void
BaseCache::regProbePoints()
{
    ppHit = new ProbePointArg<PacketPtr>(this->getProbeManager(), "Hit");
    ppMiss = new ProbePointArg<PacketPtr>(this->getProbeManager(), "Miss");
    ppFill = new ProbePointArg<PacketPtr>(this->getProbeManager(), "Fill");
    ppDataUpdate =
        new ProbePointArg<DataUpdate>(this->getProbeManager(), "Data Update");
}

///////////////
//
// CpuSidePort
//
///////////////
bool
BaseCache::CpuSidePort::recvTimingSnoopResp(PacketPtr pkt)
{
    // Snoops shouldn't happen when bypassing caches
    assert(!cache->system->bypassCaches());

    assert(pkt->isResponse());

    // Express snoop responses from requestor to responder, e.g., from L1 to L2
    cache->recvTimingSnoopResp(pkt);
    return true;
}


bool
BaseCache::CpuSidePort::tryTiming(PacketPtr pkt)
{
    if (cache->system->bypassCaches() || pkt->isExpressSnoop()) {
        // always let express snoop packets through even if blocked
        return true;
    } else if (blocked || mustSendRetry) {
        // either already committed to send a retry, or blocked
        mustSendRetry = true;
        return false;
    }
    mustSendRetry = false;
    return true;
}

bool
BaseCache::CpuSidePort::recvTimingReq(PacketPtr pkt)
{
    assert(pkt->isRequest());

    if (cache->system->bypassCaches()) {
        // Just forward the packet if caches are disabled.
        // @todo This should really enqueue the packet rather
        [[maybe_unused]] bool success = cache->memSidePort.sendTimingReq(pkt);
        assert(success);
        return true;
    } else if (tryTiming(pkt)) {
        cache->recvTimingReq(pkt);
        return true;
    }
    return false;
}

Tick
BaseCache::CpuSidePort::recvAtomic(PacketPtr pkt)
{
    if (cache->system->bypassCaches()) {
        // Forward the request if the system is in cache bypass mode.
        return cache->memSidePort.sendAtomic(pkt);
    } else {
        return cache->recvAtomic(pkt);
    }
}

void
BaseCache::CpuSidePort::recvFunctional(PacketPtr pkt)
{
    if (cache->system->bypassCaches()) {
        // The cache should be flushed if we are in cache bypass mode,
        // so we don't need to check if we need to update anything.
        cache->memSidePort.sendFunctional(pkt);
        return;
    }

    // functional request
    cache->functionalAccess(pkt, true);
}

AddrRangeList
BaseCache::CpuSidePort::getAddrRanges() const
{
    return cache->getAddrRanges();
}


BaseCache::
CpuSidePort::CpuSidePort(const std::string &_name, BaseCache *_cache,
                         const std::string &_label)
    : CacheResponsePort(_name, _cache, _label), cache(_cache)
{
}

///////////////
//
// MemSidePort
//
///////////////
bool
BaseCache::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    cache->recvTimingResp(pkt);
    return true;
}

// Express snooping requests to memside port
void
BaseCache::MemSidePort::recvTimingSnoopReq(PacketPtr pkt)
{
    // Snoops shouldn't happen when bypassing caches
    assert(!cache->system->bypassCaches());

    // handle snooping requests
    cache->recvTimingSnoopReq(pkt);
}

Tick
BaseCache::MemSidePort::recvAtomicSnoop(PacketPtr pkt)
{
    // Snoops shouldn't happen when bypassing caches
    assert(!cache->system->bypassCaches());

    return cache->recvAtomicSnoop(pkt);
}

void
BaseCache::MemSidePort::recvFunctionalSnoop(PacketPtr pkt)
{
    // Snoops shouldn't happen when bypassing caches
    assert(!cache->system->bypassCaches());

    // functional snoop (note that in contrast to atomic we don't have
    // a specific functionalSnoop method, as they have the same
    // behaviour regardless)
    cache->functionalAccess(pkt, false);
}

void
BaseCache::CacheReqPacketQueue::sendDeferredPacket()
{
    // sanity check
    assert(!waitingOnRetry);

    // there should never be any deferred request packets in the
    // queue, instead we resly on the cache to provide the packets
    // from the MSHR queue or write queue
    assert(deferredPacketReadyTime() == MaxTick);

    // check for request packets (requests & writebacks)
    QueueEntry* entry = cache.getNextQueueEntry();

    if (!entry) {
        // can happen if e.g. we attempt a writeback and fail, but
        // before the retry, the writeback is eliminated because
        // we snoop another cache's ReadEx.
    } else {
        // let our snoop responses go first if there are responses to
        // the same addresses
        if (checkConflictingSnoop(entry->getTarget()->pkt)) {
            return;
        }
        waitingOnRetry = entry->sendPacket(cache);
    }

    // if we succeeded and are not waiting for a retry, schedule the
    // next send considering when the next queue is ready, note that
    // snoop responses have their own packet queue and thus schedule
    // their own events
    if (!waitingOnRetry) {
        schedSendEvent(cache.nextQueueReadyTime());
    }
}

BaseCache::MemSidePort::MemSidePort(const std::string &_name,
                                    BaseCache *_cache,
                                    const std::string &_label)
    : CacheRequestPort(_name, _cache, _reqQueue, _snoopRespQueue),
      _reqQueue(*_cache, *this, _snoopRespQueue, _label),
      _snoopRespQueue(*_cache, *this, true, _label), cache(_cache)
{
}

void
WriteAllocator::updateMode(Addr write_addr, unsigned write_size,
                           Addr blk_addr)
{
    // check if we are continuing where the last write ended
    if (nextAddr == write_addr) {
        delayCtr[blk_addr] = delayThreshold;
        // stop if we have already saturated
        if (mode != WriteMode::NO_ALLOCATE) {
            byteCount += write_size;
            // switch to streaming mode if we have passed the lower
            // threshold
            if (mode == WriteMode::ALLOCATE &&
                byteCount > coalesceLimit) {
                mode = WriteMode::COALESCE;
                DPRINTF(Cache, "Switched to write coalescing\n");
            } else if (mode == WriteMode::COALESCE &&
                       byteCount > noAllocateLimit) {
                // and continue and switch to non-allocating mode if we
                // pass the upper threshold
                mode = WriteMode::NO_ALLOCATE;
                DPRINTF(Cache, "Switched to write-no-allocate\n");
            }
        }
    } else {
        // we did not see a write matching the previous one, start
        // over again
        byteCount = write_size;
        mode = WriteMode::ALLOCATE;
        resetDelay(blk_addr);
    }
    nextAddr = write_addr + write_size;
}

} // namespace gem5
