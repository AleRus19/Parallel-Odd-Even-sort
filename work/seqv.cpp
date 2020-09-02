#include <cassert>
#include <iostream>

#include "utils.cpp"
#include "utimer.cpp"

/*
* Quick compiling:
    g++ -g -O3 seqv.cpp -o seqv -fopt-info-vec
*
*/

//odd-even sort bussiness logic function
template <typename T> 
// *v = pointer
// end = offset
int oe_sort(T* v, int end){
    T tmp_first, tmp_second; // local variables for swapping
    int num_swap=0;  // counters of local swaps
    # pragma GCC ivdep  // avoid versioning loop and no aliasing
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


int main(int argc, char *argv[]) {

    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " size seed \n";
        return -1;
    }

    int size = atoi(argv[1]); // size of array
    int seed = atoi(argv[2]); // seed random initilization
    srand(seed);
    
    std::vector<type> v(size); // vector to sort
    v.reserve(size);

    std::generate(v.begin(), v.end(), f); // random initilization

    int num_swap = 0; //number of swaps executed 
    int start = 0;
    int end = v.size() - 1; 
    { 
        utimer u("Seq: ");
        while(true){
            num_swap = oe_sort<type>(v.data() + 1, end - 1); // odd phase 
            num_swap += oe_sort<type>(v.data(), end); // even phase
            if(num_swap == 0 ) break; // if n.swaps == 0 then exit from the while
        }
    }
    assert(std::is_sorted(v.begin(),v.end())); // check correctness
    return 0;

}
/*
// probing code
    int odd_time=0, even_time = 0; int it=0; 
    { 
        utimer u("Seq: ");
        while(true){
            auto odd_start = std::chrono::system_clock::now();
            num_swap = oe_sort<type>(v.data() + 1, end - 1);
            auto odd_end = std::chrono::system_clock::now();
            odd_time += float(std::chrono::duration_cast <std::chrono::microseconds> (odd_end - odd_start).count());
            auto even_start = std::chrono::system_clock::now();
            num_swap += oe_sort<type>(v.data(), end);
            auto even_end = std::chrono::system_clock::now();
            even_time += float(std::chrono::duration_cast <std::chrono::microseconds> (even_end - even_start).count());
            it++;
            if(num_swap == 0 ) break;
        }
    }
    std::cout << "Odd_avg: " << odd_time / it << " Even_avg: " << even_time/it << " it: " << it<< std::endl;   assert(std::is_sorted(v.begin(),v.end())); // check correctness
    return 0;

}
*/