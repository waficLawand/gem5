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
    retryReq(false), retryResp(false), 
    is_fcfs(p.is_fcfs),
    requestors(p.requestors),
    releaseEvent([this]{ release(); }, name()),
    dequeueEvent([this]{ dequeue(); }, name()),
    handle_rr([this]{ handle_rr_queues(); }, name())
{
    demand_queues = new std::queue<packet_queue_element>[requestors];

    for(int i = 0; i < requestors; ++i)
    {
        round_robin_sched_queue.push(i);
    }
}
void
LatencyRegulatedSimpleMem::startup()
{
    schedule(handle_rr,curTick());
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

void
LatencyRegulatedSimpleMem::handle_rr_queues()
{
    bool more_to_process = false;

    for(int i=0; i< requestors; i++)
    {
        if(demand_queues[i].size()!= 0)
            more_to_process = true;
    }
    //std::cout<<"PASSED  THIIS STAGE2!\n";
    if(more_to_process && !retryResp && !dequeueEvent.scheduled())
    {
        //std::cout<<"PASSED  THIIS STAGE!4\n";
        int curr_requestor = round_robin_sched_queue.front();

        while(demand_queues[curr_requestor].size() == 0)
        {
            round_robin_sched_queue.pop();
            round_robin_sched_queue.push(curr_requestor);
            curr_requestor = round_robin_sched_queue.front();
        }
        
        if(temp_queue.front() != demand_queues[curr_requestor].front().pkt->getAddr())
        {
            global_rr_order.push(curr_requestor);
            temp_queue.push(demand_queues[curr_requestor].front().pkt->getAddr());

            std::cout<<"PASSED  THIIS STAGE!10\n";
            fulfillRequest(demand_queues[curr_requestor].front().pkt,demand_queues[curr_requestor].front().pkt_tick);
            //std::cout<<"PASSED  THIIS STAGE!5\n";

            //schedule(dequeueEvent,std::max(demand_queues[curr_requestor].front().pkt_tick,curTick()));

            round_robin_sched_queue.pop();
            round_robin_sched_queue.push(curr_requestor);
        }
        
    }
    //std::cout<<"PASSED  THIIS STAGE!3\n";
    schedule(handle_rr,curTick()+1);
}


bool 
LatencyRegulatedSimpleMem::fulfillRequest(PacketPtr pkt, Tick tick)
{
    std::cout<<"PASSED  THIIS STAGE!6 at tick"<<curTick()<<"  ";
    std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<tick<<std::endl;

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
        std::cout<<"RETRY!!";
        return false;
    }
        

    // if we are busy with a read or write, remember that we have to
    // retry
    if (isBusy) {
        std::cout<<"BUSY!!";
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
            std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
        }
        // RR arbitration scheme
        else
        {

        }

        if (!retryResp && !dequeueEvent.scheduled())
        {
            if(is_fcfs)
            {
                schedule(dequeueEvent, packetQueue.back().tick);
            }
            else
            {
                std::cout<<"PASSED  THIIS STAGE!7\n";
                //schedule(handle_rr,curTick());
                schedule(dequeueEvent,std::max(tick,curTick()));

            }
            
        }
            
    } else {
        std::cout<<"OUT!\n";
        pendingDelete.reset(pkt);
    }

    return true;
}

bool
LatencyRegulatedSimpleMem::recvTimingReq(PacketPtr pkt)
{
    if(is_fcfs)
    {
        return fulfillRequest(pkt,curTick());
    }
    else
    {
        Tick when_to_send = curTick() + getLatency();
        int requestor = getRequestor(pkt);
        packet_queue_element pkt_to_push = {pkt,when_to_send,requestor,0};
        demand_queues[requestor].push(pkt_to_push);
        std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<" When to send: "<<pkt_to_push.pkt_tick - curTick()<<std::endl;
        return true;
    }
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
        assert(!packetQueue.empty());
    }
    else
    {
        //assert(demand_queues[round_robin_sched_queue.front()].size() != 0);
    }
    
    

    

    
    if(is_fcfs)
    {
        DeferredPacket fcfs_packet = packetQueue.front();
        retryResp = !port.sendTimingResp(fcfs_packet.pkt);
        if (!retryResp) 
        {
            
            std::cout<<"Core "<<getRequestor(fcfs_packet.pkt)<<" processed address: "<<fcfs_packet.pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;

            packetQueue.pop_front();

            // if the queue is not empty, schedule the next dequeue event,
            // otherwise signal that we are drained if we were asked to do so
            if (!packetQueue.empty()) {
                // if there were packets that got in-between then we
                // already have an event scheduled, so use re-schedule
                reschedule(dequeueEvent,
                        std::max(packetQueue.front().tick, curTick()), true);
            } else if (drainState() == DrainState::Draining) {
                DPRINTF(Drain, "Draining of LatencyRegulatedSimpleMem complete\n");
                signalDrainDone();
            }

        }

    }
    else
    {  
        std::cout<<"RR QUEUE: ";
            for(int i = 0; i< requestors; i++)
            {
                std::cout<<i<<" : ";
                if(demand_queues[i].size()!=0)
                {
                    std::cout<<"1 |";
                }
                else
                {
                    std::cout<<"0 |";
                }
            }
        std::cout<<"Round Robin Queue: ";
        std::queue<int> tempQueue = round_robin_sched_queue; // Make a copy of the queue
        
        while (!tempQueue.empty()) {
            std::cout << tempQueue.front() << " ";
            tempQueue.pop();
            }
        std::cout << std::endl;

        int curr_requestor = global_rr_order.front();
        packet_queue_element rr_packet = demand_queues[global_rr_order.front()].front();
        retryResp = !port.sendTimingResp(rr_packet.pkt);

        if(!retryResp)
        {
            std::cout<<"Core "<<getRequestor(rr_packet.pkt)<<" processed address: "<<rr_packet.pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            demand_queues[curr_requestor].pop();
            
            global_rr_order.pop();

            //std::cout<<"PASSED  THIIS STAGE!\n";

            //round_robin_sched_queue.pop();
            //round_robin_sched_queue.push(curr_requestor);
            // if the queue is not empty, schedule the next dequeue event,
            // otherwise signal that we are drained if we were asked to do so
            
            bool more_to_process = global_rr_order.size() > 0;

            /*if(more_to_process)
            {
                reschedule(dequeueEvent,
                        std::max(demand_queues[global_rr_order.front()].front().pkt_tick, curTick()), true);
            }*/
            if (!more_to_process && drainState() == DrainState::Draining) {
                DPRINTF(Drain, "Draining of LatencyRegulatedSimpleMem complete\n");
                signalDrainDone();
            }



        }
        else
        {
            std::cout<<"Will retry!\n";
        }

        
        /*int curr_requestor = global_rr_order.front();
        std::cout<<"RR QUEUE: ";
            for(int i = 0; i< requestors; i++)
            {
                std::cout<<i<<" : ";
                if(demand_queues[i].size()!=0)
                {
                    std::cout<<"1 |";
                }
                else
                {
                    std::cout<<"0 |";
                }
            }
        std::cout<<"Round Robin Queue: ";
        std::queue<int> tempQueue = round_robin_sched_queue; // Make a copy of the queue
        while (!tempQueue.empty()) {
            std::cout << tempQueue.front() << " ";
            tempQueue.pop();
            }
        std::cout << std::endl;
            
        
        packet_queue_element rr_packet = demand_queues[global_rr_order.front()].front();
        retryResp = !port.sendTimingResp(rr_packet.pkt);

        if(!retryResp)
        {
            std::cout<<"Core "<<getRequestor(rr_packet.pkt)<<" processed address: "<<rr_packet.pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            demand_queues[curr_requestor].pop();
            
            global_rr_order.pop();

            //round_robin_sched_queue.pop();
            //round_robin_sched_queue.push(curr_requestor);
            // if the queue is not empty, schedule the next dequeue event,
            // otherwise signal that we are drained if we were asked to do so
            
            bool more_to_process = false;
            
            for (int requestor = 0; requestor!= requestors; requestor++)
            {
                if (!demand_queues[requestor].empty()) 
                {
                    more_to_process = true;
                    // if there were packets that got in-between then we
                    // already have an event scheduled, so use re-schedule
                }
            }
            if (!more_to_process && drainState() == DrainState::Draining) {
                DPRINTF(Drain, "Draining of LatencyRegulatedSimpleMem complete\n");
                signalDrainDone();
            }

        }
        else
        {
            std::cout<<"WILL RETRY!\n";
        }*/

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



// TEMP 2/////////////////////////////////////////////////////////////////////////////////////////


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
    releaseEvent([this]{ release(); }, name()),
    dequeueEvent([this]{ dequeue(); }, name()),
    handle_rr([this]{ handle_rr_queues(); }, name())
{
    demand_queues = new std::queue<packet_queue_element>[requestors];
    
    for(int i = 0; i < requestors; ++i)
    {
        global_rr_order.push(i);
    } 
}

void
LatencyRegulatedSimpleMem::startup()
{
    if(!is_fcfs)
    {
        //schedule(handle_rr,curTick());
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

void
LatencyRegulatedSimpleMem::handle_rr_queues()
{
       
    int curr_requestor = global_rr_order.front();
    while (demand_queues[curr_requestor].size() == 0)
    {
        global_rr_order.pop();
        global_rr_order.push(curr_requestor);
        curr_requestor = global_rr_order.front();
    }


    fulfillRequest(demand_queues[curr_requestor].front().pkt,curTick(),true,0);

    //global_rr_queue.push(demand_queues[curr_requestor].front());
    demand_queues[curr_requestor].pop();
    
    if(demand_queues[curr_requestor].size() > 0)
    {
        std::cout<<"Core "<<curr_requestor<<" requested address: "<<demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
    }

    //std::cout<<"Address is : "<<global_rr_queue.front().pkt->getAddr()<<"\n";
    
    

    //std::cout<<"CURRENT TURN: "<<global_rr_order.front()<<"\n";
    global_rr_order.pop();
    global_rr_order.push(curr_requestor);
    //std::cout<<"NEXT TURN: "<<global_rr_order.front()<<"\n";

        

    bool more_to_process = false;
    for(int i =0; i < requestors; i++)
    {
        if(demand_queues[i].size()>0)
        {
            more_to_process = true;
            break;
        }
    }
    /*if (more_to_process) {
        std::cout << "Scheduling handle_rr at tick: " << curTick() + ticks_per_cycle <<" Curr tick is: "<<curTick()<<" and ticks per cycle is: "<<ticks_per_cycle<< std::endl;
        schedule(handle_rr, curTick() + ticks_per_cycle);
    }*/
    
}
bool
LatencyRegulatedSimpleMem::fulfillRequest(PacketPtr pkt,Tick tick, bool needsResponse,Tick receive_delay)
{

    // turn packet around to go back to requestor if response expected
    if (needsResponse) {
        // recvAtomic() should already have turned packet into
        // atomic response
        assert(pkt->isResponse());
        
        Tick when_to_send;
        
        if(is_fcfs)
        {
            when_to_send = curTick() + receive_delay + getLatency();
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

            std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            //std::cout<<"First requestor to send out: "<<global_fcfs_order.front()<<std::endl;

        }
        else
        {
            packet_queue_element packet_to_push = {pkt,curTick()};
            global_rr_queue.push(packet_to_push);
            //std::cout<<"Core "<<getRequestor(pkt)<<" pushed packet with address "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
        }
        
        
        if (!retryResp && !dequeueEvent.scheduled())
        {
            if(is_fcfs)
            {
                schedule(dequeueEvent, packetQueue.back().tick);
            }
            else
            {
                schedule(dequeueEvent, global_rr_queue.front().pkt_tick);
                //global_rr_queue.front().scheduled = true;
                //std::cout<<"Address : "<<global_rr_queue.front().pkt->getAddr()<<" scheduled! at tick: "<<curTick()<<"\n";

                //std::cout<<"Global RR queue0: "<<global_rr_queue.front().scheduled<<std::endl;
            }
        }
            
    } else {
        //std::cout<<"IME HEREE!!\n";
        pendingDelete.reset(pkt);
    }

    return true;

    
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

    if(is_fcfs)
    {
        //std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
        return fulfillRequest(pkt,curTick(),needsResponse,receive_delay);
    }
    else
    {
        if (needsResponse)
        {
            Tick when_to_send = curTick();
            int requestor = getRequestor(pkt);
            packet_queue_element pkt_to_push = {pkt,when_to_send};
            demand_queues[requestor].push(pkt_to_push);
            
            /*if(!handle_rr.scheduled())   
            {
                std::cout << "Scheduling handle_rr at tick: " << curTick() + ticks_per_cycle <<" Curr tick is: "<<curTick()<<" and ticks per cycle is: "<<ticks_per_cycle<< std::endl;
                schedule(handle_rr,curTick()+ticks_per_cycle);
            }*/

            if(demand_queues[requestor].size() == 1)
            {
                std::cout<<"Core "<<getRequestor(pkt)<<" requested address: "<<pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            }
            
        
            if (!retryResp && !dequeueEvent.scheduled())
            {
                schedule(dequeueEvent, curTick()+ticks_per_cycle);
            }
            
            
        }
        else
        {
            //std::cout<<"IM HEREEEEEEEEREEEEEREREREREEE~~~~!!!\n";
            pendingDelete.reset(pkt);
        }
            
            return true;
        
    }
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
        assert(!packetQueue.empty());
        DeferredPacket deferred_pkt = packetQueue.front();

        retryResp = !port.sendTimingResp(deferred_pkt.pkt);

        if (!retryResp) {
            std::cout<<"Core "<<getRequestor(deferred_pkt.pkt)<<" processed address: "<<deferred_pkt.pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            packetQueue.pop_front();

            // if the queue is not empty, schedule the next dequeue event,
            // otherwise signal that we are drained if we were asked to do so
            if (!packetQueue.empty()) {
                 //std::cout<<"RESCHEDULING!\n";
                // if there were packets that got in-between then we
                // already have an event scheduled, so use re-schedule
                reschedule(dequeueEvent,
                        std::max(packetQueue.front().tick, curTick()), true);
            } else if (drainState() == DrainState::Draining) {
                DPRINTF(Drain, "Draining of SimpleMemory complete\n");
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
            global_rr_order.pop();
            global_rr_order.push(curr_requestor);
            curr_requestor = global_rr_order.front();
        }

        PacketPtr pkt = demand_queues[curr_requestor].front().pkt;

        /*if(curTick()- demand_queues[curr_requestor].front().pkt_tick < ticks_per_cycle)
        {

            int remaining_ticks = curTick()- demand_queues[curr_requestor].front().pkt_tick;
            std::cout<<"RESCHEDULING: Remaining ticks: "<<remaining_ticks<<"\n";
            reschedule(dequeueEvent,curTick()+remaining_ticks, true);
        }
        else
        {*/
            retryResp = !port.sendTimingResp(pkt);
            if(!retryResp)
            {
                std::cout<<"Core "<<curr_requestor<<" processed address: "<<demand_queues[curr_requestor].front().pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
                demand_queues[curr_requestor].pop();
                global_rr_order.pop();
                global_rr_order.push(curr_requestor);
                
                if( demand_queues[curr_requestor].size() > 0)
                {
                    std::cout<<"Core "<<curr_requestor<<" requested address: "<<demand_queues[curr_requestor].front().pkt_tick<<" at tick: "<<curTick()<<std::endl;
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
                if(more_to_process)
                {
                    reschedule(dequeueEvent,curTick()+ticks_per_cycle, true);
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


        //assert(!packetQueue.empty());
        /*PacketPtr deferred_pkt = global_rr_queue.front().pkt;
        retryResp = !port.sendTimingResp(deferred_pkt);

        if (!retryResp) {
            std::cout<<"Core "<<getRequestor(deferred_pkt)<<" processed address: "<<deferred_pkt->getAddr()<<" at tick: "<<curTick()<<std::endl;
            //std::cout<<"Global RR queue1: "<<global_rr_queue.front().scheduled<<std::endl;
            global_rr_queue.pop();
            

            // if the queue is not empty, schedule the next dequeue event,
            // otherwise signal that we are drained if we were asked to do so
            if (!global_rr_queue.empty()) {
                //std::cout<<"Global RR queue2: "<<global_rr_queue.front().scheduled<<std::endl;
                
                std::cout<<"RESCHEDULING!\n";
                reschedule(dequeueEvent,std::max(global_rr_queue.front().pkt_tick+ticks_per_cycle, curTick()), true);
                
                // if there were packets that got in-between then we
                // already have an event scheduled, so use re-schedule


            
            }
            else if (drainState() == DrainState::Draining) {
                DPRINTF(Drain, "Draining of SimpleMemory complete\n");
                signalDrainDone();
            }
        }

        else
        {
            std::cout<<"RETRYING!\n";
        }
    */
    
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
