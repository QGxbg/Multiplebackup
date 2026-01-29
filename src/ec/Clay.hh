#ifndef _CLAY_HH_
#define _CLAY_HH_

//#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"
 #include "../util/reed_sol.h"
//#include"../util/reed_sol.c"

using namespace std;
#define CLAYPFT_N_MAX (32)
#define CLAYMDS_N_MAX (32)
#define CLAY_DEBUG false


class Erasure_t {
    public: 
        int _x;
        int _y;

        Erasure_t(int x, int y);
        void dump();
};

class Clay: public ECBase {
    private:
        int _m; 
        int _d;
        int _q;
        int _t;
        int _nu = 0;

        // for mds
        int _mds_k;
        int _mds_m;
        int _mds_encode_matrix[CLAYMDS_N_MAX * CLAYMDS_N_MAX];

        // for pft
        int _pft_k;
        int _pft_m;
        int _pft_encode_matrix[CLAYPFT_N_MAX * CLAYPFT_N_MAX];

        int _sub_chunk_no;

        // for shortening?
        unordered_map<int, int> _short2real;
        unordered_map<int, int> _real2short;

        // to remove x = 1 * y
        // if a Join requires to use x, it directly use y
        unordered_map<int, int> _identical; // given a parent, directly use its child  z_vec[y] == x
        bool _encode;

        void generate_matrix(int* matrix, int rows, int cols, int w);
        void generate_matrix_ceph(int* matrix, int rows, int cols, int w) ;
        int pow_int(int a, int x);
        void get_erasure_coordinates(vector<int> erased_chunk, Erasure_t** erasures);
        void get_weight_vector(Erasure_t** erasures, int* weight_vec);
        int get_hamming_weight(int* weight_vec);
        void set_planes_sequential_decoding_order(int* order, Erasure_t** erasures);
        void get_plane_vector(int z, int* z_vec);

        void decode_erasures(vector<int> erased_chunks, int z, ECDAG* ecdag);
        vector<int> get_uncoupled_from_coupled(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        vector<int> get_uncoupled2_from_coupled(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        vector<int> get_uncoupled3_from_coupled(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        vector<int> get_uncoupled3_from_coupled02(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        vector<int> get_uncoupled3_from_coupled12(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        vector<int> get_uncoupled2_from_coupled03(int x, int y, int z, int* z_vec, ECDAG* ecdag);

        vector<int> decode_uncoupled(vector<int> erased_chunks, int z, ECDAG* ecdag);
        void get_coupled_from_uncoupled(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        void get_coupled10_from_uncoupled(int x, int y, int z, int* z_vec, ECDAG* ecdag);

        void print_matrix(int* matrix, int row, int col);

        int is_repair(vector<int> want_to_read, vector<int> avail);
        void minimum_to_repair(vector<int> want_to_read, vector<int> available_chunks, unordered_map<int, vector<pair<int, int>>>& minimum);
        void get_repair_subchunks(int lost_node, vector<pair<int, int>> &repair_sub_chunks_ind);
        int get_repair_sub_chunk_count(vector<int> want_to_read);
        void repair_one_lost_chunk(unordered_map<int, bool>& recovered_data,
                vector<int>& aloof_nodes,
                unordered_map<int, bool>& helper_data,
                vector<pair<int,int>> &repair_sub_chunks_ind,
                ECDAG* ecdag);

        vector<int> get_coupled1_from_pair02(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        vector<int> get_coupled0_from_pair13(int x, int y, int z, int* z_vec, ECDAG* ecdag);
        vector<int> get_coupled0_from_pair12(int x, int y, int z, int* z_vec, ECDAG* ecdag);
          vector<int> get_coupled1_from_pair03(int x, int y, int z, int* z_vec, ECDAG* ecdag);

        int get_real_from_short(int idx);
        int get_short_from_real(int idx);
        void minimum_to_decode(vector<int> want_to_read,vector<int> available,unordered_map<int, vector<pair<int, int>>>& minimum);
        void rs_minimum_to_decode(vector<int> want_to_read,vector<int> available,unordered_map<int, vector<pair<int, int>>>& minimum);
        void rs1_minimum_to_decode(vector<int> want_to_read,vector<int> available,set<int> *minimum);
        void rs_decode(vector<int> &want_to_read, unordered_map<int, bool>& chunks,unordered_map<int, bool>&repaire,ECDAG* ecdag);
        void decode_chunks(vector<int> want_to_read,unordered_map<int, bool>& chunks,unordered_map<int, bool>& decoded,ECDAG* ecdag);
        void decode_layered(vector<int>& erased_chunks,ECDAG* ecdag);
         int is_erasure_type_1(int ind, Erasure_t** erasures, int* z_vec);
         vector<int>  recover_type1_erasure(int x, int y, int z, int* z_vec, ECDAG* ecdag) ;

    public:
        Clay(int n, int k, int w, vector<string> param);
//        Clay(int n, int k, int w, int opt, vector<string> param);
//
        ECDAG* Encode();
        ECDAG* Decode(vector<int> from, vector<int> to);
//        void Place(vector<vector<int>>& group);
//        void Shorten(unordered_map<int, int>& shortening);
};


#endif