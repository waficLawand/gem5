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

#include "mem/latency_bw_regulated_simple_mem.hh"

#include "base/random.hh"
#include "base/trace.hh"
#include "debug/Drain.hh"

namespace gem5
{

namespace memory
{

LatencyBwRegulatedSimpleMem::LatencyBwRegulatedSimpleMem(const LatencyBwRegulatedSimpleMemParams &p) :
    AbstractMemory(p),
    port(name() + ".port", *this), latency(p.latency),
    latency_var(p.latency_var), bandwidth(p.bandwidth), isBusy(false), 
    retryReq(false), retryResp(false), 
    hpa_mode(true),
    demand_burstiness(p.demand_burstiness),
    prefetch_burstiness(p.prefetch_burstiness),
    refill_tokens(p.refill_tokens),
    refill_period(p.refill_period),
    requestors(p.requestors),
    demand_queue_size(p.demand_queue_size),
    prefetch_queue_size(p.prefetch_queue_size),
    requestor_queue_size(p.requestor_queue_size),
    initial_slack(p.initial_slack),
    ticks_per_cycle(p.ticks_per_cycle),
    start_with_full_buckets(p.start_with_full_buckets),
    enable_bw_regulation(p.enable_bw_regulation),
    is_fcfs(p.is_fcfs),
    releaseEvent([this]{ release(); }, name()),
    dequeueEvent([this]{ dequeue(); }, name()),
    monitoring_event([this]{bucket_refill();},name()),
    //arbiter_mode_event([this]{switch_arbiter_mode();},name()),
    //update_resource_counters([this]{add_sub_counters();},name()),
    handle_fcfs([this]{handle_global_fcfs();},name()),
    fill_global_fcfs([this]{fill_global_fcfs_queue();},name()),
    handle_pkts([this]{handle_pkts_in_queues();},name()),
    rr_arbitration([this]{fill_global_rr_queue();},name())
{
    demand_tokens = new int64_t[requestors];
    prefetch_tokens = new int64_t[requestors];

    demand_queues_pre_bucket = new std::queue<packet_queue_element>[requestors];
    demand_queues_post_bucket = new std::queue<packet_queue_element>[requestors];

    prefetch_queues_pre_bucket = new std::queue<packet_queue_element>[requestors];
    prefetch_queues_post_bucket = new std::queue<packet_queue_element>[requestors];

    requestor_queues = new std::queue<packet_queue_element>[requestors];

    latency_counters = new int64_t[requestors];

    global_fcfs_queue = new std::queue<packet_queue_element>;
    global_rr_queue = new std::queue<packet_queue_element>;

    // Initializing demand and prefetch token buckets with zeros
    for(int i = 0; i < requestors; ++i)
    {
        round_robin_sched_queue.push(i); // Initialize the round robin queue
        latency_counters[i] = initial_slack; // Initialize all latency counters with the slack value
        if(start_with_full_buckets)
        {
            prefetch_tokens[i] = prefetch_burstiness;
            demand_tokens[i] = demand_burstiness;
        }
        else
        {
            prefetch_tokens[i] = 0;
            demand_tokens[i] = 0;
        }

    }

    std::queue<int> tempQueue = round_robin_sched_queue; // Make a copy of the queue
    while (!tempQueue.empty()) {
        std::cout << tempQueue.front() << " ";
        tempQueue.pop();
    }
    std::cout << std::endl;
}



void
LatencyBwRegulatedSimpleMem::init()
{
    AbstractMemory::init();

    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }
}

void
LatencyBwRegulatedSimpleMem::startup()
{
    schedule(monitoring_event,curTick());
    //schedule(arbiter_mode_event,curTick());
    //schedule(update_resource_counters,curTick());
    //schedule(handle_fcfs,curTick());
    schedule(handle_pkts,curTick());
    if(is_fcfs)
    {
        schedule(fill_global_fcfs,curTick());
    }
    else
    {
        //schedule(rr_arbitration,curTick());
    }

    
}

/*void
LatencyBwRegulatedSimpleMem::add_sub_counters()
{
    // Iterate through the finished list and process each element
    for (auto it = finished_list.begin(); it != finished_list.end(); ) 
    {
        // Add relative deadline to the requestor's latency counter
        latency_counters[it->requestor_id] += it->relative_deadline;

        // Erase the element and move to the next one
        it = finished_list.erase(it);
    }

    // If there are outstanding requests, subtract one from the resources counter
    for(int requestor = 0; requestor < requestors; ++requestor)
    {  
        if(demand_queues_post_bucket[requestor].size() > 0)
        {
            latency_counters[requestor] -= 1;    
        }
    }
    
    schedule(update_resource_counters, ticks_per_cycle + curTick());

}*/

/*void
LatencyBwRegulatedSimpleMem::switch_arbiter_mode()
{
    for(int requestor = 0; requestor < requestors; ++requestor)
    {   
        // Switch to RTA
        if(latency_counters[requestor] <= 0)
        {
            hpa_mode = false;
            schedule(arbiter_mode_event,ticks_per_cycle + curTick());
            return;
        }
    }
    // Switch to HPA
    hpa_mode = true;
    schedule(arbiter_mode_event,ticks_per_cycle + curTick());
}*/


void
LatencyBwRegulatedSimpleMem::consume_demand_token(int requestor)
{
    --demand_tokens[requestor];
}

void
LatencyBwRegulatedSimpleMem::bucket_refill()
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


    schedule(monitoring_event, curTick() + refill_period*ticks_per_cycle);
    
}

void
LatencyBwRegulatedSimpleMem::handle_global_fcfs()
{
    if(global_fcfs_queue->size() > 0)
    {
        PacketPtr next_packet = global_fcfs_queue->front().pkt;
        
        //std::cout<<"About to process packet from: "<<global_fcfs_queue->front().requestor_id<<std::endl;
        //std::cout<<"Packet address is "<<global_fcfs_queue->front().pkt->getAddr()<<std::endl;

        bool packet_processed = fulfillReq(next_packet);

        //std::cout<<"Processed!\n";

        // Check if packet was successfully delivered
        if(packet_processed)
        {
            // If so pop the packet from the queue
           
            global_fcfs_queue->pop();
        }
        else
        {
            //std::cout<<"FAILED\n";
        }
    }


    schedule(handle_fcfs,curTick()+ticks_per_cycle);

}

void
LatencyBwRegulatedSimpleMem::fill_global_fcfs_queue()
{
    
    Tick most_recent_global = 9223372036854775807;
    packet_queue_element  * most_recent_packet = NULL;


    // Extract the most recent packet across demand and prefetch queues and across cores and push it to the global FCFS queue
    for(int requestor = 0; requestor < requestors; ++requestor)
    {
        // Packets exist in prefetch and demand queues
        if(demand_queues_post_bucket[requestor].size() > 0 && prefetch_queues_post_bucket[requestor].size() > 0)
        {
            Tick most_recent_demand = demand_queues_post_bucket[requestor].front().pkt_tick ;
            Tick most_recent_prefetch = prefetch_queues_post_bucket[requestor].front().pkt_tick;
        
            if(most_recent_demand <= most_recent_prefetch)
            {
                if(most_recent_demand < most_recent_global)
                {
                    most_recent_global = most_recent_demand;
                    most_recent_packet =  &demand_queues_post_bucket[requestor].front();
                }
            }
            else
            {
                if(most_recent_prefetch < most_recent_global)
                {
                    most_recent_global = most_recent_prefetch; 
                    most_recent_packet = &prefetch_queues_post_bucket[requestor].front();
                }
            }
        }

        // Packets exist only in the demand queue
        else if(demand_queues_post_bucket[requestor].size() > 0)
        {
            Tick most_recent_demand = demand_queues_post_bucket[requestor].front().pkt_tick ;
            if(most_recent_demand < most_recent_global)
            {
                most_recent_global = most_recent_demand;
                most_recent_packet =  &demand_queues_post_bucket[requestor].front();
            }

        }

        // Packets exist only in the prefetch queue
        else if(prefetch_queues_post_bucket[requestor].size() > 0)
        {
            Tick most_recent_prefetch = prefetch_queues_post_bucket[requestor].front().pkt_tick;
            if(most_recent_prefetch < most_recent_global)
            {
                most_recent_global = most_recent_prefetch; 
                most_recent_packet = &prefetch_queues_post_bucket[requestor].front();
            }
        }
        
    }

    if (most_recent_packet != NULL)
    {
        //std::cout<<"Packet pushed is requested by: "<< most_recent_packet->requestor_id<<std::endl;

        // Push most recent packet to the global FCFS queue
        //global_fcfs_queue->push(*most_recent_packet);

        bool result = fulfillReq(most_recent_packet->pkt); 

        if(result)
        {
            if(isPrefetch(most_recent_packet->pkt))
            {
                //std::cout<<"Prefetch Packet poped!"<<std::endl;
                prefetch_queues_post_bucket[most_recent_packet->requestor_id].pop();
            }
            else
            {
                std::cout<<"Core "<<most_recent_packet->requestor_id<<" processed address: "<<demand_queues_post_bucket[most_recent_packet->requestor_id].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                demand_queues_post_bucket[most_recent_packet->requestor_id].pop();
                //std::cout<<"Demand Packet poped!"<<std::endl;
            }
        }

    }

    schedule(fill_global_fcfs,curTick()+ticks_per_cycle);
}

void
LatencyBwRegulatedSimpleMem::fill_global_rr_queue()
{
    // Extract turn from the round robin queue
    int curr_turn = round_robin_sched_queue.front();

    if(demand_queues_post_bucket[curr_turn].size() != 0)
    {
        // Push packet to the global Round Robin Queue
        //global_rr_queue->push(demand_queues_post_bucket[curr_turn].front());
        PacketPtr packet = demand_queues_post_bucket[curr_turn].front().pkt;

        bool result = fulfillReq(packet);

        if(result)
        {
            std::cout<<"Core "<<curr_turn<<" processed address: "<<demand_queues_post_bucket[curr_turn].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            demand_queues_post_bucket[curr_turn].pop();
            
        }
        else
        {
            schedule(rr_arbitration,curTick()+ticks_per_cycle);
            return;
        }
    }

    // Pop curr turn from the queue and push it to the end of the RR queue
    round_robin_sched_queue.pop();
    round_robin_sched_queue.push(curr_turn);

    schedule(rr_arbitration,curTick()+ticks_per_cycle);

}

void
LatencyBwRegulatedSimpleMem::handle_pkts_in_queues()
{
    for(int requestor = 0; requestor < requestors; ++requestor)
    {
        // Check if requests exist in the queues before the token bucket
        if(demand_queues_pre_bucket[requestor].size() != 0)
        {
            if(demand_tokens[requestor] > 0)
            {
                /*TODO Check for queue size if needed in the future. Right now, we assume that queues will not overflow*/
                if(enable_bw_regulation)
                    demand_tokens[requestor]--;

                // Extract the front element in the queue
                packet_queue_element front_pkt = demand_queues_pre_bucket[requestor].front();
                
                // Push pkt to post bucket queue
                demand_queues_post_bucket[requestor].push(front_pkt);

                std::cout<<"Core "<<requestor<<" requested address: "<<front_pkt.pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                
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
                if(enable_bw_regulation)
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
    schedule(handle_pkts,curTick()+ticks_per_cycle);
}

Tick
LatencyBwRegulatedSimpleMem::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    access(pkt);
    return getLatency();
}

Tick
LatencyBwRegulatedSimpleMem::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    Tick latency = recvAtomic(pkt);
    getBackdoor(_backdoor);
    return latency;
}

void
LatencyBwRegulatedSimpleMem::recvFunctional(PacketPtr pkt)
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
LatencyBwRegulatedSimpleMem::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &_backdoor)
{
    getBackdoor(_backdoor);
}

bool 
LatencyBwRegulatedSimpleMem::fulfillReq(PacketPtr pkt)
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

    
    // turn packet around to go back to requestor if response expected
    if (needsResponse) {
        // recvAtomic() should already have turned packet into
        // atomic response
        assert(pkt->isResponse());

        Tick when_to_send = curTick() + receive_delay + getLatency();
        if(is_fcfs)
        {
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
        }

        else
        {
            auto i = packetQueue.end();
            packetQueue.emplace(i, pkt, when_to_send);

        }


        if (!retryResp && !dequeueEvent.scheduled())
            schedule(dequeueEvent, packetQueue.back().tick);
    } else {
        pendingDelete.reset(pkt);
    }

    return true;
}

bool
LatencyBwRegulatedSimpleMem::recvTimingReq(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see read and writes at memory controller, "
             "saw %s to %#llx\n", pkt->cmdString(), pkt->getAddr());
    
    //std::cout<<"Requestor is "<<_system->getRequestorName(pkt->req->requestorId()).c_str()<<std::endl;
    
    if(isPrefetch(pkt))
    {
        if(prefetch_queues_pre_bucket[getRequestor(pkt)].size() < prefetch_queue_size)
        {
            //std::cout<<"PREFETCH PACKET PUSHED!"<<std::endl;
            
            // Create a queue element 
            packet_queue_element queue_element{pkt,curTick(),getRequestor(pkt),10};
            
            // Push the element into the prefetch queue
            prefetch_queues_pre_bucket[getRequestor(pkt)].push(queue_element);
            
            return true;
        }
        else
        {
            retryReq = true;
            std::cout<<"PREFTCH QUEUE IS FULL!"<<std::endl;
            return false;
        }
    }
    else
    {
        if(demand_queues_pre_bucket[getRequestor(pkt)].size() < demand_queue_size)
        {
            //std::cout<<"DEMAND PACKET PUSHED! from requestor: "<<getRequestor(pkt)<<" queue size is: "<<demand_queues_pre_bucket[getRequestor(pkt)].size() << "ACTUAL LIMIT: "<<demand_queue_size<<std::endl;
            
            // Create a queue element 
            packet_queue_element queue_element{pkt,curTick(),getRequestor(pkt),10};
            
            // Push the element into the demand queue
            demand_queues_pre_bucket[getRequestor(pkt)].push(queue_element);
            
            return true;
        }        
           
        
        else
        {
            retryReq = true;
            std::cout<<"DEMAND QUEUE IS FULL!"<<std::endl;
            return false;
        }
        
    }

}

void
LatencyBwRegulatedSimpleMem::release()
{
    assert(isBusy);
    isBusy = false;
    if (retryReq) {
        retryReq = false;
        port.sendRetryReq();
    }
}

void
LatencyBwRegulatedSimpleMem::dequeue()
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
            DPRINTF(Drain, "Draining of LatencyBwRegulatedSimpleMem complete\n");
            signalDrainDone();
        }
    }
}

Tick
LatencyBwRegulatedSimpleMem::getLatency() const
{
    return latency +
        (latency_var ? random_mt.random<Tick>(0, latency_var) : 0);
}

void
LatencyBwRegulatedSimpleMem::recvRespRetry()
{
    assert(retryResp);

    dequeue();
}

Port &
LatencyBwRegulatedSimpleMem::getPort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return AbstractMemory::getPort(if_name, idx);
    } else {
        return port;
    }
}

DrainState
LatencyBwRegulatedSimpleMem::drain()
{
    if (!packetQueue.empty()) {
        DPRINTF(Drain, "LatencyBwRegulatedSimpleMem Queue has requests, waiting to drain\n");
        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

LatencyBwRegulatedSimpleMem::MemoryPort::MemoryPort(const std::string& _name,
                                     LatencyBwRegulatedSimpleMem& _memory)
    : ResponsePort(_name), mem(_memory)
{ }

AddrRangeList
LatencyBwRegulatedSimpleMem::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(mem.getAddrRange());
    return ranges;
}

Tick
LatencyBwRegulatedSimpleMem::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return mem.recvAtomic(pkt);
}

Tick
LatencyBwRegulatedSimpleMem::MemoryPort::recvAtomicBackdoor(
        PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    return mem.recvAtomicBackdoor(pkt, _backdoor);
}

void
LatencyBwRegulatedSimpleMem::MemoryPort::recvFunctional(PacketPtr pkt)
{
    mem.recvFunctional(pkt);
}

void
LatencyBwRegulatedSimpleMem::MemoryPort::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &backdoor)
{
    mem.recvMemBackdoorReq(req, backdoor);
}

bool
LatencyBwRegulatedSimpleMem::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return mem.recvTimingReq(pkt);
}

void
LatencyBwRegulatedSimpleMem::MemoryPort::recvRespRetry()
{
    mem.recvRespRetry();
}

} // namespace memory
} // namespace gem5
