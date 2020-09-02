/*
 * Parallel schema:
 *
 *                | ---> Worker -|
 *                |              |
 *    Emitter --> | ---> Worker -|--|
 *       ^        |              |  |
 *       |        | ---> Worker -|  |
 *       |__________________________|
 *       
 * This version starts nw + 1 threads: nw worker and 1 emitter. 
 * The Emitter sends to each Worker a value defining the odd or even phase
 * to be executed.
 * An iteration is composed by 2 phases: Odd and even phase.
 * Each Worker computes the local phase of its sub-range, then it sends 
 * the value of the number of swaps executed to the Emitter which coordinate and manage
 * the new iteration or eventually a termination. 
 *
 * 
 * 
 */

#include <iostream>
#include <iomanip>
#include <ff/ff.hpp>
#include <ff/farm.hpp>

#include "utils.cpp"

using namespace ff;
int nw; // number of workers

// odd-even sort bussiness logic function
template <typename T> 
// *v = pointer
// end = offset
int oe_sort(T *v, int end){
    T tmp_first, tmp_second; // local variables for swapping
    int num_swap = 0;  // counters of local swaps
    # pragma GCC ivdep // avoid versioning loop and no aliasing
    for(int i = 0; i < end; i+=2 ){
        tmp_first = v[i];
        tmp_second = v[i+1];
        // avoid explicit branches
        v[i] = ( (tmp_first > tmp_second) ? tmp_second : tmp_first ); 
        v[i+1] =  ( (tmp_first > tmp_second) ? tmp_first : tmp_second );  
        num_swap = ( (v[i] != tmp_first) ? num_swap + 1 : num_swap);                
    }
    return num_swap; // return number of swaps executed
}



struct firstThirdStage: ff_node_t<int, int> {

    // phase = 1 ==> Odd phase
    // phase = 0 ==> Even phase

    firstThirdStage(int size, int nw)
        : size(size), nw(nw), phase(1), ntasks(0), num_swap(0) {}

    int* svc(int* task) {
        // starting the computation
        if (task == nullptr) {
            for(int i=0; i < nw; i++){
                // tell to the workers to start the Odd phase
                ff_send_out(new int(phase));
            }
            return GO_ON;
        }

        // receive tasks 
        int &t = *task;
        num_swap += t; // accumulate swaps notified by the threads
        delete task;
        // wait that all workers completed the phase
        if(++ntasks < nw) return GO_ON;

        // terminate if both phases has been completed and 
        // the total swap of odd and even phase = 0
        if ((phase%2==0) && num_swap == 0 ) return EOS; // propagate the end of stream
        ntasks = 0;  
        if((phase%2) == 0) num_swap = 0; // reset if the two phases of the iteration are completed
        phase = (phase + 1) % 2; // set the next phase 
        for(int i=0; i < nw; i++){
            ff_send_out(new int(phase)); // tell to the workers to start the new phase
        }
        return GO_ON;   

    }
    
    void svc_end() {      
        
    }

    int nw; // number of workers
    int size; // size of the vector
    int phase; // phase = odd or even
    int ntasks; // number of reply of workers
    int num_swap; // number of swaps
};

struct secondStage: ff_node_t<int, int> {
     
    secondStage(std::vector<type> &v, int s, int st, int o, int e):start(s), end(st), mode_odd(o), mode_even(e){
        ptr = v.data();
    }

    int* svc(int * task) {
        int updates; // actual swap executed
        int &t = *task; // receive the phase
    
        if(t == 1){
            //odd phase
            updates = oe_sort<type>( ptr + start + mode_odd , end - start - mode_odd );
        }
        else {
            // even phase
            updates = oe_sort<type>(ptr + start + mode_even , end - start - mode_even );
        }
       
        return new int(updates); // return number of swaps
    }

    // portion of vector
    int start; // pointer + start
    int end; // pointer + end - start
    int mode_odd; // offset odd phase
    int mode_even; // offset even phase
    int *ptr; // pointer of vectors
   
};


int main(int argc, char * argv[]) {
    
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " size seed nw\n";
        return -1;
    }

    

    int size = atoi(argv[1]); // size of array
    int seed = atoi(argv[2]); // seed random initilization
    nw = atoi(argv[3]); // number of workers
    srand(seed);
     
    std::vector<type> v;
    v.resize(size); // vector to sort
    std::generate(v.begin(), v.end(), f); // random initilization
    //show(v);
    ffTime(START_TIME);
    
    // emitter   
    firstThirdStage  firstThirdStage(size, nw);
    
    // workers and distribution of ranges
    std::vector<std::unique_ptr<ff_node> > W;
    int start=0, end=0;
    int mode_even=0, mode_odd=0;   
    int  partition_size = size / nw;
    int remaining = size % nw;
    
     
    for(size_t t=0;t<nw;t++){
            start = end;
            end  = start + partition_size + (remaining>0 ? 1:0) - (t==nw-1 ? 1:0);
            --remaining;
            
            mode_odd = 1 - (start%2);
            mode_even = (start%2);
             
            W.push_back(make_unique<secondStage>(std::ref(v), start, end, mode_odd, mode_even));
        
    }
    
    ff_Farm<> farm(std::move(W), firstThirdStage);
    
    farm.remove_collector(); // needed because the collector is present by default in the ff_Farm
    farm.wrap_around();   // this call creates feedbacks from Workers to the Emitter
    //farm.set_scheduling_ondemand(); // optional

    if (farm.run_and_wait_end()<0) {
        error("running farm");
        return -1;
    }
  
    ffTime(STOP_TIME);
              std::cout<< ffTime(GET_TIME) << " ms"  << std::endl;
    //show(v);
    //farm.ffStats(std::cerr); // print stats
    assert(std::is_sorted(v.begin(),v.end())); // checking correctness
    return(0);
}