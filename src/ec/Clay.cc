#include "Clay.hh"

#define CLAY_DEBUG_ENABLE 0
#define CLAY_DEBUG 0
//#include"reed_sol.hh"
#define talloc(type, num) (type *) malloc(sizeof(type)*(num))

Erasure_t::Erasure_t(int x, int y) {
    _x = x;
    _y = y;
}

void Erasure_t::dump() {
    cout << "(" << _x << ", " << _y << ")" << endl;
}

//Clay::Clay(int n, int k, int w, int opt, vector<string> param) {
Clay::Clay(int n, int k, int w, vector<string> param) {
    _n = n;
    _k = k;
    _w = w;
//    _opt = opt;

    // param[0]: d
    _d = atoi(param[0].c_str());

    _m = _n - _k;
    _q = _d - _k + 1;

    if ((_k+_m) % _q) {
        _nu = _q - (_k+_m)%_q;
    } else {
        _nu = 0;
    }

    if (_nu != 0) { 
        //cout << "The configuration is not applicable in OpenEC!!!!!!!!!!!!!!!!!!!!!" << endl;
        if (CLAY_DEBUG)
            cout << "Add shorten idx!" << endl;
    }

    // mds
    _mds_k = _k+_nu;
    _mds_m = _m;

    // pft
    _pft_k = 2;
    _pft_m = 2;

    _t = (_k+_m+_nu)/_q;
    _sub_chunk_no = pow_int(_q, _t);

    if (w != _sub_chunk_no)
        _w = _sub_chunk_no;

    // prepare shortenidx to realidx
    for (int i=0; i<_k; i++) {
        for (int j=0; j<_w; j++) {
            int shortSubPktIdx = i * _w + j;
            _short2real[shortSubPktIdx] = shortSubPktIdx;
            _real2short[shortSubPktIdx] = shortSubPktIdx;

            int virSubPktIdx = shortSubPktIdx + (_n+_nu) * _w;
            _short2real[virSubPktIdx] = virSubPktIdx;
            _real2short[virSubPktIdx] = virSubPktIdx;
        }
    }


    for (int i=0; i<_nu; i++) {
        int shortPktIdx = _k+i;
        int realPktIdx = _n+i;
        for (int j=0; j<_w; j++) {
            int shortSubPktIdx = shortPktIdx * _w + j;
            int realSubPktIdx = realPktIdx * _w + j;
            _short2real[shortSubPktIdx] = realSubPktIdx;
            _real2short[realSubPktIdx] = shortSubPktIdx;

            int virShortSubPktIdx = shortSubPktIdx + (_n+_nu) * _w;
            int virRealSubPktIdx = realSubPktIdx + (_n+_nu) * _w;
            _short2real[virShortSubPktIdx] = virRealSubPktIdx;
            _real2short[virRealSubPktIdx] = virShortSubPktIdx;
        }
    }
    for (int i=0; i<(_n-_k); i++) {
        int shortPktIdx = _k+_nu+i;
        int realPktIdx = _k+i;
        for (int j=0; j<_w; j++) {
            int shortSubPktIdx = shortPktIdx * _w + j;
            int realSubPktIdx = realPktIdx * _w + j; 

            _short2real[shortSubPktIdx] = realSubPktIdx;
            _real2short[realSubPktIdx] = shortSubPktIdx;

            int virShortSubPktIdx = shortSubPktIdx + (_n+_nu) * _w;
            int virRealSubPktIdx = realSubPktIdx + (_n+_nu) * _w;
            _short2real[virShortSubPktIdx] = virRealSubPktIdx;
            _real2short[virRealSubPktIdx] = virShortSubPktIdx;
        }
    }

    // get pair-wise information
    for (int y=0; y<_t; y++) {
        for (int x=0; x<_q; x++) {
            int node_xy = y*_q + x;
            for (int z=0; z<_w; z++) {
                int z_vec[_t];
                get_plane_vector(z, z_vec);
                //cout << "node_xy = " << node_xy << ", z = " << z << ", z_vec = ( ";
                //for (int i=0; i<_t; i++)
                //    cout << z_vec[i] << " ";
                //cout << ")" << endl;

                int idx = node_xy * _w + z;   //unpaired id
                int pidx = idx + (_n+_nu) * _w;

                if (z_vec[y] == x) {
                    // cout << "  " << idx << " and " << pidx << " unpaired" << endl;
                    _identical.insert(make_pair(pidx, idx));
                }
            }
        }
    }

    if (CLAY_DEBUG) {
        cout << "Clay::Clay() k:" << _k << ", m:" << _m << ", d:" << _d << ", q:" << _q << ", t: " << _t << ", subchunkno:"<<_sub_chunk_no << endl;
        cout << "Clay::Clay() mds_k:" <<_mds_k << ", mds_m:" << _mds_m << endl;
        cout << "Clay::Clay() pft_k:" <<_pft_k << ", pft_m:" << _pft_m << endl;
    }
}

int Clay::pow_int(int a, int x) {
    int power = 1;
    while (x) {
        if (x & 1) power *= a;
        x /= 2;
        a *= a;
    }
    return power;
}

void Clay::get_erasure_coordinates(vector<int> erased_chunk,
        Erasure_t** erasures) {
    int eclen=erased_chunk.size();
    for (int idx=0; idx<eclen; idx++) {
        int i=erased_chunk[idx];
        int x = i%_q;
        int y = i/_q;
        erasures[idx] = new Erasure_t(x, y);
    }
}

void Clay::get_weight_vector(Erasure_t** erasures,
        int* weight_vec) {
    int i;
    memset(weight_vec, 0, sizeof(int)*_t);
    for (i=0; i<_m; i++) {
        weight_vec[erasures[i]->_y]++;
    }
}

int Clay::get_hamming_weight(int* weight_vec) {
    int i;
    int weight=0;
    for (i=0; i<_t; i++) {
        if (weight_vec[i] != 0) weight++;
    }
    return weight;
}

void Clay::set_planes_sequential_decoding_order(int* order, Erasure_t** erasures) {
    int z, i;
    int z_vec[_t];
    for (z=0; z<_sub_chunk_no; z++) {
        get_plane_vector(z, z_vec);
        order[z] = 0;
        for (i=0; i<_m; i++) {
            if (erasures[i]->_x == z_vec[erasures[i]->_y]) {
                order[z] = order[z]+1;
            }
        }
    }
}

void Clay::get_plane_vector(int z, int* z_vec) {
    int i;
    for (i=0; i<_t; i++) {
        z_vec[_t-1-i] = z % _q;
        z = (z - z_vec[_t-1-i])/_q;
    }
}

ECDAG* Clay::Encode() {
    _encode = true;
    ECDAG* ecdag = new ECDAG();
    vector<int> erased_chunks;
    
    for (int idx=_k+_nu; idx<_n+_nu; idx++) {
        erased_chunks.push_back(idx);
    }

    int i;
    int x, y;
    int hm_w;
    int z, node_xy, node_sw;

    int num_erasures = erased_chunks.size();
    assert(num_erasures > 0);
    assert(num_erasures == _m);

    Erasure_t** erasures = (Erasure_t**)calloc(num_erasures, sizeof(Erasure_t*));
    int weight_vec[_t];

    // get the (x, y) coordinate of a node_xy
    get_erasure_coordinates(erased_chunks, erasures);
    if (CLAY_DEBUG) {
        cout << "Clay::decode_layered() erasures:" << endl;
        for (int i=0; i<num_erasures; i++) {
            cout << " ";
            erasures[i]->dump();
        }
    }

    // get the number of failures in each y
    get_weight_vector(erasures, weight_vec);

    if (CLAY_DEBUG) {
        cout << "Clay::decode_layered() weight_vector: ";
        for (int i=0; i<_t; i++) cout << weight_vec[i] << " ";
        cout << endl;
    }

    // get the number of non-zero bit in the weight vector
    int max_weight = get_hamming_weight(weight_vec);
    if (CLAY_DEBUG)
        cout << "Clay::decode_layered() max_weight: " << max_weight << endl;

    // the number of unpaired symbols in each plane
    int order[_sub_chunk_no];
    int z_vec[_t];
    
    // in each plane, get the number of symbols that are unpaired from the node that we should
    // encode
    set_planes_sequential_decoding_order(order, erasures);

    if (CLAY_DEBUG) {
        cout << "Clay::decode_layer() order: ";
        for (int j=0; j<_sub_chunk_no; j++) cout << order[j] << " ";
        cout << endl;
    }

    for (hm_w = 0; hm_w <= max_weight; hm_w++) {
        for (z = 0; z < _sub_chunk_no; z++) {
            if (order[z]==hm_w) {
                // for available nodes, we get uncoupled data from coupled data
                // only used in encode_chunks
                decode_erasures(erased_chunks, z, ecdag);
            }
        }

        // from uncoupled to coupled data
        for (z=0; z<_sub_chunk_no; z++) {
            if (order[z] == hm_w) {
                get_plane_vector(z,z_vec);
                for (i = 0; i<num_erasures; i++) {
                    x = erasures[i]->_x;
                    y = erasures[i]->_y;
                    node_xy = y*_q+x;
                    node_sw = y*_q+z_vec[y];

                    if (z_vec[y] == x) {
                        int uncoupled_idx = node_xy * _w + z + (_n+_nu) * _w;
                        int coupled_idx = node_xy * _w + z;
                        //ecdag->Join(coupled_idx, {uncoupled_idx}, {1});
                        
                        int real_coupled_idx = get_real_from_short(coupled_idx);
                        int real_uncoupled_idx = get_real_from_short(uncoupled_idx);
                        if(CLAY_DEBUG)
                            cout << "277::old: Join(" << coupled_idx << "; [" << uncoupled_idx << "])" << endl;
//                      // XL comment starts
                        //ecdag->Join(real_coupled_idx, {real_uncoupled_idx}, {1});
                        // XL comment ends
                    
                    } else if (z_vec[y] < x) {
                        get_coupled_from_uncoupled(x, y, z, z_vec, ecdag);
                    }
                }
            }
        }
    }

    return ecdag;
}

void Clay::decode_erasures(vector<int> erased_chunks, int z, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout << "Clay::decode_erasures begin" << endl;
    int x, y;
    int node_xy, node_sw;
    int z_vec[_t];

    get_plane_vector(z,z_vec);

    if(CLAY_DEBUG){
        cout << "Clay::decode_erasure() z:( ";
        for (int i=0; i<_t; i++) cout << z_vec[i] << " ";
        cout << ")" << endl;
    }
    

    for (x=0; x < _q; x++) {
        for (y=0; y < _t; y++) {
            node_xy = _q*y+x;
            node_sw = _q*y+z_vec[y];

            if (find(erased_chunks.begin(), erased_chunks.end(), node_xy) == erased_chunks.end()) {
                if (z_vec[y] < x) {
                    // this symbol should be paired
                    // cout<<"309 hang: z_vec[y] < x:"<<endl;
                    if (find(erased_chunks.begin(), erased_chunks.end(), node_sw) == erased_chunks.end()) {
                        get_uncoupled_from_coupled(x, y, z, z_vec, ecdag);
                    }
                    else{
                        get_uncoupled2_from_coupled03(x, y, z, z_vec, ecdag);
                        //get_coupled_from_uncoupled(x, y, z, z_vec, ecdag);
                    }
                } else if (z_vec[y] == x) {
                    // we color this symbol, directly copy content from coupled buffers
                    int coupled_idx = node_xy * _w + z;
                    int uncoupled_idx = coupled_idx + (_n+_nu) * _w;
                    //ecdag->Join(uncoupled_idx, {coupled_idx}, {1});

                    int real_coupled_idx = get_real_from_short(coupled_idx);
                    int real_uncoupled_idx = get_real_from_short(uncoupled_idx);
                    if(CLAY_DEBUG)
                        cout << "318::(" << uncoupled_idx << "; [" << coupled_idx << "])" << endl;
//                  // XL comment start
                    //ecdag->Join(real_uncoupled_idx, {real_coupled_idx}, {1});
                    // XL comment end
                }else{
                    if(find(erased_chunks.begin(), erased_chunks.end(), node_sw) != erased_chunks.end()){
                        // cout<<"332 hang:"<<endl;
                        //continue;
                        get_uncoupled3_from_coupled02(x, y, z, z_vec, ecdag);
                    }
                }
            }
        }
    }
    
    decode_uncoupled(erased_chunks, z, ecdag);
    if(CLAY_DEBUG)
        cout << "Clay::decode_erasures end" << endl;
}

vector<int> Clay::get_uncoupled_from_coupled(int x, int y, int z, int* z_vec, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout<<"Clay::get_uncoupled_from_coupled begin"<<endl;
    vector<int> toret;

    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x

    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);
    int node_xy = y*_q+x;
    int z_xy = z;
    assert(z_vec[y] < x);
    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);
    vector<int> data;
    vector<int> code;
    
    int idx0 = node_xy * _w + z_xy;
    int idx1 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx0);
    data.push_back(idx1);

    // data.push_back(idx0);
    // data.push_back(idx2);
    // code.push_back(idx1);
    // code.push_back(idx3);
//     361 data: 5, 3
// 362 code: 21, 19

    if(CLAY_DEBUG){
        cout << "361 data: " << idx0 << ", " << idx1 << endl;
        cout << "362 code: " << idx2 << ", " << idx3 << endl;
    }
    // calculate idx2
    vector<int> coef2;
    for (int i=0; i<2; i++) {
        int curcoef = _pft_encode_matrix[_pft_k*_pft_k+i];
        coef2.push_back(curcoef);
    }
    //ecdag->Join(idx2, data, coef2);

    int real_idx2 = get_real_from_short(idx2);
    if (CLAY_DEBUG)
        cout<<"real_idx2:"<<real_idx2<<endl;

    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        // cout<<tmprealidx<<endl;
        realdata.push_back(tmprealidx);
    }
    if(CLAY_DEBUG)
        cout << "379::Join(" << real_idx2 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    ecdag->Join(real_idx2, realdata, coef2);
    //ecdag->dump();
    toret.push_back(real_idx2);

    // calculate idx3
    vector<int> coef3;
    for (int i=0; i<2; i++) {
        int curcoef = _pft_encode_matrix[(_pft_k+1)*_pft_k+i];
        coef3.push_back(curcoef);
    }
    //ecdag->Join(idx3, data, coef3);

    int real_idx3 = get_real_from_short(idx3);
    if(CLAY_DEBUG)
        cout << "393::old: Join(" << real_idx3 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    
    ecdag->Join(real_idx3, realdata, coef3);   //DEBUG
    //ecdag->dump();
    toret.push_back(real_idx3);
    //ecdag->BindX({idx2, idx3});
    if(CLAY_DEBUG)
        cout<<"435 hang Clay::get_uncoupled_from_coupled end"<<endl;
    return toret;
}

vector<int> Clay::get_uncoupled2_from_coupled(int x, int y, int z, int* z_vec, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout<<"Clay::get_uncoupled2_from_coupled begin"<<endl;
    vector<int> toret;

    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x

    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);
    int node_xy = y*_q+x;
    int z_xy = z;
    assert(z_vec[y] < x);
    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);
    vector<int> data;
    vector<int> code;
    
    int idx0 = node_xy * _w + z_xy;
    int idx1 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx0);
    data.push_back(idx1);

    // data.push_back(idx0);
    // data.push_back(idx2);
    // code.push_back(idx1);
    // code.push_back(idx3);
//     361 data: 5, 3
// 362 code: 21, 19

    if(CLAY_DEBUG){
        cout << "361 data: " << idx0 << ", " << idx1 << endl;
        cout << "362 code: " << idx2 << ", " << idx3 << endl;
    }
    // calculate idx2
    vector<int> coef2;
    for (int i=0; i<2; i++) {
        int curcoef = _pft_encode_matrix[_pft_k*_pft_k+i];
        coef2.push_back(curcoef);
    }
    //ecdag->Join(idx2, data, coef2);

    int real_idx2 = get_real_from_short(idx2);
    if (CLAY_DEBUG)
        cout<<"real_idx2:"<<real_idx2<<endl;

    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        // cout<<tmprealidx<<endl;
        realdata.push_back(tmprealidx);
    }
    if(CLAY_DEBUG)
        cout << "379::Join(" << real_idx2 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    ecdag->Join(real_idx2, realdata, coef2);
    //ecdag->dump();
    toret.push_back(real_idx2);


    if(CLAY_DEBUG)
        cout<<"503 hang Clay::get_uncoupled_from_coupled end"<<endl;
    return toret;
}

vector<int> Clay::get_uncoupled3_from_coupled(int x, int y, int z, int* z_vec, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout<<"Clay::get_uncoupled3_from_coupled begin"<<endl;
    vector<int> toret;

    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x

    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);
    int node_xy = y*_q+x;
    int z_xy = z;
    assert(z_vec[y] > x);
    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);
    vector<int> data;
    vector<int> code;
    
    int idx1 = node_xy * _w + z_xy;
    int idx0 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx0);
    data.push_back(idx1);

    // data.push_back(idx0);
    // data.push_back(idx2);
    // code.push_back(idx1);
    // code.push_back(idx3);
//     361 data: 5, 3
// 362 code: 21, 19

    if(CLAY_DEBUG){
        cout << "361 data: " << idx0 << ", " << idx1 << endl;
        cout << "362 code: " << idx2 << ", " << idx3 << endl;
    }

    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        // cout<<tmprealidx<<endl;
        realdata.push_back(tmprealidx);
    }

    // calculate idx3
    vector<int> coef3;
    for (int i=0; i<2; i++) {
        int curcoef = _pft_encode_matrix[(_pft_k+1)*_pft_k+i];
        coef3.push_back(curcoef);
    }
    //ecdag->Join(idx3, data, coef3);

    int real_idx3 = get_real_from_short(idx3);
    if(CLAY_DEBUG)
        cout << "393::old: Join(" << real_idx3 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    
    ecdag->Join(real_idx3, realdata, coef3);   //DEBUG
    //ecdag->dump();
    toret.push_back(real_idx3);
    //ecdag->BindX({idx2, idx3});
    if(CLAY_DEBUG)
        cout<<"570 hang Clay::get_uncoupled_from_coupled end"<<endl;
    return toret;
}

vector<int> Clay::get_uncoupled3_from_coupled02(int x, int y, int z, int* z_vec, ECDAG* ecdag) {

    // LOG<<"get_uncoupled3_from_coupled02 start"<<endl;

    vector<int> toret;
    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    // we know 0, 2 and want to repair 1


    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    // node_xy refers to 0, 2
    int node_xy = y*_q + x;
    int z_xy = z;
    //assert(z_vec[y] < x);

    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data; 
    vector<int> code;

    int idx1 = node_xy * _w + z_xy;   //11
    int idx0 = node_sw * _w + z_sw;   //14
    int idx2 = idx0 + (_n+_nu) * _w;   //30
    int idx3 = idx1 + (_n+_nu) * _w;   //27

    data.push_back(idx1);
    data.push_back(idx2);

    vector<int> select_lines = {1,2};
    int _select_matrix[_pft_k*_pft_k];
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }

    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);

        int* test_matirx = Computation::JerasureMatrixMultiply(
            _select_matrix, _invert_matrix, _pft_k, _pft_k, _pft_k, _pft_k, 8
            );
    // print_matrix(test_matirx,_pft_k,_pft_k);



    int _select_vector[_pft_k];
    memcpy(_select_vector,
            _pft_encode_matrix + 3 * _pft_k,
            _pft_k * sizeof(int));
    //int* _coef_vector = jerasure_matrix_multiply(
    //        _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8);
    int* _coef_vector = Computation::JerasureMatrixMultiply(
            _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8
            );

    // calculate idx2
    vector<int> coef;
    for (int i=0; i<_pft_k; i++) coef.push_back(_coef_vector[i]);
    //ecdag->Join(idx1, data, coef);

    int real_idx3 = get_real_from_short(idx3);
    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        realdata.push_back(tmprealidx);
    }
    //27  11 30
    // cout << "507: Join(" << real_idx3 << "; [" << data[0] << ", " << data[1] << "])" << endl;
    ecdag->Join(real_idx3, realdata, coef);
    toret.push_back(real_idx3);
    return toret;

}

vector<int> Clay::get_uncoupled3_from_coupled12(int x, int y, int z, int* z_vec, ECDAG* ecdag) {

    // LOG<<"get_uncoupled3_from_coupled02 start"<<endl;

    vector<int> toret;
    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    // we know 0, 2 and want to repair 1


    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    // node_xy refers to 0, 2
    int node_xy = y*_q + x;
    int z_xy = z;
    //assert(z_vec[y] < x);

    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data; 
    vector<int> code;

    int idx1 = node_xy * _w + z_xy;   //11
    int idx0 = node_sw * _w + z_sw;   //14
    int idx2 = idx0 + (_n+_nu) * _w;   //30
    int idx3 = idx1 + (_n+_nu) * _w;   //27

    data.push_back(idx1);
    data.push_back(idx2);

    vector<int> select_lines = {1,2};
    int _select_matrix[_pft_k*_pft_k];
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }

    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);

        int* test_matirx = Computation::JerasureMatrixMultiply(
            _select_matrix, _invert_matrix, _pft_k, _pft_k, _pft_k, _pft_k, 8
            );
    // print_matrix(test_matirx,_pft_k,_pft_k);



    int _select_vector[_pft_k];
    memcpy(_select_vector,
            _pft_encode_matrix + 3 * _pft_k,
            _pft_k * sizeof(int));
    //int* _coef_vector = jerasure_matrix_multiply(
    //        _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8);
    int* _coef_vector = Computation::JerasureMatrixMultiply(
            _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8
            );

    // calculate idx2
    vector<int> coef;
    for (int i=0; i<_pft_k; i++) coef.push_back(_coef_vector[i]);
    //ecdag->Join(idx1, data, coef);

    int real_idx3 = get_real_from_short(idx3);
    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        realdata.push_back(tmprealidx);
    }
    //27  11 30
    // cout << "507: Join(" << real_idx3 << "; [" << data[0] << ", " << data[1] << "])" << endl;
    ecdag->Join(real_idx3, realdata, coef);
    toret.push_back(real_idx3);
    return toret;

}

vector<int> Clay::get_uncoupled2_from_coupled03(int x, int y, int z, int* z_vec, ECDAG* ecdag) {

    // cout<<"get_uncoupled2_from_coupled03"<<endl;

    vector<int> toret;
    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    // we know 0, 2 and want to repair 1


    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    // node_xy refers to 0, 2
    int node_xy = y*_q + x;
    int z_xy = z;
    //assert(z_vec[y] < x);

    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data; 
    vector<int> code;

    int idx0 = node_xy * _w + z_xy;  //5
    int idx1 = node_sw * _w + z_sw;  //
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx0);  //5
    data.push_back(idx3);  //21

    vector<int> select_lines = {0,3};
    int _select_matrix[_pft_k*_pft_k];
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }

    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);

    // cout<<"573 hang test:"<<endl;
        int* test_matirx = Computation::JerasureMatrixMultiply(
            _select_matrix, _invert_matrix, _pft_k, _pft_k, _pft_k, _pft_k, 8
            );
    // print_matrix(test_matirx,_pft_k,_pft_k);



    int _select_vector[_pft_k];
    memcpy(_select_vector,
            _pft_encode_matrix + 2 * _pft_k,
            _pft_k * sizeof(int));
    //int* _coef_vector = jerasure_matrix_multiply(
    //        _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8);
    int* _coef_vector = Computation::JerasureMatrixMultiply(
            _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8
            );

    // calculate idx2
    vector<int> coef;
    for (int i=0; i<_pft_k; i++) coef.push_back(_coef_vector[i]);
    //ecdag->Join(idx1, data, coef);

    int real_idx2 = get_real_from_short(idx2);
    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        realdata.push_back(tmprealidx);
    }
    //19    5 21
    // cout << "591: Join(" << real_idx2 << "; [" << data[0] << ", " << data[1] << "])" << endl;
    ecdag->Join(real_idx2, realdata, coef);
    toret.push_back(real_idx2);
    return toret;
}

void Clay::get_coupled_from_uncoupled(int x, int y, int z, int* z_vec, ECDAG* ecdag) {

    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    if(CLAY_DEBUG)
        cout<<"debug::get_coupled_from_uncoupled"<<endl;

    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    if(CLAY_DEBUG){
        cout<<"CLAY::get_coupled_from_uncoupled:"<<endl;
        cout<<"(x,y,z):"<<x<<" "<<y<<" "<<z<<endl;
    }

    int node_xy = y*_q+x;
    int z_xy = z;
    assert(z_vec[y] < x);

    int node_sw = y*_q+z_vec[y]; 
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data;
    vector<int> code; 

    int idx0 = node_xy * _w + z_xy;
    int idx1 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx2);
    data.push_back(idx3);
    if(CLAY_DEBUG)
        cout << "429 data: " << idx0<<","<<idx1<<","<<idx2 << ", " << idx3 << endl;

    vector<int> select_lines = {2, 3};
    int _select_matrix[_pft_k*_pft_k];   //???
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }

    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);
    if(CLAY_DEBUG){
        cout<<"_invert_matrix:"<<endl;
        for(int i=0;i<_pft_k*_pft_k ; i++){
            cout<<_invert_matrix[i]<<" ";
        }
        cout<<endl;  //245 244 244 244 
    }

    // calculate idx0
    vector<int> coef0;
    for (int i=0; i<2; i++) {
        int curcoef = _invert_matrix[i];
        coef0.push_back(curcoef);
    }
    //ecdag->Join(idx0, data, coef0);

    int real_idx0 = get_real_from_short(idx0);


    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        //cout<<"realdata:"<<tmprealidx<<endl;
        realdata.push_back(tmprealidx);
    }
    if(CLAY_DEBUG)
        cout << "585: Join(" << real_idx0 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    
    ecdag->Join(real_idx0, realdata, coef0);   //coef0:245 244

    // calculate idx1
    vector<int> coef1;
    for (int i=0; i<2; i++) {
        int curcoef = _invert_matrix[2+i];
        coef1.push_back(curcoef);
    }
    //ecdag->Join(idx1, data, coef1);

    int real_idx1 = get_real_from_short(idx1);
    if(CLAY_DEBUG){
        cout<<"real_idx1:"<<real_idx1<<endl;
        cout << "600: Join(" << real_idx1 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    }
    

    ecdag->Join(real_idx1, realdata, coef1);//coef1:244 244
    
    //ecdag->dump();

    //ecdag->BindX({idx0, idx1});
}

void Clay::get_coupled10_from_uncoupled(int x, int y, int z, int* z_vec, ECDAG* ecdag) {

    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    if(CLAY_DEBUG)
        cout<<"debug::get_coupled_from_uncoupled"<<endl;

    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    if(CLAY_DEBUG){
        cout<<"CLAY::get_coupled_from_uncoupled:"<<endl;
        cout<<"(x,y,z):"<<x<<" "<<y<<" "<<z<<endl;
    }

    int node_xy = y*_q+x;
    int z_xy = z;
    assert(z_vec[y] > x);

    int node_sw = y*_q+z_vec[y]; 
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data;
    vector<int> code; 

    int idx1 = node_xy * _w + z_xy;
    int idx0 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx2);
    data.push_back(idx3);
    if(CLAY_DEBUG)
        cout << "429 data: " << idx0<<","<<idx1<<","<<idx2 << ", " << idx3 << endl;

    vector<int> select_lines = {2, 3};
    int _select_matrix[_pft_k*_pft_k];   
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }

    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);
    if(CLAY_DEBUG){
        cout<<"_invert_matrix:"<<endl;
        for(int i=0;i<_pft_k*_pft_k ; i++){
            cout<<_invert_matrix[i]<<" ";
        }
        cout<<endl;  //245 244 244 244 
    }

    // calculate idx0
    vector<int> coef0;
    for (int i=0; i<2; i++) {
        int curcoef = _invert_matrix[i];
        coef0.push_back(curcoef);
    }
    //ecdag->Join(idx0, data, coef0);

    int real_idx0 = get_real_from_short(idx0);


    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        //cout<<"realdata:"<<tmprealidx<<endl;
        realdata.push_back(tmprealidx);
    }
    if(CLAY_DEBUG)
        cout << "585: Join(" << real_idx0 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    
    ecdag->Join(real_idx0, realdata, coef0);   //coef0:245 244

    // calculate idx1
    vector<int> coef1;
    for (int i=0; i<2; i++) {
        int curcoef = _invert_matrix[2+i];
        coef1.push_back(curcoef);
    }
    //ecdag->Join(idx1, data, coef1);

    int real_idx1 = get_real_from_short(idx1);
    if(CLAY_DEBUG){
        cout<<"real_idx1:"<<real_idx1<<endl;
        cout << "600: Join(" << real_idx1 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    }
    

    ecdag->Join(real_idx1, realdata, coef1);//coef1:244 244
    
    //ecdag->dump();

    //ecdag->BindX({idx0, idx1});
}


vector<int> Clay::get_coupled1_from_pair02(int x, int y, int z, int* z_vec, ECDAG* ecdag) {
    // LOG << "Clay::get_coupled1_from_pair02 start"<<endl;

    vector<int> toret;
    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    // we know 0, 2 and want to repair 1


    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    // node_xy refers to 0, 2
    int node_xy = y*_q + x;
    int z_xy = z;
    assert(z_vec[y] < x);

    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data; 
    vector<int> code;

    int idx0 = node_xy * _w + z_xy;
    int idx1 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx0);
    data.push_back(idx2);

    vector<int> select_lines = {0,2};
    int _select_matrix[_pft_k*_pft_k];
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }

    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);

    int* test_matirx = Computation::JerasureMatrixMultiply(
        _select_matrix, _invert_matrix, _pft_k, _pft_k, _pft_k, _pft_k, 8
        );
    // print_matrix(test_matirx,_pft_k,_pft_k);



    int _select_vector[_pft_k];
    memcpy(_select_vector,
            _pft_encode_matrix + 1 * _pft_k,
            _pft_k * sizeof(int));
    //int* _coef_vector = jerasure_matrix_multiply(
    //        _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8);
    int* _coef_vector = Computation::JerasureMatrixMultiply(
            _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8
            );

    // calculate idx1
    vector<int> coef;
    for (int i=0; i<_pft_k; i++) coef.push_back(_coef_vector[i]);
    //ecdag->Join(idx1, data, coef);

    int real_idx1 = get_real_from_short(idx1);
    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        realdata.push_back(tmprealidx);
    }

//    cout << "484::old: Join(" << idx1 << "; [" << data[0] << ", " << data[1] << "])" << endl;
    ecdag->Join(real_idx1, realdata, coef);
    toret.push_back(real_idx1);
    // LOG << "Clay::get_coupled1_from_pair02 end"<<endl;
    return toret;
}

vector<int> Clay::get_coupled0_from_pair12(int x, int y, int z, int* z_vec, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout<<"debug::get_coupled0_from_pair12"<<endl;
    vector<int> toret;
    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    // we know 0, 2 and want to repair 1


    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    // cout<<"693 hang _pft_encode_matrix:"<<endl;
    // print_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k);
    

    // node_xy refers to 0, 2
    int node_xy = y*_q + x;
    int z_xy = z;
    //assert(z_vec[y] > x);

    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data; 
    vector<int> code;

    int idx0 = node_xy * _w + z_xy;
    int idx1 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;


    // if(z_vec[y] <x){
    //     idx1 = node_xy * _w + z_xy;
    //     idx0 = node_sw * _w + z_sw;
    //     idx2 = idx0 + (_n+_nu) * _w;
    //     idx3 = idx1 + (_n+_nu) * _w;
    // }

    //cout<<"730hang  id:"<<idx0<<"     "<<idx1<<"     "<<idx2<<"     "<<idx3<<endl;

    data.push_back(idx1);
    data.push_back(idx2);
    vector<int> select_lines = {1,2};


    // if(x<z_vec[y]){
    //     cout<<"725hang  x<z_vec[y]"<<endl;
    //      data.push_back(idx1);
    //     data.push_back(idx2);
    //     select_lines = {0,3};
    // }else{
    //     cout<<"730hang  x>z_vec[y]"<<endl;
    //     data.push_back(idx1);
    //     data.push_back(idx2);
    //     select_lines = {2,2};
    // }


    //vector<int> select_lines = {1,2};
    int _select_matrix[_pft_k*_pft_k];
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }
    
    // cout<<"759 hang _select_matrix:"<<endl;
    // print_matrix(_select_matrix, _pft_k, _pft_k);


    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);   //求矩阵的逆

    // cout<<"767hang  _invert_matrix:"<<endl;
    // print_matrix(_select_matrix, _pft_k, _pft_k);


    int _select_vector[_pft_k];
    memcpy(_select_vector,
            _pft_encode_matrix + 0 * _pft_k,
            _pft_k * sizeof(int));

    //  cout<<"776hang  _select_vector:"<<endl;
    // print_matrix(_select_vector, 1,_pft_k);

    //int* _coef_vector = jerasure_matrix_multiply(
    //        _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8);
    int* _coef_vector = Computation::JerasureMatrixMultiply(
            _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8
            );
            
    // cout<<"coef:"<<endl;
    // for(int i =0;i<_pft_k;i++){
    //     cout<<"  "<<_coef_vector[i];
    // }
    // cout<<endl;

    // calculate idx0
    vector<int> coef;
    for (int i=0; i<_pft_k; i++) coef.push_back(_coef_vector[i]);
    //ecdag->Join(idx1, data, coef);


    int real_idx0 = get_real_from_short(idx0);
    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        realdata.push_back(tmprealidx);
    }
    if(CLAY_DEBUG)
        cout << "798: Join(" << real_idx0 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    ecdag->Join(real_idx0, realdata, coef);
    toret.push_back(real_idx0);
    return toret;
}

vector<int> Clay::get_coupled1_from_pair03(int x, int y, int z, int* z_vec, ECDAG* ecdag) {
    // cout<<"debug::get_coupled1_from_pair03"<<endl;
    vector<int> toret;
    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    // we know 0, 2 and want to repair 1
    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    // cout<<"693 hang _pft_encode_matrix:"<<endl;
    // print_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k);
    

    // node_xy refers to 0, 2
    int node_xy = y*_q + x;
    int z_xy = z;
    //assert(z_vec[y] > x);

    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data; 
    vector<int> code;

    int idx1 = node_xy * _w + z_xy;
    int idx0 = node_sw * _w + z_sw;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;


    // if(z_vec[y] <x){
    //     idx1 = node_xy * _w + z_xy;
    //     idx0 = node_sw * _w + z_sw;
    //     idx2 = idx0 + (_n+_nu) * _w;
    //     idx3 = idx1 + (_n+_nu) * _w;
    // }

    //cout<<"730hang  id:"<<idx0<<"     "<<idx1<<"     "<<idx2<<"     "<<idx3<<endl;

    data.push_back(idx0);
    data.push_back(idx3);
    vector<int> select_lines = {0,3};


    // if(x<z_vec[y]){
    //     cout<<"725hang  x<z_vec[y]"<<endl;
    //      data.push_back(idx1);
    //     data.push_back(idx2);
    //     select_lines = {0,3};
    // }else{
    //     cout<<"730hang  x>z_vec[y]"<<endl;
    //     data.push_back(idx1);
    //     data.push_back(idx2);
    //     select_lines = {2,2};
    // }


    //vector<int> select_lines = {1,2};
    int _select_matrix[_pft_k*_pft_k];
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }
    
    // cout<<"880 hang _select_matrix:"<<endl;
    // print_matrix(_select_matrix, _pft_k, _pft_k);


    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);   //求矩阵的逆

    // cout<<"767hang  _invert_matrix:"<<endl;
    // print_matrix(_select_matrix, _pft_k, _pft_k);


    int _select_vector[_pft_k];
    memcpy(_select_vector,
            _pft_encode_matrix + 1 * _pft_k,
            _pft_k * sizeof(int));

    //  cout<<"776hang  _select_vector:"<<endl;
    // print_matrix(_select_vector, 1,_pft_k);

    //int* _coef_vector = jerasure_matrix_multiply(
    //        _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8);
    int* _coef_vector = Computation::JerasureMatrixMultiply(
            _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8
            );
            
    // cout<<"coef:"<<endl;
    // for(int i =0;i<_pft_k;i++){
    //     cout<<"  "<<_coef_vector[i];
    // }
    // cout<<endl;

    // calculate idx0
    vector<int> coef;
    for (int i=0; i<_pft_k; i++) coef.push_back(_coef_vector[i]);
    //ecdag->Join(idx1, data, coef);


    int real_idx1 = get_real_from_short(idx1);
    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        realdata.push_back(tmprealidx);
    }

    // cout << "798: Join(" << real_idx1 << "; [" << realdata[0] << ", " << realdata[1] << "])" << endl;
    ecdag->Join(real_idx1, realdata, coef);
    toret.push_back(real_idx1);
    return toret;
}

vector<int> Clay::get_coupled0_from_pair13(int x, int y, int z, int* z_vec, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout<<"debug::get_coupled0_from_pair13 begin"<<endl;   
    
    vector<int> toret;
    // the coupled index are 0, 1
    // the uncoupled index are 2, 3
    // 0, 2 refer to the node where z[y] < x
    // we know 1, 3 and want to repair 0

    memset(_pft_encode_matrix, 0, CLAYPFT_N_MAX * CLAYPFT_N_MAX * sizeof(int));
    generate_matrix(_pft_encode_matrix, _pft_k+_pft_m, _pft_k, 8);

    // node_xy refers to 1, 3
    int node_xy = y*_q + x;
    int z_xy = z;
    assert(z_vec[y] > x);

    int node_sw = y*_q+z_vec[y];
    int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

    vector<int> data; 
    vector<int> code;

    int idx0 = node_sw * _w + z_sw;
    int idx1 = node_xy * _w + z_xy;
    int idx2 = idx0 + (_n+_nu) * _w;
    int idx3 = idx1 + (_n+_nu) * _w;

    data.push_back(idx1);
    data.push_back(idx3);

    vector<int> select_lines = {1,3};
    int _select_matrix[_pft_k*_pft_k];
    for (int i=0; i<_pft_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _pft_k,
                _pft_encode_matrix + sidx * _pft_k,
                sizeof(int) * _pft_k);
    }

    int _invert_matrix[_pft_k*_pft_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _pft_k, 8);
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _pft_k, 8);

    int _select_vector[_pft_k];
    memcpy(_select_vector,
            _pft_encode_matrix + 0 * _pft_k,
            _pft_k * sizeof(int));
    //int* _coef_vector = jerasure_matrix_multiply(
    //        _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8);
    int* _coef_vector = Computation::JerasureMatrixMultiply( 
            _select_vector, _invert_matrix, 1, _pft_k, _pft_k, _pft_k, 8
            );

    // calculate idx0
    vector<int> coef;
    for (int i=0; i<_pft_k; i++) coef.push_back(_coef_vector[i]);
    //ecdag->Join(idx0, data, coef);

    int real_idx0 = get_real_from_short(idx0);
    vector<int> realdata;
    for (auto tmpidx: data) {
        int tmprealidx = get_real_from_short(tmpidx);
        realdata.push_back(tmprealidx);
    }

    //    cout << "548::old: Join(" << idx0 << "; [" << data[0] << ", " << data[1] << "])" << endl;
    ecdag->Join(real_idx0, realdata, coef);
    toret.push_back(real_idx0);
    if(CLAY_DEBUG)
        cout<<"debug::get_coupled0_from_pair13 end"<<endl;
    return toret;
}

vector<int> Clay::decode_uncoupled(vector<int> erased_chunks, int z, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout<<"Clay::decode_uncoupled begin"<<endl;
    vector<int> toret;
    // in the layer z, we focus on the uncoupled data
    // we need to repair chunks in erased_chunks from _mds_k chunks
    memset(_mds_encode_matrix, 0, CLAYMDS_N_MAX * CLAYMDS_N_MAX * sizeof(int));
    //_mds_encode_matrix 
    // int * matrix = reed_sol_vandermonde_coding_matrix(_mds_k, _mds_m, 8);
    // for(int i=0;i<1024;i++){
    //     _mds_encode_matrix[i]=matrix[i];
    // }
    generate_matrix(_mds_encode_matrix, _mds_k+_mds_m, _mds_k, 8);
    if(CLAY_DEBUG)
        cout<<"1018   hang  _mds_encode_matrix:"<<endl;
    // print_matrix(_mds_encode_matrix, _mds_k+_mds_m, _mds_k);

    // figure out the select lines
    vector<int> select_lines;
    vector<int> data;
    for (int i=0; i<_mds_k+_mds_m; i++) {
        if (find(erased_chunks.begin(), erased_chunks.end(), i) == erased_chunks.end()) {
            select_lines.push_back(i);
            int curidx = i * _w + z + (_n+_nu) * _w;
            data.push_back(curidx);
        }

        if (select_lines.size() == _mds_k)
            break;
    }

    //  select_lins
    // cout<<"1033hang select line:";
    // for(int i =0;i<_mds_k;i++){
    //     cout<<select_lines[i]<<"  ";
    // }
    // cout<<endl;

    // XL add start
//    cout << "1043 before: ";
//    for (int i=0; i<data.size(); i++) 
//        cout << data[i] << " ";
//    cout << endl;

    for (int i=0; i<data.size(); i++) {
        int idx = data[i];
        if (_identical.find(idx) != _identical.end())
            data[i] = _identical[idx];
    }

//    cout << "1043 after: ";
//    for (int i=0; i<data.size(); i++) 
//        cout << data[i] << " ";
//    cout << endl;
    // XL add end

    int _select_matrix[_mds_k*_mds_k];
    for (int i=0; i<_mds_k; i++) {
        int sidx = select_lines[i];
        memcpy(_select_matrix + i * _mds_k,
                _mds_encode_matrix + sidx * _mds_k,
                sizeof(int) * _mds_k); 
    }

    int _invert_matrix[_mds_k*_mds_k];
    //jerasure_invert_matrix(_select_matrix, _invert_matrix, _mds_k, 8); 
    Computation::JerasureInvertMatrix(_select_matrix, _invert_matrix, _mds_k, 8);

    // cout<<"1072   hang  _invert_matrix:"<<endl;
    // print_matrix(_invert_matrix, _mds_k, _mds_k);       


    vector<int> tobind;
    for (int i=0; i<erased_chunks.size(); i++) {
        int cidx = erased_chunks[i];
        //cout<<"1075hang  i,cidx:"<<i<<"    "<<cidx<<endl;
        int _select_vector[_mds_k];
        memcpy(_select_vector,
                _mds_encode_matrix + cidx * _mds_k,
                _mds_k * sizeof(int));

        // cout<<"1081hang  _select_vector:";
        // for(int j=0; j<_mds_k;j++){
        //     cout<<_select_vector[j]<<" ";
        // }
        // cout<<endl;

        
        //        int* _coef_vector = jerasure_matrix_multiply(
        //                _select_vector, _invert_matrix, 1, _mds_k, _mds_k, _mds_k, 8);
        int* _coef_vector = Computation::JerasureMatrixMultiply( 
                _select_vector, _invert_matrix, 1, _mds_k, _mds_k, _mds_k, 8
                );
        
        //cout<<"1094   hang  _coef_vector:"<<endl;
        //print_matrix(_coef_vector, _mds_k, _mds_k+_mds_m);       
        
        
        vector<int> coef;
        for (int m=0; m<_mds_k; m++) coef.push_back(_coef_vector[m]);

        //  coef
        // cout<<"1106hang coef:";
        // for(int i =0;i<_mds_k;i++){
        //     cout<<coef[i]<<"  ";
        // }
        // cout<<endl;   

        int targetidx = cidx * _w + z + (_n+_nu) * _w;
        //ecdag->Join(targetidx, data, coef);

        if(CLAY_DEBUG)
            cout << "674 before: targetidx = " << targetidx << endl;
        if (_identical.find(targetidx) != _identical.end())
            targetidx = _identical[targetidx];
        if(CLAY_DEBUG)
            cout << "677 after: targetidx = " << targetidx << endl;

        //if (_encode & targetidx < (_n + _nu) * _w)
        //    continue;

        int real_targetidx = get_real_from_short(targetidx);
        vector<int> realdata;
        for (auto tmpidx: data) {
            int tmprealidx = get_real_from_short(tmpidx);
            realdata.push_back(tmprealidx);
        }
        

        // cout << "1049: Join(" << targetidx << "; [" << data[0] << ", " << data[1] << "])" << endl;
//        cout << "605::old: Join(" << targetidx << "; [" << data[0] << ", " << data[1] << "])" << endl;
        // cout<<"1131 hang   Join(" << targetidx << "; [";
        // for(int i=0;i<realdata.size();i++){
        //     if(i==realdata.size()-1) cout<<realdata[i]<<"])"<<endl;
        //         else cout<<realdata[i]<<",";
        // }  
        ecdag->Join(real_targetidx, realdata, coef);
        //ecdag->dump();
        toret.push_back(real_targetidx);
    }
    if(CLAY_DEBUG)
        cout<<"Clay::decode_uncoupled end"<<endl;
    return toret; 
}

// int Clay::is_repair(vector<int> want_to_read, vector<int> avail) {
//     if (want_to_read.size() > 1) return 0; 

//     int i = * want_to_read.begin();
//     int lost_node_id = (i < _k) ? i : i + _nu;
//     for (int x = 0; x < _q; x++) {         //  
//         int node = (lost_node_id / _q) * _q + x;    //lost_node_id 所在组内的第一个节点编号 + x
//         node = (node < _k) ? node : node - _nu;
//         if (node != i) { // node in the same group other than erased node
//             if (std::find(avail.begin(), avail.end(), node) == avail.end()){
//                 return 0;
//             }else{
//                 // cout<<"node in avail   node:"<<node<<endl;
//             }
//         }
//     }    
//     if ((int)avail.size() < _d) {return 0;}
//     else{
//         // cout<<"avail.size >= _d  avail.size():"<<avail.size()<<"   _d:"<<_d<<endl;
//     }

//     return 1;
// }
int Clay::is_repair(vector<int> want_to_read,
        vector<int> avail) {
    //single failure 
    if(want_to_read.size() == 1){
            int i = * want_to_read.begin();
            int lost_node_id = (i < _k) ? i : i + _nu;
            for (int x = 0; x < _q; x++) {         //  
                int node = (lost_node_id / _q) * _q + x;    //lost_node_id 所在组内的第一个节点编号 + x
                node = (node < _k) ? node : node - _nu;
                if (node != i) { // node in the same group other than erased node
                    if (std::find(avail.begin(), avail.end(), node) == avail.end()){
                        return 0;
                    }else{
                        cout<<"node in avail   node:"<<node<<endl;
                    }
                }
            }    
            if ((int)avail.size() < _d) {return 0;}
            else{
                cout<<"avail.size >= _d  avail.size():"<<avail.size()<<"   _d:"<<_d<<endl;
            }
        return 1;
    }else if (want_to_read.size() > 1){
        //mutiple failure repairable failure  patterns.


        int count = get_repair_sub_chunk_count(want_to_read);
        //d < n-1
        if(_d < _k +_m -1){
            if((want_to_read.size() <= _k +_m -_d)&&(_d * count <= _k * _sub_chunk_no)){
                return 1;
            }else return 0;
        }else if(_d = _k + _m -1){
            if(want_to_read.size()<= _q - 1){
                        //want_to_read 在同一个y -column
                int i = *want_to_read.begin();
                int lost_node_index = (i < _k) ? i : i+_nu;
                const int y_lost = lost_node_index / _q;

                for(int i=1;i<want_to_read.size();i++){
                    int lost_node_indexids = (want_to_read[i] < _k) ? want_to_read[i] : want_to_read[i]+_nu;
                    if((lost_node_indexids/_q)==y_lost) continue;                    
                    else{
                        return 0;
                    }
                }
                return 1;
            }else return 0;
        }
        return 0;
    } 
}

// void Clay::minimum_to_repair(vector<int> want_to_read, 
//         vector<int> available_chunks, 
//         unordered_map<int, vector<pair<int, int>>>& minimum) {
//     if(CLAY_DEBUG)
//         cout << "Clay::minimum_to_repair begin" << endl;
//     assert(want_to_read.size() == 1);
//     int i = want_to_read[0];
//     int lost_node_index = (i < _k) ? i : i+_nu;
//     int rep_node_index = 0;

//     // add all the nodes in lost node's y column.
//     vector<pair<int, int>> sub_chunk_ind;
//     get_repair_subchunks(lost_node_index, sub_chunk_ind);

//     if ((available_chunks.size() >= _d)) {
//         for (int j = 0; j < _q; j++) {
//             if (j != lost_node_index%_q) {
//                 rep_node_index = (lost_node_index/_q)*_q+j;
//                 if (rep_node_index < _k) {
//                     minimum.insert(make_pair(rep_node_index, sub_chunk_ind));
//                 } else if (rep_node_index >= _k+_nu) {
//                     minimum.insert(make_pair(rep_node_index-_nu, sub_chunk_ind));
//                 }
//             }
//         }

//         for (auto chunk : available_chunks) {
//             if (minimum.size() >= _d) {
//                 break;
//             }
//             if (minimum.find(chunk) == minimum.end()) {
//                 minimum.emplace(chunk, sub_chunk_ind);
//             }
//         }
//     }
//     if(CLAY_DEBUG)
//         cout << "Clay::minimum_to_repair end" << endl;
// }
void Clay::minimum_to_repair(vector<int> want_to_read,
        vector<int> available_chunks,
        unordered_map<int, vector<pair<int, int>>>& minimum) {


        if(want_to_read.size() == 1){
            assert(want_to_read.size() == 1);  //TODO ：修改为多块可以
            //int i = want_to_read[0];
            int i =  *want_to_read.begin();
            int lost_node_index = (i < _k) ? i : i+_nu;
            int rep_node_index = 0;

            // add all the nodes in lost node's y column.
            vector<pair<int, int>> sub_chunk_ind;
            get_repair_subchunks(lost_node_index, sub_chunk_ind);

            if ((available_chunks.size() >= _d)) {
                for (int j = 0; j < _q; j++) {
                    if (j != lost_node_index%_q) {
                        rep_node_index = (lost_node_index/_q)*_q+j;
                        if (rep_node_index < _k) {
                            minimum.insert(make_pair(rep_node_index, sub_chunk_ind));
                        } else if (rep_node_index >= _k+_nu) {
                            minimum.insert(make_pair(rep_node_index-_nu, sub_chunk_ind));
                        }
                    }
                }

                for (auto chunk : available_chunks) {
                    if (minimum.size() >= _d) {
                        break;
                    }
                    if (minimum.find(chunk) == minimum.end()) {
                        minimum.emplace(chunk, sub_chunk_ind);
                    }
                }
         } 
    }else if(want_to_read.size() > 1){
        if(_d < _k + _m -1){
            //d<n-1
            //The helper nodes are to be chosen in such a way that if a y-section contains a failed node, then all the surviving nodes in that
            //y-section must act as helper nodes. If no such choice of helper nodes is available then it is not a repairable failure pattern. 
        }else if(_d == _k + _m -1){
            
            //d=n-1
            //As the number of surviving nodes is smaller than d in such a case, all the surviving nodes are picked as helper nodes.
            vector<pair<int, int>> sub_chunk_ind;
            for(int i=0;i<want_to_read.size();i++){
                vector<pair<int, int>> temp_sub_chunk_ind;
                get_repair_subchunks(want_to_read[i], temp_sub_chunk_ind);   //TODO!!!
                //sub_chunk_ind.push_back(temp_sub_chunk_ind);
                sub_chunk_ind.insert(sub_chunk_ind.end(),temp_sub_chunk_ind.begin(),temp_sub_chunk_ind.end());
            }
            
            //get_repair_subchunks(i, sub_chunk_ind);   //TODO!!!
            for(int i=0;i< _k + _m ;i++){
                
                if (find(want_to_read.begin(), want_to_read.end(), i) != want_to_read.end()) {
                    // i is a helper
                    //vector<pair<int, int>> sub_chunk_ind;
                    //get_repair_subchunks(i, sub_chunk_ind);   //TODO!!!
                    
                    //cout<<"1005 hang:"<<i<<endl;
                    
                    for(int j=0;j<available_chunks.size();j++){
                        if (j < _k) {
                            minimum.insert(make_pair(available_chunks[j], sub_chunk_ind));
                        } else if (j >= _k+_nu) {
                            minimum.insert(make_pair(available_chunks[j]-_nu, sub_chunk_ind));
                        }
                    }

                    // for (auto chunk : available_chunks) {
                    //     if (minimum.find(chunk) == minimum.end()) {
                    //         minimum.emplace(chunk, sub_chunk_ind);
                    //     }
                    // }
            } 
            
        }
            // vector<pair<int, int>> default_subchunks;
            // default_subchunks.push_back(make_pair(0, _sub_chunk_no));  //表示子块范围
            // for (auto &&id : available_chunks) {
            //     minimum.insert(make_pair(id, default_subchunks));
            // }
        }
    }

}

void Clay::get_repair_subchunks(int lost_node, vector<pair<int, int>> &repair_sub_chunks_ind) {
    int y_lost = lost_node / _q;
    int x_lost = lost_node % _q;
    int seq_sc_count = pow_int(_q, _t-1-y_lost);
    int num_seq = pow_int(_q, y_lost);
    int index = x_lost * seq_sc_count;
    for (int ind_seq = 0; ind_seq < num_seq; ind_seq++) {
        repair_sub_chunks_ind.push_back(make_pair(index, seq_sc_count));
        index += _q * seq_sc_count;
    }
}

int Clay::get_repair_sub_chunk_count(vector<int> want_to_read) {
    // get the number of sub chunks we should repair???
    int repair_subchunks_count = 1;
    // record the number of failures in each y
    int weight_vector[_t];
    memset(weight_vector, 0, _t*sizeof(int));
    for (auto i: want_to_read) {
        weight_vector[i/_q]++;
    }
    for (int y = 0; y < _t; y++) repair_subchunks_count = repair_subchunks_count*(_q-weight_vector[y]);
    return _sub_chunk_no - repair_subchunks_count;
}




ECDAG* Clay::Decode(vector<int> from, vector<int> to) {
    if(CLAY_DEBUG)
        cout << "Clay::Decode begin" << endl;
    _encode = false;
    
    ECDAG* ecdag = new ECDAG();
    
    vector<int> want_to_read;
//    int lost = to[0] / _w;
    // cout<<"1075hang want_to_read";
    for(int i=0;i<to.size();i=i+_w){
        // cout<<i<<":  "<<to[i]<<endl;
        want_to_read.push_back(to[i]/_w);
    }

    if (CLAY_DEBUG){
        cout << "Decode: lost = " ;
        for(int i=0;i<want_to_read.size();i++){
            cout<<want_to_read[i]<<"  ";
        }
        cout<<endl;
    }
        
    vector<int> avail;
    for (int i=0; i<(_k+_nu+_m); i++) {
    //for (int i=0; i<( _k + _m); i++) {
        //if (i == lost) continue;
        if(find(want_to_read.begin(), want_to_read.end(), i) != want_to_read.end()) continue;
        avail.push_back(i);
    }

    if (1){
        cout<<"Clay::Decode  avail:"<<endl;
        for (int i = 0; i < avail.size(); i++)
        {
            cout<<avail[i]<<"  ";  //0  1  2  3  4  5  6  7  8  9  11  14  15  
        }
        cout <<endl;
    }

    // cout <<endl;
    //want_to_read.pop_back();
    cout << "1104 hang Decode: lost = " << endl;  //11 12 13
        for(int i=0;i<want_to_read.size();i++){
            cout<<want_to_read[i]<<"  ";
        }
        cout<<endl;

    // minimum map records symbols we should read in each node <startid, succeed number>
    unordered_map<int, vector<pair<int, int>>> minimum_map;
    //minimum_to_repair(want_to_read, avail, minimum_map);
    minimum_to_decode(want_to_read, avail, minimum_map);//UNDO
    if(CLAY_DEBUG)
    {
        cout << "tester:: minimum_map: " << endl;
        for (auto item: minimum_map) {
            cout << "  " << item.first << " : ";
            for (auto pitem: item.second) {
                cout << "(" << pitem.first << ", " << pitem.second << ") ";
            }
            cout << endl;
        }
    }
    
    
    unordered_map<int, bool> chunks;
    for (auto item: minimum_map) {
        int nodeid=item.first;
        chunks.insert(make_pair(nodeid, true)); //含13
    }

        int repair_sub_chunk_no = get_repair_sub_chunk_count(want_to_read);
    
        vector<pair<int, int>> repair_sub_chunks_ind;
    
        unordered_map<int, bool> recovered_data;
        unordered_map<int, bool> helper_data;
        vector<int> aloof_nodes;


    if (is_repair(want_to_read, avail)) {
        cout<<"1772 hang is_repair return 1"<<endl;
        //minimum to repair
        //claycode->repair(want_to_read, available_chunks, decoded, available_chunk_size);

        //单节点修复


        for (int i =  0; i < _k + _m; i++) {
            if (chunks.find(i) != chunks.end() && find(want_to_read.begin(), want_to_read.end(), i) == want_to_read.end()) { //05 add
            // i is a helper
                if (i<_k) {
                    helper_data.insert(make_pair(i, true));
                 } else {
                    helper_data.insert(make_pair(i+_nu, true));
                }
            } else {
                //if (find(want_to_read.begin(), want_to_read.end(), i) == want_to_read.end()) {//不一致的地方
                if (find(want_to_read.begin(), want_to_read.end(), i) == want_to_read.end()) { // aloof node case. // 11 12 13
                    int aloof_node_id = (i < _k) ? i: i+_nu;
                    cout << "1892 hang aloof_node_id:"<<aloof_node_id<<endl;
                    aloof_nodes.push_back(aloof_node_id);
                } else {
                    int lost_node_id = (i < _k) ? i : i+_nu;
                    cout << "1896 hang lost_node_id:"<<lost_node_id<<endl;
                    recovered_data.insert(make_pair(lost_node_id, true));
                    get_repair_subchunks(lost_node_id, repair_sub_chunks_ind);
                }
            }
        }

        // this is for shortened codes i.e., when nu > 0
        for (int i=_k; i < _k+_nu; i++) {
            helper_data.insert(make_pair(i, true));
        }

        assert(helper_data.size() + aloof_nodes.size() + recovered_data.size() == _q*_t);

        repair_one_lost_chunk(recovered_data, aloof_nodes,
                helper_data, repair_sub_chunks_ind, ecdag);    //TODO:注意ecdag部分

    }else{
        //claycode->minimum_to_decode(want_to_read, avail, minimum_map);
        //claycode->rs_decode(want_to_read, available_chunks, decoded,blocksize);
        //多节点修复
        if(CLAY_DEBUG){
            cout<<"is_repair return 0"<<endl;

            cout<<"Decode::want_to_read:";
            for(int i=0;i<want_to_read.size();i++){
                cout<<want_to_read[i]<<"  ";
            }
            cout<<endl;
            
            cout<<"Decode::chunks:";
            for(auto kv:chunks){
                cout<<kv.first<<"  "<<kv.second<<endl;
            }
            cout<<endl;

            //ecdag->dump();

        }


        rs_decode(want_to_read, chunks, recovered_data,ecdag);//TODO:多节点修复

        //ecdag->dump();
    }
    if(CLAY_DEBUG)
        cout << "Clay::Decode end" << endl;
    return ecdag;
}

void Clay::repair_one_lost_chunk(unordered_map<int, bool>& recovered_data,
        vector<int>& aloof_nodes,
        unordered_map<int, bool>& helper_data,
        vector<pair<int,int>> &repair_sub_chunks_ind,
        ECDAG* ecdag) {
    
    // cout << "Clay::repair_one_lost_chunk start" << endl;
    // cout << "recovered_data: ";
    // for (auto item: recovered_data)
    //     cout << item.first << " ";
    // cout << endl;
    
    // cout << "aloof_nodes: ";
    // for (auto item: aloof_nodes)
    //     cout << item << " ";
    // cout << endl;
    
    // cout << "helper_data: ";
    // for (auto item: helper_data)
    //     cout << item.first << " ";
    // cout << endl;
    
    // cout << "repair_sub_chunks_ind: ";
    // for (auto item: repair_sub_chunks_ind) {
    //     cout << "(" << item.first << ", " << item.second << ") ";
    // }
    // cout << endl;
    
    int repair_subchunks = recovered_data.size() * _sub_chunk_no /_q;
    //int repair_subchunks = _sub_chunk_no / _q;
    int z_vec[_t];
    unordered_map<int, vector<int>> ordered_planes;
    unordered_map<int, int> repair_plane_to_ind;

    int count_retrieved_sub_chunks = 0;
    int plane_ind = 0;

    for (auto item: repair_sub_chunks_ind) {
        int index = item.first;
        int count = item.second;

        for (int j = index; j < index + count; j++) {
            get_plane_vector(j, z_vec);

            if (CLAY_DEBUG) {
                cout << "plane vector: ( ";
                for (int ii=0; ii<_t; ii++)
                    cout << z_vec[ii] << " ";
                cout << ")" << endl;
            }

            int order = 0;
            // check across all erasures and aloof nodes
            // check recover node the number of vertex colored red starting from the index
            for (auto ritem: recovered_data) {
                int node = ritem.first;
                if (node % _q == z_vec[node / _q]) order++;
            }
            for (auto node : aloof_nodes) {
                if (node % _q == z_vec[node / _q]) order++;
            }
            assert(order > 0);
            // order means the number of red vertex in the plane j among all recover node and aloof
            // node
            if (ordered_planes.find(order) == ordered_planes.end()) {
                vector<int> curlist;
                curlist.push_back(j);
                ordered_planes.insert(make_pair(order, curlist));
            } else {
                ordered_planes[order].push_back(j);
            }
            repair_plane_to_ind[j] = plane_ind;
            plane_ind++;
        }
    }

    cout<<"1924hang plane_ind == repair_subchunks:"<<plane_ind<<"   "<<repair_subchunks<<endl;

    assert(plane_ind == repair_subchunks);

    int plane_count = 0;

    int lost_chunk;
    int count = 0;
    for (auto item: recovered_data) {
        lost_chunk = item.first;
        count++;
    }
    //assert(count == 1);
    // add all nodes in the same y of the lost chunk into erasures, as well as the aloof node
    vector<int> erasures;
    for (int i = 0; i < _q; i++) {
        erasures.push_back(lost_chunk - lost_chunk % _q + i);
    }
    for (auto node : aloof_nodes) {
        erasures.push_back(node);
    }

    cout << "erasures: ";
    for (auto item: erasures)
        cout << item << " ";
    cout << endl;

    vector<int> orderlist;
    for (auto item: ordered_planes)  {
        int order = item.first;
        orderlist.push_back(order);
    }
    sort(orderlist.begin(), orderlist.end());

    cout << "orderlist: ";
    for (auto item: orderlist)
        cout << item << " ";
    cout << endl;

    vector<int> list1;
    vector<int> list2;
    vector<int> list3;
    // we start from the planes that have the minimum number of red vertex
    for (int order: orderlist) {
        plane_count += ordered_planes[order].size();

        // calculate uncoupled value from coupled value read from disk
        for (auto z : ordered_planes[order]) {
            get_plane_vector(z, z_vec);
            for (int y = 0; y < _t; y++) {
                for (int x = 0; x < _q; x++) {
                    int node_xy = y*_q + x;
                    if (find(erasures.begin(), erasures.end(), node_xy) == erasures.end()) {
                        // current node_xy does not erasure
                        assert(helper_data.find(node_xy) != helper_data.end());

                        int z_sw = z + (x - z_vec[y])*pow_int(_q,_t-1-y);
                        int node_sw = y*_q + z_vec[y];

                        if (find(aloof_nodes.begin(), aloof_nodes.end(), node_sw) != aloof_nodes.end()){
                            assert(repair_plane_to_ind.find(z) != repair_plane_to_ind.end());
                            assert(repair_plane_to_ind.find(z_sw) != repair_plane_to_ind.end());

                            if(z_vec[y]>x){
                                vector<int> tmplist = get_uncoupled3_from_coupled12(x, y, z, z_vec, ecdag);
                                //get_uncoupled3_from_coupled12(x, y, z, z_vec, ecdag);
                                //get_uncoupled_from_coupled(x, y, z, z_vec, ecdag);
                                for (auto item: tmplist)
                                    list1.push_back(item);                                
                            }else{
                                vector<int> tmplist = get_uncoupled2_from_coupled03(x, y, z, z_vec, ecdag);
                                //get_uncoupled_from_coupled(x, y, z, z_vec, ecdag);
                                for (auto item: tmplist)
                                    list1.push_back(item);
                            }
                        }else{
                            assert(helper_data.find(node_sw) != helper_data.end());
                            assert(repair_plane_to_ind.find(z) != repair_plane_to_ind.end());

                            if(z_vec[y]==x){
                                int coupled_idx = node_xy * _w + z;
                                int uncoupled_idx = coupled_idx + (_n+_nu) * _w;
                                //ecdag->Join(uncoupled_idx, {coupled_idx}, {1});
                                int real_coupled_idx = get_real_from_short(coupled_idx);
                                int real_uncoupled_idx = get_real_from_short(uncoupled_idx);

                                // cout<<"1998 hang: z_vec[y]==x:real_coupled_idx:"<<real_coupled_idx<<endl;
                                // cout<<"1998 hang: z_vec[y]==x:real_uncoupled_idx:"<<real_uncoupled_idx<<endl;

                                list1.push_back(real_uncoupled_idx);
                            }else if(z_vec[y] < x){
                                assert(repair_plane_to_ind.find(z_sw) != repair_plane_to_ind.end());

                                vector<int> tmplist = get_uncoupled2_from_coupled(x, y, z, z_vec, ecdag);
                                for (auto item: tmplist)
                                    list1.push_back(item);

                            }else if(z_vec[y]>x){
                                assert(repair_plane_to_ind.find(z_sw) != repair_plane_to_ind.end());
                                
                                vector<int> tmplist = get_uncoupled3_from_coupled(x, y, z, z_vec, ecdag);
                                for (auto item: tmplist)
                                    list1.push_back(item);                                

                            }
                        }
                    }
                }
            }

            //assert(erasures.size() <= _m);
            vector<int> tmplist = decode_uncoupled(erasures, z, ecdag);

            for (auto item: tmplist)
                list2.push_back(item);

            // check node in erasures
            // if the node is recovery node, the vertex is colored red, repair coupled from
            // uncoupled
            // if the node is not recovery node, pair the vertex with the one in the recovery node
            // and repair the paired

            for (auto i : erasures) {
                int x = i % _q;
                int y = i / _q;

                int node_sw = y*_q+z_vec[y];
                int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);

                if (find(aloof_nodes.begin(), aloof_nodes.end(), i) == aloof_nodes.end()){
                    if(x==z_vec[y]){
                     // red vertex, we directly get coupled data from uncoupled data
                        int coupled_idx = i * _w + z;
                        int uncoupled_idx = coupled_idx + (_n+_nu) * _w;
                        //ecdag->Join(coupled_idx, {uncoupled_idx}, {1});
                    
                        int real_coupled_idx = get_real_from_short(coupled_idx);
                        int real_uncoupled_idx = get_real_from_short(uncoupled_idx);
                    
//                    cout << "914::old: Join(" << coupled_idx << "; [" << uncoupled_idx << "])" << endl;
//                  // XL comment start
                    //ecdag->Join(real_coupled_idx, {real_uncoupled_idx}, {1});
                    // XL comment end
                        list3.push_back(real_coupled_idx);
                    }else{
                        assert(y == lost_chunk / _q);
                        if (helper_data.find(i) != helper_data.end()){
                            if (z_vec[y] < x) {
                                //if (node_sw == lost_chunk) {
                                    vector<int> tmplist = get_coupled1_from_pair02(x, y, z, z_vec, ecdag);
                                    for (auto item: tmplist)
                                        list3.push_back(item);
                                //}
                            } else if (z_vec[y] > x) {
                               // if (node_sw == lost_chunk) {
                                    vector<int> tmplist = get_coupled0_from_pair13(x, y, z, z_vec, ecdag);
                                    for (auto item: tmplist)
                                        list3.push_back(item);
                                //}
                            }
                        }else{
                            
                            int x_sw = node_sw % _q;
                            int y_sw = node_sw / _q;

                            if(x <z_vec[y]){
                                get_coupled10_from_uncoupled(x, y, z, z_vec, ecdag);
                            
                            }
                            //TODO：

                        }
                    }
                }

            }
        }
    }
    if(CLAY_DEBUG)
        cout << "Clay::repair_one_lost_chunk end" << endl;
}

void Clay::generate_matrix(int* matrix, int rows, int cols, int w) {
    int k = cols;
    int n = rows;
    int m = n - k;

    memset(matrix, 0, rows * cols * sizeof(int));
    for(int i=0; i<k; i++) {
        matrix[i*k+i] = 1; 
    }

    int a = 1;
    for (int i=0; i<m; i++) {
        int tmp = 1;
        for (int j=0; j<k; j++) {
            matrix[(i+k)*cols+j] = tmp;
            tmp = Computation::singleMulti(tmp, a, w);
        }
        a = a + 1;
    }
}

// void Clay::generate_matrix_ceph(int* matrix, int rows, int cols, int w) {
//     int i, j;
//     int *coding_matrix;

//     coding_matrix = reed_sol_vandermonde_coding_matrix(rows - cols, cols, w);
//     if (coding_matrix == NULL) {
//         printf("Error: Failed to generate coding matrix.\n");
//         return;
//     }

//     for (i = 0; i < rows; i++) {
//         for (j = 0; j < cols; j++) {
//             matrix[i * cols + j] = coding_matrix[i * cols + j];
//         }
//     }

//     free(coding_matrix);
// }

void Clay::print_matrix(int* matrix, int row, int col) {
   for (int i=0; i<row; i++) {
       for (int j=0; j<col; j++) {
           cout << matrix[i*col+j] << " ";
       }
       cout << endl;
   }
}


int Clay::get_real_from_short(int idx) {
    if (_short2real.find(idx) == _short2real.end())
        return idx;
    else
        return _short2real[idx];
}

int Clay::get_short_from_real(int idx) {
    if (_real2short.find(idx) == _real2short.end())
        return idx;
    else
        return _real2short[idx];
}

void Clay::minimum_to_decode(vector<int> want_to_read,
        vector<int> available,                                                
        unordered_map<int, vector<pair<int, int>>>& minimum) {
        if(CLAY_DEBUG)
            cout << "Clay::minimum_to_decode begin" << endl;
        if (is_repair(want_to_read, available)) {
            // cout<<"1489 return 1"<<endl;
            return minimum_to_repair(want_to_read, available, minimum);
        } else {
            // cout<<"1492hang is_repair return 0"<<endl;
            return rs_minimum_to_decode(want_to_read, available, minimum);
        }
   // return minimum_to_repair(want_to_read, available, minimum);
}

void Clay::rs_minimum_to_decode(vector<int> want_to_read,
        vector<int> available,                                                
        unordered_map<int, vector<pair<int, int>>>& minimum){
        if(CLAY_DEBUG)
            cout << "Clay::rs_minimum_to_decode begin" << endl;
        
        set<int> minimum_shard_ids;
        rs1_minimum_to_decode(want_to_read, available, &minimum_shard_ids);
        vector<pair<int, int>> default_subchunks;
        default_subchunks.push_back(make_pair(0, _sub_chunk_no));  //表示子块范围
        for (auto &&id : minimum_shard_ids) {
            minimum.insert(make_pair(id, default_subchunks));
        }
        if(CLAY_DEBUG)
            cout << "Clay::rs_minimum_to_decode end" << endl;
}

void Clay::rs1_minimum_to_decode(vector<int> want_to_read,
        vector<int> available,                                                
        set<int> *minimum){
    if (includes(available.begin(), available.end(),want_to_read.begin(), want_to_read.end())) {
        set<int> tmp(want_to_read.begin(), want_to_read.end());
        *minimum = tmp;    //最小必需即为想要读取的
  } else {
    unsigned int k = _k;
    // if (available.size() < (unsigned)k)
    //   return -EIO;
    vector<int>::iterator i;
    unsigned j;
    for (i = available.begin(), j = 0; j < (unsigned)k; ++i, j++)
      minimum->insert(*i);   //前k个可用块
    }
}

//TODO：
void Clay::rs_decode(vector<int> &want_to_read, unordered_map<int, bool>& chunks,unordered_map<int, bool>&repaire,ECDAG* ecdag){
    //rs_decode(want_to_read, chunks, recovered_data,ecdag);//TODO:多节点修复
    vector<int> have;
    have.reserve(chunks.size());

    for (auto  i = chunks.begin();i != chunks.end();++i) {
        have.push_back(i->first);
    }

    if(CLAY_DEBUG){
        cout<<"CLAY::have:";
        for(int i =0;i<have.size();i++){
            cout<<have[i]<<"  ";
        }
        cout<<endl;
    }


    if (includes(have.begin(), have.end(), want_to_read.begin(), want_to_read.end())) {
        for (vector<int>::iterator i = want_to_read.begin(); i != want_to_read.end();++i) {
            // cout << "rs_decode::*i";
            // cout<<(*i)<<endl;
            (repaire)[*i] = chunks.find(*i)->second;
        }
        return;
    }

    for (unsigned int i =  0; i < _k + _m; i++) {
        
        //char* tmpbuf1 = (char*)calloc(chunksize, sizeof(char));
        //repaire.insert(make_pair(i, true));

        if (chunks.find(i) == chunks.end()) {
            // cout<<"not find i in available_chunk    i="<<i<<endl;
            //char* tmpbuf = (char*)calloc(chunksize, sizeof(char));
            repaire.insert(make_pair(i, true));
        } else {
            // cout<<"find i in available_chunk    i="<<i<<endl;
            auto it = chunks.find(i);
            if(it != chunks.end()){
                // cout<<i<<endl;
                //repaire [i]= new char[chunksize];
                // 修复：将数据复制到 repaire
                repaire[i]=it->second;
                //copy(it->second, it->second+sizeof(bool), repaire[i]);
            }else{
                // cout<< "Key " << i << " not found in chunks." << std::endl;
            }

             //(repaire)[i] = chunks.find(i)->second;
            // (repaire)[i].rebuild_aligned(SIMD_ALIGN);
        }
  }

    //TODO
    //decode_chunks(want_to_read, chunks, repaire, _pft_k, _pft_m, _pft_w, chunksize);
    //decode_chunks(want_to_read, chunks, repaire, chunksize);
    
    if(CLAY_DEBUG){
        cout<<"CLAY::repaire:";
        for(auto kv:repaire){
            cout<<kv.first<<"   "<<kv.second<<endl;
        }
        cout<<endl;
    }
    decode_chunks(want_to_read, chunks, repaire, ecdag);
}

void Clay::decode_chunks(vector<int> want_to_read,
        unordered_map<int, bool>& chunks,
        unordered_map<int, bool>& decoded,ECDAG* ecdag){
    //want_to_read 1 2
    vector<int> erasures;
    unordered_map<int, bool> coded_chunks;

    for (int i = 0; i < _k + _m; i++) {
        if (chunks.find(i) == chunks.end()) {
            erasures.push_back(i < _k ? i : i+_nu);
        }
        assert(decoded.find(i) != decoded.end());
        coded_chunks[i < _k ? i : i+_nu] = (decoded)[i];
    }
    //   cout << "code_chunks:" << endl;
    // for (int off=0; off<chunksize; off+=chunksize/_sub_chunk_no) {
    //    for (int i=0; i<_k+_m; i++) {
    //        char* data = coded_chunks[i];
    //        cout << (int)data[off] << " ";
    //    }
    //    cout << endl;
    // }

    for (int i = _k; i < _k+_nu; i++) {
        coded_chunks.insert(make_pair(i, true));
    }
    //ecdag=Encode();/////TODO
    decode_layered(erasures, ecdag);
 }

 void Clay::decode_layered(vector<int>& erased_chunks,ECDAG* ecdag){
       //_encode = false;
    
    // for (int i = 0; i < erased_chunks.size(); i++)
    // {
    //     cout<< erased_chunks[i]<<"   ";
    // }
    // cout<<endl;
    
    int i;
    int x, y;
    int hm_w;
    int z, node_xy, node_sw;

    int num_erasures = erased_chunks.size();
    assert(num_erasures > 0);
    assert(num_erasures == _m);

    Erasure_t** erasures = (Erasure_t**)calloc(num_erasures, sizeof(Erasure_t*));
    int weight_vec[_t];

    // get the (x, y) coordinate of a node_xy
    get_erasure_coordinates(erased_chunks, erasures);
    if (CLAY_DEBUG) {
        cout << "Clay::decode_layered() erasures:" << endl;
        for (int i=0; i<num_erasures; i++) {
            cout << " ";
            erasures[i]->dump();
        }
    }

    // get the number of failures in each y
    get_weight_vector(erasures, weight_vec);

    if (CLAY_DEBUG) {
        cout << "Clay::decode_layered() weight_vector: ";
        for (int i=0; i<_t; i++) cout << weight_vec[i] << " ";
        cout << endl;
    }

    // get the number of non-zero bit in the weight vector
    int max_weight = get_hamming_weight(weight_vec);
    if (CLAY_DEBUG)
        cout << "Clay::decode_layered() max_weight: " << max_weight << endl;

    // the number of unpaired symbols in each plane
    int order[_sub_chunk_no];
    int z_vec[_t];
    
    // in each plane, get the number of symbols that are unpaired from the node that we should
    // encode
    set_planes_sequential_decoding_order(order, erasures);

    if (CLAY_DEBUG) {
        cout << "Clay::decode_layer() order: ";
        for (int j=0; j<_sub_chunk_no; j++) cout << order[j] << " ";
        cout << endl;
    }

    for (hm_w = 0; hm_w <= max_weight; hm_w++) {
        for (z = 0; z < _sub_chunk_no; z++) {
            if (order[z]==hm_w) {
                // for available nodes, we get uncoupled data from coupled data
                // only used in encode_chunks
                if(CLAY_DEBUG)
                    cout<<"(z,hm_w):("<<z<<","<<hm_w<<")"<<endl;
                decode_erasures(erased_chunks, z, ecdag);
            }
        }

        // from uncoupled to coupled data
        for (z=0; z<_sub_chunk_no; z++) {
            if (order[z] == hm_w) {
                get_plane_vector(z,z_vec);
                for (i = 0; i<num_erasures; i++) {
                    x = erasures[i]->_x;
                    y = erasures[i]->_y;
                    node_xy = y*_q+x;
                    node_sw = y*_q+z_vec[y];
                    if(CLAY_DEBUG)
                        cout << "    x: " << x << ", y: " << y << ", node_xy: " << node_xy << ", node_sw: " << node_sw << endl;


                    if (z_vec[y] == x) {
                        int uncoupled_idx = node_xy * _w + z + (_n+_nu) * _w;
                        int coupled_idx = node_xy * _w + z;
                        //ecdag->Join(coupled_idx, {uncoupled_idx}, {1});
                        
                        int real_coupled_idx = get_real_from_short(coupled_idx);
                        int real_uncoupled_idx = get_real_from_short(uncoupled_idx);

                        //cout << "277::old: Join(" << coupled_idx << "; [" << uncoupled_idx << "])" << endl;
//                      // XL comment starts
                        //ecdag->Join(real_coupled_idx, {real_uncoupled_idx}, {1});
                        // XL comment ends
                    
                    } else if (z_vec[y] != x) {
                        if (is_erasure_type_1(i, erasures, z_vec)) {//TODO
                                if(CLAY_DEBUG)
                                    cout<<"is_erasure_type_1"<<endl;
                             //recover_type1_erasure(x, y, z, z_vec, ecdag);//TODO
                                if(x>z_vec[y]) get_coupled0_from_pair12(x, y, z, z_vec, ecdag);
                                else get_coupled1_from_pair03(x, y, z, z_vec, ecdag);
                            }
                                 else{
                                   if(z_vec[y]<x){
                                        get_coupled_from_uncoupled(x, y, z, z_vec, ecdag);
                                    }                                   
                                 }

                    //     } else {
                    //         cout << "Clay::decode_layer() not type1" << endl;
                    //         assert (find(erased_chunks.begin(), erased_chunks.end(), node_sw) != erased_chunks.end());
                    //         if (z_vec[y] < x) {
                    //             cout<<"z_vec[y]<x"<<endl;
                    //             get_coupled_from_uncoupled(x, y, z, z_vec, ecdag);
                    //         }else{
                    //             cout << "z_vec[y]!<x   node_sw:"<<node_sw<<endl;
                    //         }
                    // }
                        //get_coupled1_from_pair02(x, y, z, z_vec, ecdag);
                        //get_coupled_from_uncoupled(x, y, z, z_vec, ecdag);
                    // }else if(z_vec[y]>x){
                    //     cout<<"get_coupled_from_uncoupled ::  z_vec[y]>x"<<endl;

                       
                   }
                }
            }
        }
    }

 }

 int Clay::is_erasure_type_1(int ind, Erasure_t** erasures, int* z_vec) {
  int i;
  if (erasures[ind]->_x == z_vec[erasures[ind]->_y]) return 0;
  for (i=0; i < _m; i++) {
    if (erasures[i]->_y == erasures[ind]->_y) {
      if (erasures[i]->_x == z_vec[erasures[i]->_y]) {
        return 0;
      }
    }
  }
  return 1;
}


vector<int> Clay::recover_type1_erasure( int x, int y, int z,int* z_vec, ECDAG* ecdag) {
    if(CLAY_DEBUG)
        cout << "Clay::recovery_type1_erasure" << endl;
  vector<int> erased_chunks;


  int node_xy = y*_q+x; 
  int node_sw = y*_q+z_vec[y];
  int z_sw = z + (x - z_vec[y]) * pow_int(_q,_t-1-y);
    if(CLAY_DEBUG)
        cout << "      node_xy: " <<node_xy << ", z: " << z << ", node_sw: " << node_sw << ", z_sw: " << z_sw << endl;

                    //   if (z_vec[y] < x) {
                    //     if (node_sw == lost_chunk) {
                    //         vector<int> tmplist = get_coupled1_from_pair02(x, y, z, z_vec, ecdag);
                    //     }
                    // } else if (z_vec[y] > x) {
                    //     if (node_sw == lost_chunk) {
                    //         vector<int> tmplist = get_coupled0_from_pair13(x, y, z, z_vec, ecdag);

                    //     }
                    // }



                    

}
