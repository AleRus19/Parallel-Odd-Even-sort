#include <iostream>
#include <atomic>

class atomicBar {
private:
  std::atomic<int> n; 
  
public:
  atomicBar() {}
  atomicBar(int n) :n(n) {}

  // initialize n to in 
  void set_t(int in) {
    n = in;
    return;
  }
  
  // decrement n 
  void dec_after_check() {
    n--;
  }
 

  void wait_before_check() {
    while(n!=1);
  }

  void dec_wait(){
    n--;
    while(n!=0);
  }

};
