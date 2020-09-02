#include <vector>
#include <algorithm>

// element type of vector
typedef int type;

int f(){ 
   return (rand() % 10000);
} 

// print vectors
void show(std::vector<type> v){
    for(int i=0; i< v.size(); i++){
        std::cout << v[i] << " " ;
    }
    std::cout<<std::endl;
}
 