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

#include "mem/duetto_simple_mem.hh"

#include "base/random.hh"
#include "base/trace.hh"
#include "debug/Drain.hh"

namespace gem5
{

namespace memory
{

DuettoSimpleMem::DuettoSimpleMem(const DuettoSimpleMemParams &p) :
    AbstractMemory(p),
    port(name() + ".port", *this), latency(p.latency),
    latency_var(p.latency_var), bandwidth(p.bandwidth), isBusy(false),
    retryReq(false), retryResp(false),
    is_fcfs(p.is_fcfs),
    latency_in_ticks(p.latency_in_ticks),
    num_requestors(p.num_requestors),
    request_delta(p.request_delta),
    remaining_ticks(0),
    latency_slack(p.latency_slack),
    releaseEvent([this]{ release(); }, name()),
    dequeueEvent([this]{ dequeue(); }, name()),
    dispatchMemoryRequest([this]{ dispatch_memory_requests(); }, name())
{
    pre_bucket_demand_queues = new std::deque<packet_queue_element>[num_requestors];
    post_bucket_demand_queues = new std::deque<packet_queue_element>[num_requestors];

    pre_bucket_prefetch_queues = new std::deque<packet_queue_element>[num_requestors];
    post_bucket_prefetch_queues = new std::deque<packet_queue_element>[num_requestors];

    requestor_latency_counters = new int64_t[num_requestors];

    // Initialize the RR scheduling queue
    for(int i = 0; i < num_requestors; ++i)
    {
        round_robin_queue.push(i);
        
        // Initializing the latency counters
        // We are regulating all cores thats why we set all requestor counters to the slack value
        requestor_latency_counters[i] = latency_slack;
    }
}

void
DuettoSimpleMem::startup()
{
    schedule(dispatchMemoryRequest,curTick());
}

void
DuettoSimpleMem::init()
{
    AbstractMemory::init();

    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }
}

void
DuettoSimpleMem::dispatch_memory_requests()
{
    // If the post demand bucket has requests deceremrent the latency counters 
    for(int i = 0; i < num_requestors; i++)
    {
        if(post_bucket_demand_queues[i].size()>0)
        {
            requestor_latency_counters[i] -= 1;

            if(requestor_latency_counters[i] == 0)
            {
                std::cout<<"CORE "<<i<<" ADDRESS: "<<post_bucket_demand_queues[i].front().pkt->getAddr()<<" AT TICK: "<<curTick()<<" WARNING!"<<std::endl;
            }
        }
    }

    // Update the queue states
    for (int i = 0; i< num_requestors; i++)
    {
        // If a request exists in the pre-bucket queue, push it to the post-bucket queue for demand requests
        if(pre_bucket_demand_queues[i].size()>0)
        {
            post_bucket_demand_queues[i].push_back(pre_bucket_demand_queues[i].front());
            pre_bucket_demand_queues[i].pop_front();
            
            if(post_bucket_demand_queues[i].size() == 1)
            {
                PacketPtr pkt = post_bucket_demand_queues[i].front().pkt;
                
                post_bucket_demand_queues[i].front().pkt_tick = curTick();
                post_bucket_demand_queues[i].front().fcfs_tick = curTick() + latency_in_ticks;

                
                std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<" FCFS MODE: "<<is_fcfs<<std::endl;
            }
        }

        // If a request exists in the pre-bucket queue, push it to the post-bucket queue for prefetch requests
        if(pre_bucket_prefetch_queues[i].size()>0)
        {
            post_bucket_prefetch_queues[i].push_back(pre_bucket_prefetch_queues[i].front());
            pre_bucket_prefetch_queues[i].pop_front();
            
            if(post_bucket_prefetch_queues[i].size() == 1)
            {
                PacketPtr pkt = post_bucket_prefetch_queues[i].front().pkt;
                
                post_bucket_prefetch_queues[i].front().pkt_tick = curTick();
                post_bucket_prefetch_queues[i].front().fcfs_tick = curTick() + latency_in_ticks;

                
                std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<" FCFS MODE: "<<is_fcfs<<std::endl;
            }
        }
    }

    bool more_to_process = false;
    for(int i =0; i < num_requestors; i++)
    {
        if(is_fcfs)
        {
            if(post_bucket_demand_queues[i].size()>0 || post_bucket_prefetch_queues[i].size()>0)
            {
                more_to_process = true;
                break;
            }
        }
        else
        {
            if(post_bucket_demand_queues[i].size()>0)
            {
                more_to_process = true;
                break;
            }
        }

    }

    int curr_requestor = -1;
    bool is_prefetch = false;
    if(is_fcfs)
    {
        // Looking for the next requestor in all the queues in FCFS order  


        for (int i = 0; i < num_requestors; i++) 
        {
            // Check if the current demand queue is not empty
            if (!post_bucket_demand_queues[i].empty()) 
            {
                if (curr_requestor == -1 || post_bucket_demand_queues[i].front().arrival_tick < (is_prefetch ? post_bucket_prefetch_queues[curr_requestor].front().arrival_tick : post_bucket_demand_queues[curr_requestor].front().arrival_tick)) 
                {
                    curr_requestor = i;
                    is_prefetch = false; // Mark that the current selected requestor is from the demand queue
                }
            }
            
            // Check if the current prefetch queue is not empty
            if (!post_bucket_prefetch_queues[i].empty()) 
            {
                if (curr_requestor == -1 || post_bucket_prefetch_queues[i].front().arrival_tick < (is_prefetch ? post_bucket_prefetch_queues[curr_requestor].front().arrival_tick : post_bucket_demand_queues[curr_requestor].front().arrival_tick)) 
                {
                    curr_requestor = i;
                    is_prefetch = true; // Mark that the current selected requestor is from the prefetch queue
                }
            }
        }
    }

    // schedule the memory requests
    if (!retryResp && !dequeueEvent.scheduled() && more_to_process)
    {
        if(!is_prefetch)
        {
            post_bucket_demand_queues[curr_requestor].front().scheduled = true;
        }
        else
        {
            post_bucket_prefetch_queues[curr_requestor].front().scheduled = true;
        }
        std::cout<<"Scheduled!\n";
        remaining_ticks = 0;
        schedule(dequeueEvent, curTick()+latency_in_ticks);
    }

    for(int i = 0; i < num_requestors; i++)
    {
        if(requestor_latency_counters[i] <= 0)
        {
            //std::cout<<"OPERATING IN RTA MODE\n";
            // Swtich to RTA
            is_fcfs = false;
            break;
        }
        else
        {
            //std::cout<<"OPERATING IN HPA MODE\n";
            // Switch to HPA
            is_fcfs = true;
        }
    }

    schedule(dispatchMemoryRequest,cyclesToTicks(Cycles(curCycle()+1)));

}

Tick
DuettoSimpleMem::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    access(pkt);
    return getLatency();
}

Tick
DuettoSimpleMem::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    Tick latency = recvAtomic(pkt);
    getBackdoor(_backdoor);
    return latency;
}

void
DuettoSimpleMem::recvFunctional(PacketPtr pkt)
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
DuettoSimpleMem::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &_backdoor)
{
    getBackdoor(_backdoor);
}

bool
DuettoSimpleMem::recvTimingReq(PacketPtr pkt)
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
        //std::cout<<"WE ARE BUSY!\n";
        retryReq = true;
        return false;
    }

    // technically the packet only reaches us after the header delay,
    // and since this is a memory controller we also need to
    // deserialise the payload before performing any write operation
    //Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
    Tick receive_delay = 0;
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

        Tick arrival_tick = curTick();
        int requestor = getRequestor(pkt);
        packet_queue_element pkt_to_push = {pkt,arrival_tick,0,0,false,false};

        /*if(!isPrefetch(pkt))
        {  
            std::cout<<"DEMAND ADDRESS: "<<pkt->getAddr()<<std::endl;  
            for (const auto& temp_pkt : *pre_bucket_prefetch_queues) {
                if (temp_pkt.pkt->getAddr() == pkt->getAddr())
                    std::cout << "DEMAND ACCESS AFTER ISSUING PF FOUND IN PRE BUCKET" << std::endl;
                std::cout<<"PF ADDRESSES PREBUCK: "<<temp_pkt.pkt->getAddr()<<std::endl;
            }
            for (const auto& temp_pkt : *post_bucket_prefetch_queues) {
                if (temp_pkt.pkt->getAddr() == pkt->getAddr())
                    std::cout << "DEMAND ACCESS AFTER ISSUING PF FOUND IN POST BUCKET" << std::endl;
                std::cout<<"PF ADDRESSES POSTBUCK: "<<temp_pkt.pkt->getAddr()<<std::endl;
            }

            
        }*/

        // Push request to the pre bucket queue
        // TODO account for prefetch and demand requests seperately
        std::cout<<"Address "<<pkt->getAddr()<<" requested by Core "<<requestor<<" entered the resource at Tick "<<curTick()<<" and PREFETCH: "<<isPrefetch(pkt)<<std::endl;
        if(isPrefetch(pkt))
        {
            std::cout<<"Prefetch packet pushed!\n";
            pkt_to_push.is_prefetch = true;
            pre_bucket_prefetch_queues[requestor].push_back(pkt_to_push);
        }
        else
        {
            pkt_to_push.is_prefetch = false;
            pre_bucket_demand_queues[requestor].push_back(pkt_to_push);
        }
        


    } else {
        pendingDelete.reset(pkt);
    }

    return true;
}

void
DuettoSimpleMem::release()
{
    assert(isBusy);
    isBusy = false;
    if (retryReq) {
        retryReq = false;
        port.sendRetryReq();
    }
}

void
DuettoSimpleMem::dequeue()
{

    if(is_fcfs)
    {
        // Looking for the next requestor in all the queues in FCFS order  
        int curr_requestor = -1;
        bool is_prefetch = false;

        for (int i = 0; i < num_requestors; i++) 
        {
            // Check if the current demand queue is not empty
            if (!post_bucket_demand_queues[i].empty()) 
            {
                if (curr_requestor == -1 || post_bucket_demand_queues[i].front().arrival_tick < (is_prefetch ? post_bucket_prefetch_queues[curr_requestor].front().arrival_tick : post_bucket_demand_queues[curr_requestor].front().arrival_tick)) 
                {
                    curr_requestor = i;
                    is_prefetch = false; // Mark that the current selected requestor is from the demand queue
                }
            }
            
            // Check if the current prefetch queue is not empty
            if (!post_bucket_prefetch_queues[i].empty()) 
            {
                if (curr_requestor == -1 || post_bucket_prefetch_queues[i].front().arrival_tick < (is_prefetch ? post_bucket_prefetch_queues[curr_requestor].front().arrival_tick : post_bucket_demand_queues[curr_requestor].front().arrival_tick)) 
                {
                    curr_requestor = i;
                    is_prefetch = true; // Mark that the current selected requestor is from the prefetch queue
                }
            }
        }

        PacketPtr pkt;
        bool more_to_process = false;
        
        // Take packet from demand if its not prefetch and from prefetch otherwise
        if(!is_prefetch)
        {
            pkt = post_bucket_demand_queues[curr_requestor].front().pkt;
            
        }
        else
        {
            pkt = post_bucket_prefetch_queues[curr_requestor].front().pkt;
        }
                    
        // Try sending out the request
        retryResp = !port.sendTimingResp(pkt);

        if(!retryResp)
        {
            if(!is_prefetch)
            {
                std::cout<<"Core "<<getRequestor(pkt)<<" processed address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                post_bucket_demand_queues[curr_requestor].pop_front();
                requestor_latency_counters[curr_requestor] = std::min(latency_slack, requestor_latency_counters[curr_requestor]+int64_t(request_delta)); 
                
                if( post_bucket_demand_queues[curr_requestor].size() > 0)
                {   
                        std::cout<<"Core "<<curr_requestor<<" requested address: "<<post_bucket_demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<" FCFS MODE: "<<is_fcfs<<std::endl;
                        post_bucket_demand_queues[curr_requestor].front().fcfs_tick = curTick() + latency_in_ticks;
                        post_bucket_demand_queues[curr_requestor].front().pkt_tick = curTick();
                }
                
                // Check if there are more demand requests to process
                more_to_process = false;
                for(int i =0; i < num_requestors; i++)
                {
                    if(post_bucket_demand_queues[i].size()>0)
                    {
                        more_to_process = true;
                        break;
                    }
                }
            }
            else
            {
                std::cout<<"Core "<<getRequestor(pkt)<<" processed address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                post_bucket_prefetch_queues[curr_requestor].pop_front();
                requestor_latency_counters[curr_requestor] = std::min(latency_slack, requestor_latency_counters[curr_requestor]+int64_t(request_delta)); 
                
                if( post_bucket_prefetch_queues[curr_requestor].size() > 0)
                {   
                        std::cout<<"Core "<<curr_requestor<<" requested address: "<<post_bucket_prefetch_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<" FCFS MODE: "<<is_fcfs<<std::endl;
                        post_bucket_prefetch_queues[curr_requestor].front().fcfs_tick = curTick() + latency_in_ticks;
                        post_bucket_prefetch_queues[curr_requestor].front().pkt_tick = curTick();
                }
                
                // Check if there are more demand requests to process
                more_to_process = false;
                for(int i =0; i < num_requestors; i++)
                {
                    if(post_bucket_prefetch_queues[i].size()>0)
                    {
                        more_to_process = true;
                        break;
                    }
                }
            }

            // Schedule the remaining requests
            if (more_to_process) 
            {
                std::cout<<"More to process!\n";
                int curr_requestor = -1;
                bool is_prefetch = false;

                for (int i = 0; i < num_requestors; i++) 
                {
                    // Check if the current demand queue is not empty
                    if (!post_bucket_demand_queues[i].empty()) 
                    {
                        if (curr_requestor == -1 || post_bucket_demand_queues[i].front().arrival_tick < (is_prefetch ? post_bucket_prefetch_queues[curr_requestor].front().arrival_tick : post_bucket_demand_queues[curr_requestor].front().arrival_tick)) 
                        {
                            curr_requestor = i;
                            is_prefetch = false; // Mark that the current selected requestor is from the demand queue
                        }
                    }
                    
                    // Check if the current prefetch queue is not empty
                    if (!post_bucket_prefetch_queues[i].empty()) 
                    {
                        if (curr_requestor == -1 || post_bucket_prefetch_queues[i].front().arrival_tick < (is_prefetch ? post_bucket_prefetch_queues[curr_requestor].front().arrival_tick : post_bucket_demand_queues[curr_requestor].front().arrival_tick)) 
                        {
                            curr_requestor = i;
                            is_prefetch = true; // Mark that the current selected requestor is from the prefetch queue
                        }
                    }
                }
                
                if(!is_prefetch)
                {
                    post_bucket_demand_queues[curr_requestor].front().scheduled = true;
                }
                else
                {
                    post_bucket_prefetch_queues[curr_requestor].front().scheduled = true;
                }

                if(is_prefetch && is_fcfs)
                {
                    reschedule(dequeueEvent,std::max(curTick(),post_bucket_prefetch_queues[curr_requestor].front().fcfs_tick), true);
                }
                else
                {
                    reschedule(dequeueEvent,std::max(curTick(),post_bucket_demand_queues[curr_requestor].front().fcfs_tick), true);
                }

            } 
            else if (drainState() == DrainState::Draining) {
                DPRINTF(Drain, "Draining of LatencyRegulatedSimpleMem complete\n");
                signalDrainDone();
            }

        }
        else
        {
            std::cout<<"RETRYING!\n";
        }
    }
    else
    {
        int curr_requestor = round_robin_queue.front();
        

        while (post_bucket_demand_queues[curr_requestor].size() == 0)
        {
            std::cout<<"We poppin!\n";
            round_robin_queue.pop();
            round_robin_queue.push(curr_requestor);
            curr_requestor = round_robin_queue.front();
        }

        PacketPtr pkt = post_bucket_demand_queues[curr_requestor].front().pkt;
        if(curTick() == post_bucket_demand_queues[curr_requestor].front().pkt_tick)
        {
            remaining_ticks = latency_in_ticks;
            std::cout<<"RESCHEDULING CORE " <<curr_requestor<<" : Remaining ticks: "<<latency_in_ticks<<" at tick: "<<curTick()<<std::endl;
            reschedule(dequeueEvent,curTick()+latency_in_ticks, true);
        }
        else if((curTick() - post_bucket_demand_queues[curr_requestor].front().pkt_tick < latency_in_ticks))
        {

            remaining_ticks = latency_in_ticks-(curTick()- post_bucket_demand_queues[curr_requestor].front().pkt_tick);
            std::cout<<"RESCHEDULING CORE "<<curr_requestor<<" : Remaining ticks: "<<remaining_ticks<<" At tick: "<<curTick()<<"\n";
            reschedule(dequeueEvent,curTick()+remaining_ticks, true);
        }
        else
        {
            retryResp = !port.sendTimingResp(pkt);
            if(!retryResp)
            {
                std::cout<<"Core "<<curr_requestor<<" processed address: "<<post_bucket_demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                
                Tick last_processed = curTick();
                
                post_bucket_demand_queues[curr_requestor].pop_front();
                
                // Increment the latency counter of the requestor with the minimum between the slack and relative_deadline + old counter value
                std::cout<<"For Core "<<curr_requestor<<" Curr Latency counter: "<<requestor_latency_counters[curr_requestor]<<" curr+latency = "<<int64_t(requestor_latency_counters[curr_requestor]+ticksToCycles(latency_in_ticks))<<std::endl;
                requestor_latency_counters[curr_requestor] = std::min(latency_slack, requestor_latency_counters[curr_requestor]+int64_t(request_delta)); 
                std::cout<<"LATENCY IS: "<<requestor_latency_counters[curr_requestor]<<std::endl; 

                if( post_bucket_demand_queues[curr_requestor].size() > 0)
                {
                    std::cout<<"Core "<<curr_requestor<<" requested address: "<<post_bucket_demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<" FCFS MODE: "<<is_fcfs<<std::endl;
                    post_bucket_demand_queues[curr_requestor].front().pkt_tick = curTick();
                    post_bucket_demand_queues[curr_requestor].front().fcfs_tick = curTick() + latency_in_ticks;
                }
                
                round_robin_queue.pop();
                round_robin_queue.push(curr_requestor);
                

                bool more_to_process = false;
                for(int i =0; i < num_requestors; i++)
                {
                    if(post_bucket_demand_queues[i].size()>0)
                    {
                        more_to_process = true;
                        break;
                    }
                }
                if(more_to_process)
                {
                    std::cout<<"More to process!\n";
                    std::cout<<"Last processed: "<<last_processed<<std::endl;
                    

                    if(remaining_ticks == 0)
                    {
                        reschedule(dequeueEvent,curTick()+latency_in_ticks, true);
                    }
                    else
                    {
                        std::cout<<"EXTRA TICKS: "<<(latency_in_ticks-remaining_ticks)<<std::endl;
                        reschedule(dequeueEvent,curTick()+(latency_in_ticks-remaining_ticks), true);
                        remaining_ticks = 0;
                    }
                    
                    
                }
                
                else if(drainState() == DrainState::Draining)
                {
                    DPRINTF(Drain, "Draining of SimpleMemory complete\n");
                    signalDrainDone();
                }
            //}
        

            }
            else
            {
                std::cout<<"RETRYING!\n";
            }
            remaining_ticks = 0;
        

        }
    
    }


}

Tick
DuettoSimpleMem::getLatency() const
{
    return latency +
        (latency_var ? random_mt.random<Tick>(0, latency_var) : 0);
}

void
DuettoSimpleMem::recvRespRetry()
{
    assert(retryResp);

    dequeue();
}

Port &
DuettoSimpleMem::getPort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return AbstractMemory::getPort(if_name, idx);
    } else {
        return port;
    }
}

DrainState
DuettoSimpleMem::drain()
{
    if (!packetQueue.empty()) {
        DPRINTF(Drain, "DuettoSimpleMem Queue has requests, waiting to drain\n");
        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

DuettoSimpleMem::MemoryPort::MemoryPort(const std::string& _name,
                                     DuettoSimpleMem& _memory)
    : ResponsePort(_name), mem(_memory)
{ }

AddrRangeList
DuettoSimpleMem::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(mem.getAddrRange());
    return ranges;
}

Tick
DuettoSimpleMem::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return mem.recvAtomic(pkt);
}

Tick
DuettoSimpleMem::MemoryPort::recvAtomicBackdoor(
        PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    return mem.recvAtomicBackdoor(pkt, _backdoor);
}

void
DuettoSimpleMem::MemoryPort::recvFunctional(PacketPtr pkt)
{
    mem.recvFunctional(pkt);
}

void
DuettoSimpleMem::MemoryPort::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &backdoor)
{
    mem.recvMemBackdoorReq(req, backdoor);
}

bool
DuettoSimpleMem::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return mem.recvTimingReq(pkt);
}

void
DuettoSimpleMem::MemoryPort::recvRespRetry()
{
    mem.recvRespRetry();
}

} // namespace memory
} // namespace gem5
