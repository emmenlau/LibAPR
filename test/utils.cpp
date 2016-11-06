////////////////////////
//
//  Mateusz Susik 2016
//
//  Utility functions for the tests
//
////////////////////////

#include "utils.h"

#include <algorithm>
#include <functional>
#include <iostream>

#include "../src/io/readimage.h"
#include "../src/io/write_parts.h"
#include "../src/io/read_parts.h"

bool compare_two_images(const Mesh_data<uint16_t>& in_memory, std::string filename) {

    /* Returns true iff two images are the same with tolerance of 1 per pixel. */

    Mesh_data<uint16_t > input_image;

    load_image_tiff(input_image, filename);

    auto it2 = input_image.mesh.begin();
    for(auto it1 = in_memory.mesh.begin(); it1 != in_memory.mesh.end(); it1++, it2++)
    {
        // 60000 is the threshold introduced in variance computations. When a value reaches zero when mean is
        // computed, it is changed to 60000 afterwards. It is caused by the set of parameters in test case.
        if(std::abs(*it1 - *it2) > 1 && std::abs(*it1 - *it2) != 60000) {
            std::cout << std::distance(it1, in_memory.mesh.begin()) << " " << *it1 << " " << *it2 << std::endl;
            return false;
        }
    }
    return true;

}

bool compare_two_ks(const Particle_map<float>& in_memory, std::string filename) {

    for (int k = in_memory.k_min;k <= in_memory.k_max;k++) {

        Mesh_data<uint8_t > to_compare;

        // in_memory.layers[k]
        load_image_tiff(to_compare, filename + "_" + std::to_string(k) + ".tif");

        auto it2 = to_compare.mesh.begin();
        for(auto it1 = in_memory.layers[k].mesh.begin();
                 it1 != in_memory.layers[k].mesh.end(); it1++, it2++)
        {
            if(*it1 != *it2) {
                std::cout << std::distance(it1, in_memory.layers[k].mesh.begin()) <<
                             " " << (int)*it1 << " " << (int)*it2 << std::endl;
                return false;
            }
        }
    }

    return true;
}

bool compare_part_rep_with_particle_map(const Particle_map<float>& in_memory, std::string filename) {
    Part_rep p_rep;
    read_parts_from_full_hdf5(p_rep, filename);


    // Check

    for(int i = 0; i < p_rep.status.data.size(); i++) {
        //count Intensity as well

        if(true) {

            if (p_rep.status.data[i] != EMPTY) {


                int x = p_rep.pl_map.cells[i].x;
                int y = p_rep.pl_map.cells[i].y;
                int z = p_rep.pl_map.cells[i].z;
                int k = p_rep.pl_map.cells[i].k;

                int x_num = in_memory.layers[k].x_num;
                int y_num = in_memory.layers[k].y_num;

                if (x <= p_rep.org_dims[1] / 2 &&
                    y <= p_rep.org_dims[0] / 2 &&
                    z <= p_rep.org_dims[2] / 2) {
                    // add if it is in domain
                    if (p_rep.status.data[i] == 2) {

                        if(in_memory.layers[k].mesh[(z-1) * x_num * y_num + (x-1) * y_num + y - 1] != TAKENSTATUS)
                        {
                            std::cout << "Different status: INITIALIZED" << std::endl;
                            return false;
                        }




                    } else if (p_rep.status.data[i] >= 4) {
                        //check if has the same status

                        if(p_rep.status.data[i] == 4 &&
                            in_memory.layers[k].mesh[(z-1) * x_num * y_num + (x-1) * y_num + y - 1] != NEIGHBOURSTATUS)
                        {
                            std::cout << "Different status: NEIGHBOUR " << std::endl;
                            return false;
                        }

                        if(p_rep.status.data[i] == 5 &&
                           in_memory.layers[k].mesh[(z-1) * x_num * y_num + (x-1) * y_num + y - 1] != SLOPESTATUS)
                        {
                            std::cout << "Different status: SLOPE" << (int)z << " " <<
                                         (int)x << " " << (int)y << std::endl;
                            //return false;
                        }

                    }

                }
            }
        }
    }


    return true;
}

Mesh_data<uint16_t> create_random_test_example(unsigned int size_y, unsigned int size_x,
                                                unsigned int size_z, unsigned int seed) {
    // creates the input image of a given size with given seed
    // uses ranlux48 random number generator
    // the seed used in 2016 for generation was 5489u

    std::ranlux48 generator(seed);
    std::normal_distribution<float> distribution(1000, 250);

    Mesh_data<uint16_t> test_example(size_y, size_x, size_z);

    std::generate(test_example.mesh.begin(), test_example.mesh.end(),
                  // partial application of generator and distribution to get_random_number function
                  std::bind(get_random_number, generator, distribution));

    return test_example;

}

Mesh_data<uint16_t> generate_random_ktest_example(unsigned int size_y, unsigned int size_x,
                                                  unsigned int size_z, unsigned int seed,
                                                  float mean_fraction, float sd_fraction) {

    // creates the input image of a given size with given seed
    // the image should be used as a source of benchmarking for the get_k step
    // dx, dy and dz should all be set to 1, rel_error to 1000
    // the seed used in 2016 for generation was 5489u

    std::ranlux48 generator(seed);

    int max_dim = std::max(size_x, std::max(size_y, size_z));
    float k_max = ceil(M_LOG2E*log(max_dim)) - 1;

    std::normal_distribution<float> distribution(k_max * mean_fraction, k_max * sd_fraction);

    Mesh_data<uint16_t> test_example(size_y, size_x, size_z);

#pragma omp parallel for default(shared)
    for(int i = 0; i < test_example.mesh.size(); i++){
        test_example.mesh[i] = get_random_number_k(generator, distribution, k_max);
    }

    std::generate(test_example.mesh.begin(), test_example.mesh.end(),
                  // partial application of generator and distribution to get_random_number function
                  std::bind(get_random_number_k, generator, distribution, k_max));

    return test_example;

}

uint16_t get_random_number(std::ranlux48& generator, std::normal_distribution<float>& distribution){

    float val = distribution(generator);
    //there should be no values below zero.
    return val < 0 ? 1 : val;

}

uint16_t get_random_number_k(std::ranlux48& generator,
                             std::normal_distribution<float>& distribution, float k_max){

    float val = distribution(generator);
    //there should be no values below zero.
    return std::max(K_BENCHMARK_REL_ERROR * pow(2, val - k_max), 0.01);

}

std::string get_source_directory(){
    // returns path to the directory where utils.cpp is stored

    std::string tests_directory = std::string(__FILE__);
    tests_directory = tests_directory.substr(0, tests_directory.find_last_of("\\/") + 1);

    return tests_directory;
}


bool compare_sparse_rep_with_part_map(const Particle_map<float>& part_map,PartCellStructure<float,uint64_t>& pc_struct){
    //
    //  Compares the sparse representation with the particle map original data used to generate it
    //
    
    //initialize
    uint64_t node_val;
    uint64_t y_coord;
    int x_;
    int z_;
    uint64_t y_;
    uint64_t j_;
    uint64_t status;
    uint64_t status_org;
    
    
    uint64_t type;
    uint64_t yp_index;
    uint64_t yp_depth;
    uint64_t ym_index;
    uint64_t ym_depth;
    uint64_t next_coord;
    uint64_t prev_coord;
    
    uint64_t xp_index;
    uint64_t xp_depth;
    uint64_t zp_index;
    uint64_t zp_depth;
    uint64_t xm_index;
    uint64_t xm_depth;
    uint64_t zm_index;
    uint64_t zm_depth;
    
    std::cout << "Start Status Test" << std::endl;
    
    bool pass_test = true;
    
    //basic tests of status and coordinates
    
    for(int i = pc_struct.pc_data.depth_min;i <= pc_struct.pc_data.depth_max;i++){
        
        const unsigned int x_num = pc_struct.pc_data.x_num[i];
        const unsigned int z_num = pc_struct.pc_data.z_num[i];
        
        
        for(z_ = 0;z_ < z_num;z_++){
            
            for(x_ = 0;x_ < x_num;x_++){
                
                const size_t offset_pc_data = x_num*z_ + x_;
                y_coord = 0;
                const size_t j_num = pc_struct.pc_data.data[i][offset_pc_data].size();
                
                const size_t offset_part_map_data_0 = part_map.downsampled[i].y_num*part_map.downsampled[i].x_num*z_ + part_map.downsampled[i].y_num*x_;
                
                for(j_ = 0;j_ < j_num;j_++){
                    
                    
                    node_val = pc_struct.pc_data.data[i][offset_pc_data][j_];
                    
                    if (node_val&1){
                        //get the index gap node
                        type = (node_val & TYPE_MASK) >> TYPE_SHIFT;
                        yp_index = (node_val & YP_INDEX_MASK) >> YP_INDEX_SHIFT;
                        yp_depth = (node_val & YP_DEPTH_MASK) >> YP_DEPTH_SHIFT;
                        
                        ym_index = (node_val & YM_INDEX_MASK) >> YM_INDEX_SHIFT;
                        ym_depth = (node_val & YM_DEPTH_MASK) >> YM_DEPTH_SHIFT;
                        
                        next_coord = (node_val & NEXT_COORD_MASK) >> NEXT_COORD_SHIFT;
                        
                        prev_coord = (node_val & PREV_COORD_MASK) >> PREV_COORD_SHIFT;
                        
                        y_coord = (node_val & NEXT_COORD_MASK) >> NEXT_COORD_SHIFT;
                        y_coord--;
                        
                        
                    } else {
                        //normal node
                        y_coord++;
                        
                        type = (node_val & TYPE_MASK) >> TYPE_SHIFT;
                        xp_index = (node_val & XP_INDEX_MASK) >> XP_INDEX_SHIFT;
                        xp_depth = (node_val & XP_DEPTH_MASK) >> XP_DEPTH_SHIFT;
                        zp_index = (node_val & ZP_INDEX_MASK) >> ZP_INDEX_SHIFT;
                        zp_depth = (node_val & ZP_DEPTH_MASK) >> ZP_DEPTH_SHIFT;
                        xm_index = (node_val & XM_INDEX_MASK) >> XM_INDEX_SHIFT;
                        xm_depth = (node_val & XM_DEPTH_MASK) >> XM_DEPTH_SHIFT;
                        zm_index = (node_val & ZM_INDEX_MASK) >> ZM_INDEX_SHIFT;
                        zm_depth = (node_val & ZM_DEPTH_MASK) >> ZM_DEPTH_SHIFT;
                        
                        //get and check status
                        status = (node_val & STATUS_MASK) >> STATUS_SHIFT;
                        status_org = part_map.layers[i].mesh[offset_part_map_data_0 + y_coord];
                        
                        //set the status
                        switch(status_org){
                            case TAKENSTATUS:
                            {
                                if(status != SEED){
                                    std::cout << "STATUS SEED BUG" << std::endl;
                                    pass_test = false;
                                }
                                break;
                            }
                            case NEIGHBOURSTATUS:
                            {
                                if(status != BOUNDARY){
                                    std::cout << "STATUS BOUNDARY BUG" << std::endl;
                                    pass_test = false;
                                }
                                break;
                            }
                            case SLOPESTATUS:
                            {
                                if(status != FILLER){
                                    std::cout << "STATUS FILLER BUG" << std::endl;
                                    pass_test = false;
                                }
                                
                                break;
                            }
                                
                        }
                        
                        
                        
                    }
                }
                
            }
        }
        
    }
    
    std::cout << "Finished Status Test" << std::endl;
    
    return pass_test;
    
    
    
}


bool compare_sparse_rep_neighcell_with_part_map(const Particle_map<float>& part_map,PartCellStructure<float,uint64_t>& pc_struct){
    //
    //
    //  Checks the
    //
    //
    //
    
    //initialize
    uint64_t node_val;
    uint64_t y_coord;
    int x_;
    int z_;
    uint64_t y_;
    uint64_t j_;
    uint64_t status;
    uint64_t status_org;
    
    
    uint64_t type;
    uint64_t yp_index;
    uint64_t yp_depth;
    uint64_t ym_index;
    uint64_t ym_depth;
    uint64_t next_coord;
    uint64_t prev_coord;
    
    uint64_t xp_index;
    uint64_t xp_depth;
    uint64_t zp_index;
    uint64_t zp_depth;
    uint64_t xm_index;
    uint64_t xm_depth;
    uint64_t zm_index;
    uint64_t zm_depth;
    
    bool pass_test = true;
    
    
    //Neighbour Routine Checking
    
    for(int i = pc_struct.pc_data.depth_min;i <= pc_struct.pc_data.depth_max;i++){
        
        const unsigned int x_num = pc_struct.pc_data.x_num[i];
        const unsigned int z_num = pc_struct.pc_data.z_num[i];
        
        
        for(z_ = 0;z_ < z_num;z_++){
            
            for(x_ = 0;x_ < x_num;x_++){
                
                const size_t offset_pc_data = x_num*z_ + x_;
                y_coord = 0;
                const size_t j_num = pc_struct.pc_data.data[i][offset_pc_data].size();
                
                const size_t offset_part_map_data_0 = part_map.downsampled[i].y_num*part_map.downsampled[i].x_num*z_ + part_map.downsampled[i].y_num*x_;
                
                
                for(j_ = 0;j_ < j_num;j_++){
                    
                    
                    node_val = pc_struct.pc_data.data[i][offset_pc_data][j_];
                    
                    if (node_val&1){
                        //get the index gap node
                        type = (node_val & TYPE_MASK) >> TYPE_SHIFT;
                        yp_index = (node_val & YP_INDEX_MASK) >> YP_INDEX_SHIFT;
                        yp_depth = (node_val & YP_DEPTH_MASK) >> YP_DEPTH_SHIFT;
                        
                        ym_index = (node_val & YM_INDEX_MASK) >> YM_INDEX_SHIFT;
                        ym_depth = (node_val & YM_DEPTH_MASK) >> YM_DEPTH_SHIFT;
                        
                        next_coord = (node_val & NEXT_COORD_MASK) >> NEXT_COORD_SHIFT;
                        
                        prev_coord = (node_val & PREV_COORD_MASK) >> PREV_COORD_SHIFT;
                        
                        
                        y_coord = (node_val & NEXT_COORD_MASK) >> NEXT_COORD_SHIFT;
                        
                        
                        y_coord--;
                        
                        
                        
                    } else {
                        //normal node
                        y_coord++;
                        
                        type = (node_val & TYPE_MASK) >> TYPE_SHIFT;
                        xp_index = (node_val & XP_INDEX_MASK) >> XP_INDEX_SHIFT;
                        xp_depth = (node_val & XP_DEPTH_MASK) >> XP_DEPTH_SHIFT;
                        zp_index = (node_val & ZP_INDEX_MASK) >> ZP_INDEX_SHIFT;
                        zp_depth = (node_val & ZP_DEPTH_MASK) >> ZP_DEPTH_SHIFT;
                        xm_index = (node_val & XM_INDEX_MASK) >> XM_INDEX_SHIFT;
                        xm_depth = (node_val & XM_DEPTH_MASK) >> XM_DEPTH_SHIFT;
                        zm_index = (node_val & ZM_INDEX_MASK) >> ZM_INDEX_SHIFT;
                        zm_depth = (node_val & ZM_DEPTH_MASK) >> ZM_DEPTH_SHIFT;
                        
                        //get and check status
                        status = (node_val & STATUS_MASK) >> STATUS_SHIFT;
                        status_org = part_map.layers[i].mesh[offset_part_map_data_0 + y_coord];
                        
                        
                        
                        //Check the x and z nieghbours, do they exist?
                        for(int face = 0;face < 6;face++){
                            
                            uint64_t x_n = 0;
                            uint64_t z_n = 0;
                            uint64_t y_n = 0;
                            uint64_t depth = 0;
                            uint64_t j_n = 0;
                            uint64_t status_n = 1;
                            uint64_t node_n = 0;
                            
                            std::vector<uint64_t> neigh_keys;
                            PartCellNeigh<uint64_t> neigh_keys_;
                            
                            uint64_t curr_key = 0;
                            curr_key |= ((uint64_t)i) << PC_KEY_DEPTH_SHIFT;
                            curr_key |= z_ << PC_KEY_Z_SHIFT;
                            curr_key |= x_ << PC_KEY_X_SHIFT;
                            curr_key |= j_ << PC_KEY_J_SHIFT;
                            
                            pc_struct.pc_data.get_neighs_face(curr_key,node_val,face,neigh_keys_);
                            neigh_keys = neigh_keys_.neigh_face[face];
                            
                            if (neigh_keys.size() > 0){
                                depth = (neigh_keys[0] & PC_KEY_DEPTH_MASK) >> PC_KEY_DEPTH_SHIFT;
                                
                                if(i == depth){
                                    y_n = y_coord + pc_struct.pc_data.von_neumann_y_cells[face];
                                } else if (depth > i){
                                    //neighbours are on layer down (4)
                                    y_n = (y_coord + pc_struct.pc_data.von_neumann_y_cells[face])*2 + (pc_struct.pc_data.von_neumann_y_cells[face] < 0);
                                } else {
                                    //neighbour is parent
                                    y_n =  (y_coord + pc_struct.pc_data.von_neumann_y_cells[face])/2;
                                }
                                
                            } else {
                                //check that it is on a boundary and should have no neighbours
                                
                                
                            }
                            
                            int y_org = y_n;
                            
                            for(int n = 0;n < neigh_keys.size();n++){
                                
                                x_n = (neigh_keys[n] & PC_KEY_X_MASK) >> PC_KEY_X_SHIFT;
                                z_n = (neigh_keys[n] & PC_KEY_Z_MASK) >> PC_KEY_Z_SHIFT;
                                j_n = (neigh_keys[n] & PC_KEY_J_MASK) >> PC_KEY_J_SHIFT;
                                depth = (neigh_keys[n] & PC_KEY_DEPTH_MASK) >> PC_KEY_DEPTH_SHIFT;
                                
                                if ((n == 1) | (n == 3)){
                                    y_n = y_n + pc_struct.pc_data.von_neumann_y_cells[pc_struct.pc_data.neigh_child_dir[face][n-1]];
                                } else if (n ==2){
                                    y_n = y_org + pc_struct.pc_data.von_neumann_y_cells[pc_struct.pc_data.neigh_child_dir[face][n-1]];
                                }
                                int dir = pc_struct.pc_data.neigh_child_dir[face][n-1];
                                int shift = pc_struct.pc_data.von_neumann_y_cells[pc_struct.pc_data.neigh_child_dir[face][n-1]];
                                
                                if (neigh_keys[n] > 0){
                                    
                                    //calculate y so you can check back in the original structure
                                    const size_t offset_pc_data_loc = pc_struct.pc_data.x_num[depth]*z_n + x_n;
                                    node_n = pc_struct.pc_data.data[depth][offset_pc_data_loc][j_n];
                                    const size_t offset_part_map = part_map.downsampled[depth].y_num*part_map.downsampled[depth].x_num*z_n + part_map.downsampled[depth].y_num*x_n;
                                    status_n = part_map.layers[depth].mesh[offset_part_map + y_n];
                                    
                                    if((status_n> 0) & (status_n < 8)){
                                        //fine
                                    } else {
                                        std::cout << "NEIGHBOUR LEVEL BUG" << std::endl;
                                        pass_test = false;
                                    }
                                    
                                    if (node_n&1){
                                        //points to gap node
                                        std::cout << "INDEX BUG" << std::endl;
                                        pass_test = false;
                                    } else {
                                        //points to real node, correct
                                    }
                                }
                            }
                            
                        }
                        
                        
                    }
                }
                
            }
        }
        
    }
    
    std::cout << "Finished Neigh Cell test" << std::endl;
    
    
    
    
    return pass_test;
    
}

bool compare_sparse_rep_neighpart_with_part_map(const Particle_map<float>& part_map,PartCellStructure<float,uint64_t>& pc_struct){
    //
    //
    //  Tests the particle sampling and particle neighbours;
    //
    //
    //
    //
    
    
    
    
    
    //initialize
    uint64_t node_val;
    uint64_t y_coord;
    int x_;
    int z_;
    uint64_t y_;
    uint64_t j_;
    uint64_t status;
    uint64_t status_org;
    
    
    bool pass_test = true;
    
    //Neighbour Routine Checking
    
    for(int i = pc_struct.pc_data.depth_min;i <= pc_struct.pc_data.depth_max;i++){
        
        const unsigned int x_num = pc_struct.pc_data.x_num[i];
        const unsigned int z_num = pc_struct.pc_data.z_num[i];
        
        
        for(z_ = 0;z_ < z_num;z_++){
            
            for(x_ = 0;x_ < x_num;x_++){
                
                const size_t offset_pc_data = x_num*z_ + x_;
                y_coord = 0;
                const size_t j_num = pc_struct.pc_data.data[i][offset_pc_data].size();
                
                const size_t offset_part_map_data_0 = part_map.downsampled[i].y_num*part_map.downsampled[i].x_num*z_ + part_map.downsampled[i].y_num*x_;
                
                
                for(j_ = 0;j_ < j_num;j_++){
                    
                    
                    node_val = pc_struct.pc_data.data[i][offset_pc_data][j_];
                    
                    if (node_val&1){
                        //get the index gap node
                        
                        y_coord--;
                        
                        
                    } else {
                        //normal node
                        y_coord++;
                        
                        
                        //get and check status
                        status = (node_val & STATUS_MASK) >> STATUS_SHIFT;
                        status_org = part_map.layers[i].mesh[offset_part_map_data_0 + y_coord];
                        
                        
                        //get the index gap node
                        uint64_t node_val_part = pc_struct.part_data.access_data.data[i][offset_pc_data][j_];
                        uint64_t curr_key = 0;
                        
                        pc_struct.part_data.access_data.pc_key_set_j(curr_key,j_);
                        pc_struct.part_data.access_data.pc_key_set_z(curr_key,z_);
                        pc_struct.part_data.access_data.pc_key_set_x(curr_key,x_);
                        pc_struct.part_data.access_data.pc_key_set_depth(curr_key,i);
                        
                        PartCellNeigh<uint64_t> neigh_keys;
                        PartCellNeigh<uint64_t> neigh_cell_keys;
                        
                        
                        //neigh_keys.resize(0);
                        status = pc_struct.part_data.access_node_get_status(node_val_part);
                        uint64_t part_offset = pc_struct.part_data.access_node_get_part_offset(node_val_part);
                        
                        //loop over the particles
                        for(int p = 0;p < pc_struct.part_data.get_num_parts(status);p++){
                            pc_struct.part_data.access_data.pc_key_set_index(curr_key,part_offset+p);
                            pc_struct.part_data.get_part_neighs_all(p,node_val,curr_key,status,part_offset,neigh_cell_keys,neigh_keys,pc_struct.pc_data);
                            
                            //First check your own intensity;
                            float own_int = pc_struct.part_data.get_part(curr_key);
                            
                            if(status > SEED){
                                
                                uint64_t curr_depth = pc_struct.pc_data.pc_key_get_depth(curr_key) + 1;
                                
                                uint64_t part_num = pc_struct.pc_data.pc_key_get_partnum(curr_key);
                                
                                uint64_t curr_x = pc_struct.pc_data.pc_key_get_x(curr_key)*2 + pc_struct.pc_data.seed_part_x[part_num];
                                uint64_t curr_z = pc_struct.pc_data.pc_key_get_z(curr_key)*2 + pc_struct.pc_data.seed_part_z[part_num];
                                uint64_t curr_y = y_coord*2 + pc_struct.pc_data.seed_part_y[part_num];
                                
                                const size_t offset_part_map = part_map.downsampled[curr_depth].y_num*part_map.downsampled[curr_depth].x_num*curr_z + part_map.downsampled[curr_depth].y_num*curr_x;
                                
                                if(own_int == (offset_part_map + curr_y)){
                                    //correct value
                                } else {
                                    std:: cout << own_int << " " << (offset_part_map + curr_y) << " " << part_map.downsampled[curr_depth].mesh[offset_part_map + curr_y] << std::endl;
                                    std::cout << "Particle Intensity Error" << std::endl;
                                    pass_test = false;
                                }
                                
                                
                            } else {
                                
                                
                                //const size_t offset_part_map = part_map.downsampled[depth].y_num*part_map.downsampled[depth].x_num*z_n + part_map.downsampled[depth].y_num*x_n;
                            }
                            
                            
                            
                            //Check the x and z nieghbours, do they exist?
                            for(int face = 0;face < 6;face++){
                                
                                uint64_t x_n = 0;
                                uint64_t z_n = 0;
                                uint64_t y_n = 0;
                                uint64_t depth = 0;
                                uint64_t j_n = 0;
                                uint64_t status_n = 0;
                                uint64_t intensity = 1;
                                uint64_t node_n = 0;
                                
                                uint64_t curr_key = 0;
                                curr_key |= ((uint64_t)i) << PC_KEY_DEPTH_SHIFT;
                                curr_key |= z_ << PC_KEY_Z_SHIFT;
                                curr_key |= x_ << PC_KEY_X_SHIFT;
                                curr_key |= j_ << PC_KEY_J_SHIFT;
                                
                                
                                for(int n = 0;n < neigh_keys.neigh_face[face].size();n++){
                                    
                                    
                                    pc_struct.pc_data.get_neigh_coordinates_part(neigh_keys,face,n,y_coord,y_n,x_n,z_n,depth);
                                    j_n = (neigh_keys.neigh_face[face][n] & PC_KEY_J_MASK) >> PC_KEY_J_SHIFT;
                                    
                                    if (neigh_keys.neigh_face[face][n] > 0){
                                        
                                        //calculate y so you can check back in the original structure
                                        const size_t offset_pc_data_loc = pc_struct.pc_data.x_num[depth]*z_n + x_n;
                                        
                                        const size_t offset_part_map = part_map.downsampled[depth].y_num*part_map.downsampled[depth].x_num*z_n + part_map.downsampled[depth].y_num*x_n;
                                        //status_n = part_map.layers[depth].mesh[offset_part_map + y_n];
                                        
                                        
                                        
                                    }
                                }
                                
                            }
                            
                            
                            
                            
                        }
                        
                        
                        
                        
                    }
                }
                
            }
        }
        
    }
    
    std::cout << "Finished Neigh Part test" << std::endl;
    
    
    
    return pass_test;
    
    
    
}
