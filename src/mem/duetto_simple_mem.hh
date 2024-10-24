/*
 * Copyright (c) 2012-2013 ARM Limited
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

/**
 * @file
 * DuettoSimpleMem declaration
 */

#ifndef __MEM_SIMPLE_MEMORY_HH__
#define __MEM_SIMPLE_MEMORY_HH__

#include <list>
#include<deque>
#include<queue>
#include "mem/abstract_mem.hh"
#include "mem/port.hh"
#include "params/DuettoSimpleMem.hh"

namespace gem5
{

namespace memory
{

/**
 * The simple memory is a basic single-ported memory controller with
 * a configurable throughput and latency.
 *
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 */
class DuettoSimpleMem : public AbstractMemory
{

  private:

    /**
     * A deferred packet stores a packet along with its scheduled
     * transmission time
     */
    class DeferredPacket
    {

      public:

        const Tick tick;
        const PacketPtr pkt;

        DeferredPacket(PacketPtr _pkt, Tick _tick) : tick(_tick), pkt(_pkt)
        { }
    };

    class MemoryPort : public ResponsePort
    {
      private:
        DuettoSimpleMem& mem;

      public:
        MemoryPort(const std::string& _name, DuettoSimpleMem& _memory);

      protected:
        Tick recvAtomic(PacketPtr pkt) override;
        Tick recvAtomicBackdoor(
                PacketPtr pkt, MemBackdoorPtr &_backdoor) override;
        void recvFunctional(PacketPtr pkt) override;
        void recvMemBackdoorReq(const MemBackdoorReq &req,
                MemBackdoorPtr &backdoor) override;
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
        AddrRangeList getAddrRanges() const override;
    };

    MemoryPort port;

    /**
     * Latency from that a request is accepted until the response is
     * ready to be sent.
     */
    const Tick latency;

    /**
     * Fudge factor added to the latency.
     */
    const Tick latency_var;

    /**
     * Internal (unbounded) storage to mimic the delay caused by the
     * actual memory access. Note that this is where the packet spends
     * the memory latency.
     */
    std::list<DeferredPacket> packetQueue;

    /**
     * Bandwidth in ticks per byte. The regulation affects the
     * acceptance rate of requests and the queueing takes place after
     * the regulation.
     */
    const double bandwidth;

    /**
     * Track the state of the memory as either idle or busy, no need
     * for an enum with only two states.
     */
    bool isBusy;

    /**
     * Remember if we have to retry an outstanding request that
     * arrived while we were busy.
     */
    bool retryReq;

    /**
     * Remember if we failed to send a response and are awaiting a
     * retry. This is only used as a check.
     */
    bool retryResp;

    /**
     * Release the memory after being busy and send a retry if a
     * request was rejected in the meanwhile.
     */
    void release();

    EventFunctionWrapper releaseEvent;

    /**
     * Dequeue a packet from our internal packet queue and move it to
     * the port where it will be sent as soon as possible.
     */
    void dequeue();

    EventFunctionWrapper dequeueEvent;

    /**
     * Detemine the latency.
     *
     * @return the latency seen by the current packet
     */
    Tick getLatency() const;

    /**
     * Upstream caches need this packet until true is returned, so
     * hold it for deletion until a subsequent call
     */
    std::unique_ptr<Packet> pendingDelete;

  public:

    DuettoSimpleMem(const DuettoSimpleMemParams &p);

    DrainState drain() override;

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;
    void init() override;

    void startup() override;

    // Packet struct that gets stored in the queue
    struct packet_queue_element
    {
      PacketPtr pkt;
      Tick arrival_tick;
      Tick pkt_tick;
      Tick fcfs_tick;
      bool is_prefetch;
      bool scheduled;
    };

    // Number of requestors
    int num_requestors;

    // Variable that determines the current scheduling scheme: 1 is FCFS and 0 is RR
    bool is_fcfs;

    // Memory request latency in ticks
    Tick latency_in_ticks;

    // Queue that precedes the demand token bucket for every requestor
    std::deque<packet_queue_element> * pre_bucket_demand_queues;
    std::deque<packet_queue_element> * pre_bucket_prefetch_queues;

    // RR and FCFS picks packets from this queue that comes after the demand token bucket
    std::deque<packet_queue_element> * post_bucket_demand_queues;
    std::deque<packet_queue_element> * post_bucket_prefetch_queues;

    // Round robin queue that keeps track of the next requestor
    std::queue<int> round_robin_queue;

    Tick remaining_ticks;
    
    // Function that refills the requestors latency counter
    void refill_counter(int requestor)
    {
      requestor_latency_counters[requestor] = std::min(latency_slack, requestor_latency_counters[requestor]+int64_t(request_delta)); 
    }
    
    // Function to promote prefetch based on the queue type
    void promote_prefetch(std::deque<packet_queue_element>::iterator it, bool is_pre_queue, int requestor)
    {
        if (is_pre_queue)
        {
            // Promoting from pre bucket demand queues
            std::cout << "Promoting prefetch from pre queue to demand for address: " << it->pkt->getAddr() << "\n";

            // Insert the element at the correct position in the post demand queue
            auto insert_position = post_bucket_demand_queues[requestor].begin();

            // Find the correct spot based on arrival_tick
            while (insert_position != post_bucket_demand_queues[requestor].end() &&
                  insert_position->arrival_tick <= it->arrival_tick)
            {
                ++insert_position;
            }

            // Insert the element at the found position
            post_bucket_demand_queues[requestor].insert(insert_position, *it);

            // Erase the element from the pre bucket demand queue
            pre_bucket_demand_queues[requestor].erase(it);
        }
        else
        {
            // Promoting from post bucket prefetch queues
            std::cout << "Promoting prefetch from post queue to demand for address: " << it->pkt->getAddr() << "\n";

            // Ensure that the address does not match the front of the post bucket prefetch queue
            if (it->pkt->getAddr() != post_bucket_prefetch_queues[requestor].front().pkt->getAddr() || (it->pkt->getAddr() == post_bucket_prefetch_queues[requestor].front().pkt->getAddr() && (post_bucket_prefetch_queues[requestor].front().scheduled ==false)))
            {
                // Insert the element at the correct position in the post demand queue
                auto insert_position = post_bucket_demand_queues[requestor].begin();
                
                // Find the correct spot based on arrival_tick
                while (insert_position != post_bucket_demand_queues[requestor].end() &&
                      insert_position->arrival_tick <= it->arrival_tick)
                {
                    ++insert_position;
                }

                // Insert the element at the found position
                post_bucket_demand_queues[requestor].insert(insert_position, *it);

                // Erase the element from the post bucket prefetch queue
                post_bucket_prefetch_queues[requestor].erase(it);
            }
        }

        // Additional logic to promote the prefetch if needed
    }

    void printQueueDetails(const std::deque<packet_queue_element>& queue)
    {
        if (queue.empty())
        {
            std::cout << "Queue is empty." << std::endl;
            return;
        }

        std::cout << "Queue details (size: " << queue.size() << "):\n";

        for (auto it = queue.begin(); it != queue.end(); ++it)
        {
            if (it == queue.begin())
            {
                std::cout << "Front -> ";
            }
            else if (std::next(it) == queue.end())
            {
                std::cout << "Back -> ";
            }

            std::cout << "Address: " << std::hex<< it->pkt->getAddr() << ", Arrival Tick: " << std::dec <<it->arrival_tick << std::endl;
        }
    }

    // Event that moves request from pre-bucket to post bucket and schedules requests
    EventFunctionWrapper dispatchMemoryRequest;
    void dispatch_memory_requests();

    // Slack latency in cycles
    int64_t latency_slack;

    // Delta is the relative deadline of every request which is >= RR worst case time
    int64_t request_delta;

    // Counters for each requestor, those counters will be used to switch from RTA to HPA
    int64_t * requestor_latency_counters;

  protected:
    Tick recvAtomic(PacketPtr pkt);
    Tick recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor);
    void recvFunctional(PacketPtr pkt);
    void recvMemBackdoorReq(const MemBackdoorReq &req,
            MemBackdoorPtr &backdoor);
    bool recvTimingReq(PacketPtr pkt);
    void recvRespRetry();
};

} // namespace memory
} // namespace gem5

#endif //__MEM_SIMPLE_MEMORY_HH__
