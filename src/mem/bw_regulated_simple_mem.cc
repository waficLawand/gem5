/*
 * Copyright (c) 2010-2013, 2015 ARM Limited
 * All rights reserved
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
 * Copyright (c) 2001-2005 The Regents of The University of Michigan
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

#include "mem/bw_regulated_simple_mem.hh"

#include "base/random.hh"
#include "base/trace.hh"
#include "debug/Drain.hh"

#include <thread>
#include <chrono>

namespace gem5
{

namespace memory
{

BwRegulatedSimpleMemory::BwRegulatedSimpleMemory(const BwRegulatedSimpleMemoryParams &p) :
    AbstractMemory(p),
    port(name() + ".port", *this), latency(p.latency),
    latency_var(p.latency_var), bandwidth(p.bandwidth), isBusy(false),
    retryReq(false), retryResp(false), 
    demand_burstiness(p.demand_burstiness),
    prefetch_burstiness(p.prefetch_burstiness),
    refill_tokens(p.refill_tokens),
    refill_period(p.refill_period),
    requestors(p.requestors),
    demand_queue_size(p.demand_queue_size),
    prefetch_queue_size(p.prefetch_queue_size),
    //ticks_per_cycle(p.ticks_per_cycle),
    releaseEvent([this]{ release(); }, name()),
    dequeueEvent([this]{ dequeue(); }, name()),
    monitoring_event([this]{bucket_refill();},name()),
    consume_demand_token_event([this]{consume_demand_token();},name()),
    handle_pkts([this]{handle_pkts_in_queues();},name())
{
    demand_tokens = new int64_t[requestors];
    prefetch_tokens = new int64_t[requestors];

    demand_queues_pre_bucket = new std::queue<packet_queue_element>[requestors];
    demand_queues_post_bucket = new std::queue<packet_queue_element>[requestors];

    prefetch_queues_pre_bucket = new std::queue<packet_queue_element>[requestors];
    prefetch_queues_post_bucket = new std::queue<packet_queue_element>[requestors];

    // Initializing demand and prefetch token buckets with zeros
    for(int i = 0; i < requestors; ++i)
    {
        //latency_counters[i] = initial_slack; // Initialize all latency counters with the slack value
        prefetch_tokens[i] = 0;
        demand_tokens[i] = 0;
    }
}

void
BwRegulatedSimpleMemory::init()
{
    AbstractMemory::init();

    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }

}
void
BwRegulatedSimpleMemory::startup()
{
    schedule(monitoring_event,curTick());
    schedule(handle_pkts,curTick());
}

void
BwRegulatedSimpleMemory::consume_demand_token()
{
    --demand_tokens;
}

void
BwRegulatedSimpleMemory::handle_pkts_in_queues()
{
    for(int requestor = 0; requestor < requestors; ++requestor)
    {
        // Check if requests exist in the queues before the token bucket
        if(demand_queues_pre_bucket[requestor].size() != 0)
        {
            if(demand_tokens[requestor] > 0)
            {
                /*TODO Check for queue size if needed in the future. Right now, we assume that queues will not overflow*/
                demand_tokens[requestor]--;

                // Extract the front element in the queue
                packet_queue_element front_pkt = demand_queues_pre_bucket[requestor].front();
                
                // Push pkt to post bucket queue
                demand_queues_post_bucket->push(front_pkt);
                
                // Pop the element from the pre bucket queue
                demand_queues_pre_bucket[requestor].pop();
            }
        }

        // Check if requests exist in the queues before the token bucket
        if(prefetch_queues_pre_bucket[requestor].size() != 0)
        {
            if(prefetch_tokens[requestor] > 0)
            {
                /*TODO Check for queue size if needed in the future. Right now, we assume that queues will not overflow*/
                prefetch_tokens[requestor]--;
                
                // Extract the front element in the queue
                packet_queue_element front_pkt = prefetch_queues_pre_bucket[requestor].front();
                
                // Push pkt to post bucket queue
                prefetch_queues_post_bucket[requestor].push(front_pkt);
                
                // Pop the element from the pre bucket queue
                prefetch_queues_pre_bucket[requestor].pop();
            }
        }
    }
    schedule(handle_pkts,curTick()+1);
}

void
BwRegulatedSimpleMemory::bucket_refill()
{
    Tick curr_tick = curTick();
    for(int requestor = 0; requestor < requestors; ++requestor)
    {
        // Check if added tokens will lead to excess
        if(demand_burstiness - demand_tokens[requestor] < refill_tokens)
        {   
            // Fill the demand bucket to its burstiness
            demand_tokens[requestor] = demand_burstiness;

            // Add the excess to the prefetch bucket and cap at prefetch burstiness
            prefetch_tokens[requestor] = std::min(prefetch_burstiness,prefetch_tokens[requestor]+(refill_tokens-(demand_burstiness - demand_tokens[requestor])));
        }
        else
        {
            // Case with no token excess
            demand_tokens[requestor] = demand_tokens[requestor] + refill_tokens;
        }
        /*printf("##################################################\n");
        printf("Current Regulation Tick is: %lld\n",curr_tick);
        printf("Demand Bucket Tokens: %d\n",demand_tokens[requestor]);
        printf("Prefetch Bucket Tokens: %d\n",prefetch_tokens[requestor]);
        */    
    }


    schedule(monitoring_event, curTick() + refill_period);
}

Tick
BwRegulatedSimpleMemory::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    access(pkt);
    return getLatency();
}

Tick
BwRegulatedSimpleMemory::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    Tick latency = recvAtomic(pkt);
    getBackdoor(_backdoor);
    return latency;
}

void
BwRegulatedSimpleMemory::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(name());

    functionalAccess(pkt);

    bool done = false;
    auto p = packetQueue.begin();
    // potentially update the packets in our packet queue as well
    while (!done && p != packetQueue.end()) {
        done = pkt->trySatisfyFunctional(p->pkt);
        ++p;
    }

    pkt->popLabel();
}

void
BwRegulatedSimpleMemory::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &_backdoor)
{
    getBackdoor(_backdoor);
}


bool
BwRegulatedSimpleMemory::fulfillRequest(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see read and writes at memory controller, "
             "saw %s to %#llx\n", pkt->cmdString(), pkt->getAddr());

    // we should not get a new request after committing to retry the
    // current one, but unfortunately the CPU violates this rule, so
    // simply ignore it for now
    if (retryReq)
        return false;

    // if we are busy with a read or write, remember that we have to
    // retry
    if (isBusy) {
        retryReq = true;
        return false;
    }

    // technically the packet only reaches us after the header delay,
    // and since this is a memory controller we also need to
    // deserialise the payload before performing any write operation
    Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;
    

    // update the release time according to the bandwidth limit, and
    // do so with respect to the time it takes to finish this request
    // rather than long term as it is the short term data rate that is
    // limited for any real memory

    // calculate an appropriate tick to release to not exceed
    // the bandwidth limit
    //Tick duration = pkt->getSize() * bandwidth;
    Tick duration = 0;
    //printf("Bandwidth inside is: %lld\n",bandwidth);
    // only consider ourselves busy if there is any need to wait
    // to avoid extra events being scheduled for (infinitely) fast
    // memories
    if (duration != 0) {
        schedule(releaseEvent, curTick() + duration);
        isBusy = true;
    }

    // go ahead and deal with the packet and put the response in the
    // queue if there is one
    bool needsResponse = pkt->needsResponse();
    recvAtomic(pkt);

    //printf("Is prefetch:%d!, Duration: %d\n",pkt->cmd.isPrefetch(),duration);
    if(pkt->isPrefetchPacket())
    {
        printf("Prefetch Request Detected!, Duration: %d\n",duration);
    }
    
    // turn packet around to go back to requestor if response expected
    if (needsResponse) {
        // recvAtomic() should already have turned packet into
        // atomic response
        assert(pkt->isResponse());

        Tick when_to_send = curTick() + receive_delay + getLatency();

        // typically this should be added at the end, so start the
        // insertion sort with the last element, also make sure not to
        // re-order in front of some existing packet with the same
        // address, the latter is important as this memory effectively
        // hands out exclusive copies (shared is not asserted)
        auto i = packetQueue.end();
        --i;
        while (i != packetQueue.begin() && when_to_send < i->tick &&
               !i->pkt->matchAddr(pkt))
            --i;

        // emplace inserts the element before the position pointed to by
        // the iterator, so advance it one step
        packetQueue.emplace(++i, pkt, when_to_send);

        if (!retryResp && !dequeueEvent.scheduled())
            schedule(dequeueEvent, packetQueue.back().tick);
    } else {
        pendingDelete.reset(pkt);
    }

    return true;
}
bool
BwRegulatedSimpleMemory::recvTimingReq(PacketPtr pkt)
{
    if(!isPrefetch(pkt))
    {
        if(demand_queues_pre_bucket[getRequestor(pkt)].size() < demand_queue_size)
        {
            std::cout<<"DEMAND PACKET PUSHED! from requestor: "<<getRequestor(pkt)<<" queue size is: "<<demand_queues_pre_bucket[getRequestor(pkt)].size() << "ACTUAL LIMIT: "<<demand_queue_size<<std::endl;
            
            // Create a queue element 
            packet_queue_element queue_element{pkt,curTick(),getRequestor(pkt),10};
            
            // Push the element into the demand queue
            demand_queues_pre_bucket[getRequestor(pkt)].push(queue_element);
            
            //return true;
        }        
    }
    return fulfillRequest(pkt);
}

void
BwRegulatedSimpleMemory::release()
{
    assert(isBusy);
    isBusy = false;
    if (retryReq) {
        retryReq = false;
        port.sendRetryReq();
    }
}

void
BwRegulatedSimpleMemory::dequeue()
{
    assert(!packetQueue.empty());
    DeferredPacket deferred_pkt = packetQueue.front();

    retryResp = !port.sendTimingResp(deferred_pkt.pkt);

    if (!retryResp) {
        packetQueue.pop_front();

        // if the queue is not empty, schedule the next dequeue event,
        // otherwise signal that we are drained if we were asked to do so
        if (!packetQueue.empty()) {
            // if there were packets that got in-between then we
            // already have an event scheduled, so use re-schedule
            reschedule(dequeueEvent,
                       std::max(packetQueue.front().tick, curTick()), true);
        } else if (drainState() == DrainState::Draining) {
            DPRINTF(Drain, "Draining of BwRegulatedSimpleMemory complete\n");
            signalDrainDone();
        }
    }
}

Tick
BwRegulatedSimpleMemory::getLatency() const
{
    return latency +
        (latency_var ? random_mt.random<Tick>(0, latency_var) : 0);
}

void
BwRegulatedSimpleMemory::recvRespRetry()
{
    assert(retryResp);

    dequeue();
}

Port &
BwRegulatedSimpleMemory::getPort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return AbstractMemory::getPort(if_name, idx);
    } else {
        return port;
    }
}

DrainState
BwRegulatedSimpleMemory::drain()
{
    if (!packetQueue.empty()) {
        DPRINTF(Drain, "BwRegulatedSimpleMemory Queue has requests, waiting to drain\n");
        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

BwRegulatedSimpleMemory::MemoryPort::MemoryPort(const std::string& _name,
                                     BwRegulatedSimpleMemory& _memory)
    : ResponsePort(_name), mem(_memory)
{ }

AddrRangeList
BwRegulatedSimpleMemory::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(mem.getAddrRange());
    return ranges;
}

Tick
BwRegulatedSimpleMemory::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return mem.recvAtomic(pkt);
}

Tick
BwRegulatedSimpleMemory::MemoryPort::recvAtomicBackdoor(
        PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    return mem.recvAtomicBackdoor(pkt, _backdoor);
}

void
BwRegulatedSimpleMemory::MemoryPort::recvFunctional(PacketPtr pkt)
{
    mem.recvFunctional(pkt);
}

void
BwRegulatedSimpleMemory::MemoryPort::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &backdoor)
{
    mem.recvMemBackdoorReq(req, backdoor);
}

bool
BwRegulatedSimpleMemory::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return mem.recvTimingReq(pkt);
}

void
BwRegulatedSimpleMemory::MemoryPort::recvRespRetry()
{
    mem.recvRespRetry();
}

} // namespace memory
} // namespace gem5
