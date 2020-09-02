#include <assert.h>
#include <math.h>

#include "atomicBar.cpp"
#include "utils.cpp"
#include "utimer.cpp"

/*
* Quick compiling: 
*   g++ -g -O3 par.cpp -o par -fopt-info-vec -pthread
*
*/

int nw; //number of workers
bool exit_cond = false; // exit condition: owner writes mode

//odd-even sort bussiness logic function
template <typename T>  
// *v = pointer
// end = offset
int oe_sort(T *v, int end){
    T tmp_first, tmp_second; // local variables for swapping
    int num_swap = 0; // counters of local swaps
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



// workers logic

void thread_logic (std::vector<type> &v, int id, int start, int end, int mode_odd, int mode_even, 
                   std::vector<atomicBar> &barriers, std::vector<int> &updates, int padding) {
    
    int iter_bar = 0; // index for vector of barriers
    
    while(true){

        if( exit_cond ){ // exit condition 
            return;
        }
        // odd phase 
        updates[id * padding] = oe_sort<type>( v.data() + start + mode_odd , end - start - mode_odd );
        // 1st synchronization point
        barriers[iter_bar].dec_wait(); //decrement and wait
        iter_bar++; 
        // even phase
        updates[id * padding] += oe_sort<type>(v.data() + start + mode_even , end - start - mode_even );
       
        // 2nd synchronization point

        // last thread and its n.swaps = 0 then can check the termination
        if(id == nw -1 && (updates[id * padding] == 0)) { 
            barriers[iter_bar].wait_before_check(); // wait without decrementing 
            int i=0;
            // checking the vector of swaps
            for(; i < nw - 1; i++){
                if(updates[i*padding] > 0) break; // exit if at least one threads has n.swap > 0
            }
            if(i == nw - 1) exit_cond = true; // if n.swap = 0 for all threads, set to true
            barriers[iter_bar].dec_after_check(); // decrement the barrier freeing the others
        }
        else  barriers[iter_bar].dec_wait(); // normally decrement and wait 
        iter_bar++;
    }
    return ;
};



int main(int argc, char *argv[]) {

    if (argc != 5) {
        std::cout << "Usage: " << argv[0] << " size seed nw cacheline \n";
        return -1;
    }

    int size = atoi(argv[1]); // size of array
    int seed = atoi(argv[2]); // seed random initilization
    nw = atoi(argv[3]); // number of workers
    int cacheLine = atoi(argv[4]); // size cacheline of the target machine
    srand(seed);
    
    std::vector<type> v;
    v.resize(size); // vector to sort
    
    std::generate(v.begin(), v.end(), f); // random initilization
    
    int start=0; int end=0;
    exit_cond = false; 
    
    {
        utimer u("Parallel: ");
        
        int pad = cacheLine / sizeof(int); // compute the padding

        std::vector<int> updates; 
        // nw * padding to avoid false sharing by putting only one used location
        // on a single cacheline
        updates.resize(nw * pad);  

        // vector of barriers
        std::vector<atomicBar> barriers(2*size);
        // set up vector of barriers with number of workers
        for(int i=0; i<barriers.size();i++){
            barriers[i].set_t(nw);
        }
        
        // set up the paralllel activities and 
        // distribution of the data
        std::vector<std::thread> tids(nw);
        
        int mode_even=0, mode_odd=0;   
        int  partition_size = size / nw; 
        int remaining = size % nw; //remaining elements to distribute eventually
        
        // forking a pool of nw threads 
        for(int t=0; t < nw; t++) {
            
            start = end;
            end  = start + partition_size + (remaining>0 ? 1:0) - (t==nw-1 ? 1:0);
            --remaining;
           
            mode_odd = 1 - (start%2);
            mode_even = (start%2);
            tids[t] = std::thread(thread_logic, std::ref(v), t, start, end, mode_odd, mode_even, 
                                  std::ref(barriers), std::ref(updates), pad);
                
        }
        // waiting the termination threads
        for(int t=0; t<nw; t++) {
            tids[t].join();
        }

    }
    // checking correctness
    assert(std::is_sorted(v.begin(),v.end()));
    return 0;
}
