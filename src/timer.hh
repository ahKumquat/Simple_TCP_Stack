#pragma once

class Timer {
    private:
        //retransmission timeout
        uint64_t curr_RTO_;

        uint64_t initial_RTO_ms_;

        uint64_t time_{0};

        bool is_running_{false};

    public:
        explicit Timer(uint64_t initial_RTO) : curr_RTO_(initial_RTO), initial_RTO_ms_(initial_RTO){}

        // tick() method will be called with an argument that tells 
        // it how many milliseconds have elapsed since the last time the method wascalled. 
        void tick( uint64_t const ms_since_last_tick ) {
            if (is_running_) time_ += ms_since_last_tick ; 
        }

        void start() {
            is_running_ = true;
            time_ = 0;
        }

        void stop() {is_running_ = false;}

        bool is_running() {return is_running_;}

        bool is_expired() {return is_running_ && time_ >= curr_RTO_;}

        void doule_RTO() {curr_RTO_*=2;}

        void reset_RTO() {curr_RTO_ = initial_RTO_ms_;}
};