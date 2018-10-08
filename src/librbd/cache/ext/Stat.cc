#include "Stat.h"

  void Stat::reset_stat(){
    this->num_hit = 0;
    this->num_missed = 0;
    this->num_unused = 0;
  }
  // member function to increment hit count
  void Stat::inc_hit(){
    this->tick = this->tick + 1;
    if(this->tick >= this->warmup_tick)
        this->warmup_num_hit = this->warmup_num_hit + 1;
    this->num_hit = this->num_hit + 1;
  }
  void Stat::inc_miss(){
    this->tick = this->tick + 1;
    if(this->tick >= this->warmup_tick)
      this->warmup_num_missed = this->warmup_num_missed + 1;
    this->num_missed = this->num_missed + 1;
  }
  void Stat::add_unused(uint64_t num_used){
    if(this->tick >= this->warmup_tick)
        this->warmup_num_unused = this->warmup_num_unused + 1;
    this->num_unused = this->num_unused + num_used;
  }
