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

#include "mem/latency_regulated_simple_mem.hh"

#include "base/random.hh"
#include "base/trace.hh"
#include "debug/Drain.hh"

namespace gem5
{

namespace memory
{

LatencyRegulatedSimpleMem::LatencyRegulatedSimpleMem(const LatencyRegulatedSimpleMemParams &p) :
    AbstractMemory(p),
    port(name() + ".port", *this), latency(p.latency),
    latency_var(p.latency_var), bandwidth(p.bandwidth), isBusy(false),
    requestors(p.requestors),
    is_fcfs(p.is_fcfs),
    ticks_per_cycle(p.ticks_per_cycle),
    retryReq(false), retryResp(false),
    remaining_ticks(0),
    releaseEvent([this]{ release(); }, name()),
    dequeueEvent([this]{ dequeue(); }, name())
    //handle_rr([this]{ handle_rr_queues(); }, name())
{
    demand_queues = new std::queue<packet_queue_element>[requestors];
    last_processed = 0;
    
    for(int i = 0; i < requestors; ++i)
    {
        global_rr_order.push(i);
    }
}

void
LatencyRegulatedSimpleMem::init()
{
    AbstractMemory::init();

    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }
}

Tick
LatencyRegulatedSimpleMem::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    access(pkt);
    return getLatency();
}

Tick
LatencyRegulatedSimpleMem::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    Tick latency = recvAtomic(pkt);
    getBackdoor(_backdoor);
    return latency;
}

void
LatencyRegulatedSimpleMem::recvFunctional(PacketPtr pkt)
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
LatencyRegulatedSimpleMem::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &_backdoor)
{
    getBackdoor(_backdoor);
}

bool
LatencyRegulatedSimpleMem::recvTimingReq(PacketPtr pkt)
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
    {
        //std::cout<<"RETRYING!!\n";
        return false;
    }
        

    // if we are busy with a read or write, remember that we have to
    // retry
    if (isBusy) {
        //std::cout<<"BUSY!!!\n";
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
    if (needsResponse) 
    {
        // recvAtomic() should already have turned packet into
        // atomic response
        assert(pkt->isResponse());

        //Tick when_to_send = curTick() + receive_delay + getLatency();
        if(is_fcfs)
        {
            Tick when_to_send = curTick();
            int requestor = getRequestor(pkt);
            packet_queue_element pkt_to_push = {pkt,when_to_send,0,0};
            std::cout<<"Address "<<pkt->getAddr()<<" requested by Core "<<requestor<<" entered the resource! at tick "<<curTick()<<std::endl;
            demand_queues[requestor].push(pkt_to_push);
            //std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            
            
            //            
            //std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            if(demand_queues[requestor].size() == 1)
            {
                std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                demand_queues[requestor].front().oldest_tik = curTick()+ticks_per_cycle;
                demand_queues[requestor].front().last_tick = curTick();
                //demand_queues[requestor].front().pkt_tick = curTick();
            }

            if (!retryResp && !dequeueEvent.scheduled())
            {
                std::cout<<"Scheduled!\n";
                remaining_ticks = 0;
                schedule(dequeueEvent, curTick()+ticks_per_cycle);
            }
        }
        else
        {
            Tick when_to_send = curTick();
            int requestor = getRequestor(pkt);
            packet_queue_element pkt_to_push = {pkt,0,0};
            std::cout<<"Address "<<pkt->getAddr()<<" requested by Core "<<requestor<<" entered the resource at Tick "<<curTick()<<std::endl;
            demand_queues[requestor].push(pkt_to_push);
            //std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            
            //std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            if(demand_queues[requestor].size() == 1)
            {
                std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                demand_queues[requestor].front().pkt_tick = curTick();
            }

            if (!retryResp && !dequeueEvent.scheduled())
            {
                std::cout<<"Scheduled!\n";
                remaining_ticks = 0;
                schedule(dequeueEvent, curTick()+ticks_per_cycle);
            }
                    
        }

        
    }
    else {
            pendingDelete.reset(pkt);
        }

    return true;
    

}

void
LatencyRegulatedSimpleMem::release()
{
    assert(isBusy);
    isBusy = false;
    if (retryReq) {
        retryReq = false;
        port.sendRetryReq();
    }
}

void
LatencyRegulatedSimpleMem::dequeue()
{
    if(is_fcfs)
    {
        int curr_requestor = -1;
        for(int i = 0; i < requestors; i++)
        {
            // Check if the current queue is not empty
            if (!demand_queues[i].empty())
            {
                // If curr_requestor is not set yet or the current requestor's pkt_tick is older
                if (curr_requestor == -1 || demand_queues[i].front().pkt_tick < demand_queues[curr_requestor].front().pkt_tick)
                {
                    curr_requestor = i;
                }
            }
        }



        

        std::cout<<"Curr requestor is: "<<curr_requestor<<std::endl;

        PacketPtr pkt = demand_queues[curr_requestor].front().pkt;
        retryResp = !port.sendTimingResp(pkt);
        
        if (!retryResp) {
            std::cout<<"Core "<<getRequestor(pkt)<<" processed address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            //last_processed = curTick();
            demand_queues[curr_requestor].pop();

            if( demand_queues[curr_requestor].size() > 0)
            {
                    std::cout<<"Core "<<curr_requestor<<" requested address: "<<demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                    demand_queues[curr_requestor].front().oldest_tik = curTick()+ticks_per_cycle;
                    demand_queues[curr_requestor].front().last_tick = curTick();
            }



            bool more_to_process = false;
            for(int i =0; i < requestors; i++)
            {
                if(demand_queues[i].size()>0)
                {
                    more_to_process = true;
                    break;
                }
            }

            // if the queue is not empty, schedule the next dequeue event,
            // otherwise signal that we are drained if we were asked to do so
            if (more_to_process) 
            {
                int curr_requestor = -1;
                for(int i = 0; i < requestors; i++)
                {
                    // Check if the current queue is not empty
                    if (!demand_queues[i].empty())
                    {
                        // If curr_requestor is not set yet or the current requestor's pkt_tick is older
                        if (curr_requestor == -1 || demand_queues[i].front().pkt_tick < demand_queues[curr_requestor].front().pkt_tick)
                        {
                            curr_requestor = i;
                        }
                    }
                }
                std::cout<<"More to process!\n";
                
                reschedule(dequeueEvent,std::max(curTick(),demand_queues[curr_requestor].front().oldest_tik), true);
                //reschedule(dequeueEvent,curTick()+ticks_per_cycle,true);
                
               
            } 
            else if (drainState() == DrainState::Draining) {
                DPRINTF(Drain, "Draining of LatencyRegulatedSimpleMem complete\n");
                signalDrainDone();
            }
        
        }
    


        

    }
    else
    {
        std::cout<<"HERE!\n";
        int curr_requestor = global_rr_order.front();
        

        while (demand_queues[curr_requestor].size() == 0)
        {
            std::cout<<"We poppin!\n";
            global_rr_order.pop();
            global_rr_order.push(curr_requestor);
            curr_requestor = global_rr_order.front();
        }

        PacketPtr pkt = demand_queues[curr_requestor].front().pkt;
        if((curTick() == demand_queues[curr_requestor].front().pkt_tick))
        {
            remaining_ticks = ticks_per_cycle;
            std::cout<<"RESCHEDULING CORE " <<curr_requestor<<" : Remaining ticks: "<<ticks_per_cycle<<" at tick: "<<curTick()<<std::endl;
            reschedule(dequeueEvent,curTick()+ticks_per_cycle, true);
        }
        else if((curTick() - demand_queues[curr_requestor].front().pkt_tick < ticks_per_cycle))
        {

            remaining_ticks = ticks_per_cycle-(curTick()- demand_queues[curr_requestor].front().pkt_tick);
            std::cout<<"RESCHEDULING CORE "<<curr_requestor<<" : Remaining ticks: "<<remaining_ticks<<" At tick: "<<curTick()<<"\n";
            reschedule(dequeueEvent,curTick()+remaining_ticks, true);
        }
        else
        {
            retryResp = !port.sendTimingResp(pkt);
            if(!retryResp)
            {
                std::cout<<"Core "<<curr_requestor<<" processed address: "<<demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                
                //Tick last_processed = curTick();
                
                demand_queues[curr_requestor].pop();
             

                if( demand_queues[curr_requestor].size() > 0)
                {
                    std::cout<<"Core "<<curr_requestor<<" requested address: "<<demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                    demand_queues[curr_requestor].front().pkt_tick = curTick();
                }
                
                global_rr_order.pop();
                global_rr_order.push(curr_requestor);
                

                bool more_to_process = false;
                for(int i =0; i < requestors; i++)
                {
                    if(demand_queues[i].size()>0)
                    {
                        more_to_process = true;
                        break;
                    }
                }
                if(more_to_process)
                {
                    std::cout<<"More to process!\n";
                    
                    //reschedule(dequeueEvent,curTick()+ticks_per_cycle, true);
                    if(remaining_ticks == 0)
                    {
                        reschedule(dequeueEvent,curTick()+ticks_per_cycle, true);
                    }
                    else
                    {
                        std::cout<<"EXTRA TICKS: "<<(ticks_per_cycle-remaining_ticks)<<std::endl;
                        reschedule(dequeueEvent,curTick()+(ticks_per_cycle-remaining_ticks), true);
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
LatencyRegulatedSimpleMem::getLatency() const
{
    return latency +
        (latency_var ? random_mt.random<Tick>(0, latency_var) : 0);
}

void
LatencyRegulatedSimpleMem::recvRespRetry()
{
    assert(retryResp);

    dequeue();
}

Port &
LatencyRegulatedSimpleMem::getPort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return AbstractMemory::getPort(if_name, idx);
    } else {
        return port;
    }
}

DrainState
LatencyRegulatedSimpleMem::drain()
{
    if (!packetQueue.empty()) {
        DPRINTF(Drain, "LatencyRegulatedSimpleMem Queue has requests, waiting to drain\n");
        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

LatencyRegulatedSimpleMem::MemoryPort::MemoryPort(const std::string& _name,
                                     LatencyRegulatedSimpleMem& _memory)
    : ResponsePort(_name), mem(_memory)
{ }

AddrRangeList
LatencyRegulatedSimpleMem::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(mem.getAddrRange());
    return ranges;
}

Tick
LatencyRegulatedSimpleMem::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return mem.recvAtomic(pkt);
}

Tick
LatencyRegulatedSimpleMem::MemoryPort::recvAtomicBackdoor(
        PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    return mem.recvAtomicBackdoor(pkt, _backdoor);
}

void
LatencyRegulatedSimpleMem::MemoryPort::recvFunctional(PacketPtr pkt)
{
    mem.recvFunctional(pkt);
}

void
LatencyRegulatedSimpleMem::MemoryPort::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &backdoor)
{
    mem.recvMemBackdoorReq(req, backdoor);
}

bool
LatencyRegulatedSimpleMem::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return mem.recvTimingReq(pkt);
}

void
LatencyRegulatedSimpleMem::MemoryPort::recvRespRetry()
{
    mem.recvRespRetry();
}

} // namespace memory
} // namespace gem5
